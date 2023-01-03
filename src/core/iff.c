/*
 * Copyright 2023 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/run-zcode.
 *
 * adriagipas/run-zcode is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/run-zcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/run-zcode.  If not, see
 * <https://www.gnu.org/licenses/>.
 */
/*
 *  iff.c - Implementació de 'iff.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "iff.h"
#include "utils/error.h"




/**********/
/* MACROS */
/**********/

#define BUF_TO_U32(BUF)                          \
  (((uint32_t) ((uint8_t) (BUF)[3])) |           \
   (((uint32_t) ((uint8_t) (BUF)[2]))<<8) |      \
   (((uint32_t) ((uint8_t) (BUF)[1]))<<16) |     \
   (((uint32_t) ((uint8_t) (BUF)[0]))<<24))




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// Llig la capçalera i comprova que tot és correcte.
static bool
read_header (
             FILE        *f,
             const char  *file_name,
             const long   file_size,
             char       **err
             )
{

  char buf[4];
  uint32_t size;

  
  // Magic number
  if ( fread ( buf, sizeof(buf), 1, f ) != 1 )
    {
      msgerror ( err, "Unable to read IFF magic number: %s", file_name );
      return false;
    }
  if ( strncmp ( buf, "FORM", 4 ) != 0 )
    {
      msgerror ( err, "Wrong IFF magic number '%c%c%c%c': %s",
                 buf[0], buf[1], buf[2], buf[3], file_name );
      return false;
    }

  // Grandària
  if ( fread ( buf, sizeof(buf), 1, f ) != 1 )
    {
      msgerror ( err, "Unable to read IFF file size: %s", file_name );
      return false;
    }
  size= BUF_TO_U32 ( buf );
  if ( ((int64_t) ((uint64_t) size)) != (((int64_t) file_size)-8) )
    {
      msgerror ( err, "IFF header file size (%u) and real file size"
                 " (%ld) do not match", size, file_size-8 );
      return false;
    }

  // Form TYPE
  if ( fread ( buf, sizeof(buf), 1, f ) != 1 )
    {
      msgerror ( err, "Unable to read IFF FORM type: %s", file_name );
      return false;
    }
  if ( strncmp ( buf, "IFRS", 4 ) != 0 )
    {
      msgerror ( err, "Unknown FORM type '%c%c%c%c': %s",
                 buf[0], buf[1], buf[2], buf[3], file_name );
      return false;
    }
  
  return true;
  
} // end read_header


static void
resize_chunks (
               IFF    *iff,
               size_t *size_chunks
               )
{

  size_t nsize;


  nsize= (*size_chunks)*2;
  if ( nsize < *size_chunks )
    {
      fprintf ( stderr, "resize_chunks - Cannot allocate memory\n" );
      exit ( EXIT_FAILURE );
    }
  iff->chunks= g_renew ( IFFChunk, iff->chunks, nsize );
  *size_chunks= nsize;
  
} // end resize_chunks


// Torna: 1  - Si ha llegit un chunk
//        0  - Si no ha llegit cap chunk perquè s'ha acabat el fitxer.
//        -1 - En cas d'error.
static int
read_next_chunk (
                 IFF         *iff,
                 size_t      *chunks_size,
                 FILE        *f,
                 const char  *file_name,
                 const long   file_size,
                 char       **err
                 )
{

  long offset,new_offset;
  uint32_t data_size;
  char buf[4];
  
  
  // Offset actual i comprovació EOF.
  offset= ftell ( f );
  if ( offset == -1 ) { error_read_file ( err, file_name ); return -1; }
  if ( offset >= file_size ) return 0;

  // Reajusta
  if ( iff->N == *chunks_size )
    resize_chunks ( iff, chunks_size );
  
  // Llig tipus.
  if ( fread ( iff->chunks[iff->N].type, 4, 1, f ) != 1 )
    {
      msgerror ( err,
                 "Unable to read an expected chunk type: %s",
                 file_name );
      return -1;
    }
  iff->chunks[iff->N].type[4]= '\0';

  // Llig data length.
  if ( fread ( buf, sizeof(buf), 1, f ) != 1 )
    {
      msgerror ( err,
                 "Unable to read data length for current chunk: %s",
                 file_name );
      return -1;
    }
  data_size= BUF_TO_U32(buf);

  // Assigna valors que falten.
  iff->chunks[iff->N].offset= offset;
  iff->chunks[iff->N].length= data_size;
  ++(iff->N);
  
  // Calcula nou offset i avança.
  new_offset= (int64_t) ((uint64_t) data_size);
  if ( new_offset&0x1 )
    {
      new_offset+= 1; // Padding
      if ( new_offset == 0 )
        {
          msgerror ( err,"Chunk too large: %s", file_name );
          return -1;
        }
    }
  new_offset+= offset+8;
  if ( fseek ( f, new_offset, SEEK_SET ) != 0 )
    {
      msgerror ( err,
                 "Current chunk (%u B) does not fit into"
                 " current file size (%ld B): %s",
                 data_size, file_size, file_name );
      return -1;
    }
  
  return 1;
  
} // end read_next_chunk




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
iff_free (
          IFF *iff
          )
{

  if ( iff->chunks != NULL ) free ( iff->chunks );
  free ( iff );
  
} // end iff_free


IFF *
iff_new_from_file_name (
                        const char  *file_name,
                        char       **err
                        )
{

  IFF *ret;
  size_t chunks_size;
  long fsize;
  FILE *f;
  int err_read;
  
  
  // Reserva i prepara.
  ret= g_new ( IFF, 1 );
  chunks_size= 1;
  ret->chunks= g_new ( IFFChunk, chunks_size );
  ret->N= 0;
  f= NULL;

  // Obri i obté grandària.
  f= fopen ( file_name, "rb" );
  if ( f == NULL ) { error_open_file ( err, file_name ); goto error; }
  if ( fseek ( f, 0, SEEK_END ) == -1 )
    { error_read_file ( err, file_name ); goto error; }
  fsize= ftell ( f );
  if ( fsize == -1 ) { error_read_file ( err, file_name ); goto error; }
  rewind ( f );

  // Capçalera
  if ( !read_header ( f, file_name, fsize, err ) )
    goto error;

  // Llig chunks
  while ( (err_read= read_next_chunk ( ret, &chunks_size, f,
                                       file_name, fsize, err )) == 1 );
  if ( err_read == -1 ) goto error;
  if ( ret->N < chunks_size )
    ret->chunks= g_renew ( IFFChunk, ret->chunks, ret->N );
  
  // Tanca
  fclose ( f );
  
  return ret;

 error:
  if ( f != NULL ) fclose ( f );
  iff_free ( ret );
  return NULL;
  
} // end iff_new_from_file_name
