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
 *  story_file.c - Implementació de 'story_file.h'.
 *
 */

#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iff.h"
#include "story_file.h"
#include "utils/error.h"
#include "utils/log.h"




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


static bool
check_data (
            StoryFile   *sf,
            const char  *file_name,
            char       **err
            )
{

  uint8_t version;


  version= sf->data[0];

  // Grandària capçalera.
  if ( sf->size < 64 )
    {
      msgerror ( err, "Unable to read the story file header from file: %s",
                 file_name );
      return false;
    }
  
  // V1/2/3 - 128KB
  if ( version == 1 || version == 2 || version == 3 )
    {
      if ( sf->size > 128*1024 )
        {
          msgerror ( err,
                     "Story file (version:%d) size (%lu B) exceeds the maximum"
                     " size allowed (128 KB): %s",
                     version, sf->size, file_name );
          return false;
        }
    }

  // V4/5 - 256KB
  else if ( version == 4 || version == 5 )
    {
      if ( sf->size > 256*1024 )
        {
          msgerror ( err,
                     "Story file (version:%d) size (%lu B) exceeds the maximum"
                     " size allowed (256 KB): %s",
                     version, sf->size, file_name );
          return false;
        }
    }

  // V6/7/8 - 512KB
  else if ( version == 6 || version == 7 || version == 8 )
    {
      if ( sf->size > 512*1024 )
        {
          msgerror ( err,
                     "Story file (version:%d) size (%lu B) exceeds the maximum"
                     " size allowed (512 KB): %s",
                     version, sf->size, file_name );
          return false;
        }
    }

  // Versió desconeguda
  else
    {
      msgerror ( err, "Unsupported Version Number %d: %s", version, file_name );
      return false;
    }
  
  return true;
  
} // end check_data


static StoryFile *
new_from_zfile (
                const char  *file_name,
                char       **err
                )
{

  StoryFile *ret;
  FILE *f;
  long size;
  

  // Reserva i prepara.
  ret= g_new ( StoryFile, 1 );
  ret->data= NULL;
  ret->resources= NULL;
  ret->Nres= 0;
  ret->fres= NULL;
  ret->frontispiece= ret->Nres;
  ret->raw_metadata= NULL;
  f= NULL;

  // Llig contingut.
  f= fopen ( file_name, "rb" );
  if ( f == NULL ) { error_open_file ( err, file_name ); goto error; }
  if ( fseek ( f, 0, SEEK_END ) == -1 )
    { error_read_file ( err, file_name ); goto error; }
  size= ftell ( f );
  if ( size == -1 ) { error_read_file ( err, file_name ); goto error; }
  if ( size == 0 )
    {
      msgerror ( err, "Failed to read from empty file: %s", file_name );
      goto error;
    }
  ret->size= (size_t) size;
  if ( ret->size != size )
    {
      msgerror ( err, "Failed to read from file [file is too large %ld]: %s",
                 size, file_name );
      goto error;
    }
  rewind ( f );
  ret->data= g_new ( uint8_t, ret->size );
  if ( fread ( ret->data, ret->size, 1, f ) != 1 )
    { error_read_file ( err, file_name ); goto error; }
  fclose ( f ); f= NULL;
  
  // Comprovacions bàsiques.
  if ( !check_data ( ret, file_name, err ) )
    goto error;
  
  return ret;
  
 error:
  if ( f != NULL ) fclose ( f );
  story_file_free ( ret );
  return NULL;
  
} // end new_from_zfile


// NOTA!! En aquest punt sf->fres està apuntant a la següent entrada en RIdx.
static bool
read_resource (
               StoryFile       *sf,
               const uint32_t   n,
               const IFF       *iff,
               const char      *file_name,
               char           **err
               )
{

  char buf[4],usage[4];
  uint32_t num,start,i;
  long entry_offset;
  const IFFChunk *chunk;
  
  
  // Llig usage.
  if ( fread ( usage, sizeof(usage), 1, sf->fres ) != 1 )
    {
      msgerror ( err, "Unable to read RIdx[%u].usage: %s",
                 n, file_name );
      return false;
    }

  // Llig number i comprova
  if ( fread ( buf, sizeof(buf), 1, sf->fres ) != 1 )
    {
      msgerror ( err, "Unable to read RIdx[%u].number: %s",
                 n, file_name );
      return false;
    }
  num= BUF_TO_U32(buf);
  if ( num >= sf->Nres )
    {
      msgerror ( err, "RIdx[%u].number==%u is out of range [0,%u]: %s",
                 n, num, sf->Nres-1, file_name );
      return false;
    }
  if ( sf->resources[num].type != STORY_FILE_RESOURCE_NONE )
    {
      msgerror ( err, "RIdx[%u].number==%u already exists: %s",
                 n, num, file_name );
      return false;
    }

  // Llig start
  if ( fread ( buf, sizeof(buf), 1, sf->fres ) != 1 )
    {
      msgerror ( err, "Unable to read RIdx[%u].start: %s",
                 n, file_name );
      return false;
    }
  start= BUF_TO_U32(buf);

  // Cerca chunk que coincidisca amb start.
  entry_offset= (long) ((int64_t) ((uint64_t) start));
  for ( i= 0;
        i < iff->N && iff->chunks[i].offset != entry_offset;
        ++i );
  if ( i == iff->N )
    {
      msgerror ( err, "RIdx[%u] chunk not found: %s", n, file_name );
      return false;
    }
  chunk= &(iff->chunks[i]);

  // Emplena camps resource.
  sf->resources[num].offset= chunk->offset+8; // Ignora capçalera
  sf->resources[num].size= (size_t) chunk->length;
  if ( strncmp ( usage, "Pict", 4 ) == 0 )
    {
      if ( strcmp ( chunk->type, "PNG " ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_PICTURE_PNG;
      else if ( strcmp ( chunk->type, "JPEG" ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_PICTURE_JPEG;
      else if ( strcmp ( chunk->type, "Rect" ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_PICTURE_PLACEHOLDER;
      else
        {
          msgerror ( err, "RIdx[%u] references to an unsupported picture"
                     " resource chunk '%s': %s", n, chunk->type, file_name );
          return false;
        }
    }
  else if ( strncmp ( usage, "Snd", 4 ) == 0 )
    {
      if ( strcmp ( chunk->type, "FORM" ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_SOUND_AIFF; // ¿¿??
      else if ( strcmp ( chunk->type, "OGGV" ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_SOUND_OGG;
      else if ( strcmp ( chunk->type, "MOD " ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_SOUND_MOD;
      else
        {
          msgerror ( err, "RIdx[%u] references to an unsupported sound"
                     " resource chunk '%s': %s", n, chunk->type, file_name );
          return false;
        }
    }
  else if ( strncmp ( usage, "Data", 4 ) == 0 )
    {
      msgerror ( err, "RIdx[%u] references to an unsupported data"
                 " resource chunk '%s': %s", n, chunk->type, file_name );
      return false;
    }
  else if ( strncmp ( usage, "Exec", 4 ) == 0 )
    {
      if ( strcmp ( chunk->type, "ZCOD" ) == 0 )
        sf->resources[num].type= STORY_FILE_RESOURCE_ZCODE;
      else
        {
          msgerror ( err, "RIdx[%u] references to an unsupported executable"
                     " resource chunk '%s': %s", n, chunk->type, file_name );
          return false;
        }
    }
  else
    {
      msgerror ( err, "Unknown usage RIdx[%u].usage=='%c%c%c%c': %s",
                 n, usage[0], usage[1], usage[2], usage[3], file_name );
      return false;
    }
  
  return true;
  
} // end read_resource


static bool
init_resources (
                StoryFile   *sf,
                const IFF   *iff,
                const char  *file_name,
                char       **err
                )
{

  char buf[4];
  uint32_t n;
  
  
  // Comprova RIdx
  if ( iff->N == 0 || strcmp ( iff->chunks[0].type, "RIdx" ) != 0 )
    {
      msgerror ( err, "RIdx chunk not found: %s", file_name );
      return false;
    }
  
  // Obri fitxer resources
  sf->fres= fopen ( file_name, "rb" );
  if ( sf->fres == NULL )
    { error_read_file ( err, file_name ); return false; }
  if ( fseek ( sf->fres, iff->chunks[0].offset+8, SEEK_SET ) != 0 )
    { error_read_file ( err, file_name ); return false; }
  
  // Llig num entrades i inicialitza resources
  if ( fread ( buf, sizeof(buf), 1, sf->fres ) != 1 )
    { error_read_file ( err, file_name ); return false; }
  sf->Nres= BUF_TO_U32(buf);
  if ( (sf->Nres*12 + 4) != iff->chunks[0].length )
    {
      msgerror ( err,
                 "RIdx length (%lu) does not match with"
                 " number of resource entries (%u): %s",
                 iff->chunks[0].length, sf->Nres, file_name );
      return false;
    }
  if ( sf->Nres == 0 )
    {
      msgerror ( err, "Not resources found: %s", file_name );
      return false;
    }
  sf->resources= g_new ( StoryFileResource, sf->Nres );
  for ( n= 0; n < sf->Nres; ++n )
    {
      sf->resources[n].type= STORY_FILE_RESOURCE_NONE;
      sf->resources[n].desc= NULL;
    }
  
  // Llig resources
  for ( n= 0; n < sf->Nres; ++n )
    if ( !read_resource ( sf, n, iff, file_name, err ) )
      return false;

  // Comprova que tots els resources estiguen assignats.
  for ( n= 0; n < sf->Nres; ++n )
    if ( sf->resources[n].type == STORY_FILE_RESOURCE_NONE )
      {
        msgerror ( err, "Found unused resource entries: %s", file_name );
        return false;
      }
  
  return true;
  
} // end init_resources


// Assumeix que la taula de resources ja està inicialitzada.
static bool
load_zcode_chunk (
                  StoryFile   *sf,
                  const char  *file_name,
                  char       **err
                  )
{

  uint32_t n;
  
  
  // Cerca codi.
  for ( n= 0;
        n < sf->Nres && sf->resources[n].type != STORY_FILE_RESOURCE_ZCODE;
        ++n );
  if ( n == sf->Nres )
    {
      msgerror ( err, "No ZCode found: %s", file_name );
      return true;
    }

  // Carrega
  sf->data= g_new ( uint8_t, sf->resources[n].size );
  sf->size= sf->resources[n].size;
  if ( fseek ( sf->fres, sf->resources[n].offset, SEEK_SET ) != 0 )
    { error_read_file ( err, file_name ); return false; }
  if ( fread ( sf->data, sf->size, 1, sf->fres ) != 1 )
    { error_read_file ( err, file_name ); return false; }

  // Comprovacions bàsiques.
  if ( !check_data ( sf, file_name, err ) )
    return false;
  
  return true;
  
} // end load_zcode_chunk


static bool
load_chunk (
            StoryFile       *sf,
            const IFFChunk  *chunk,
            uint8_t         *mem,
            const char      *file_name,
            char           **err
            )
{

  if ( fseek ( sf->fres, chunk->offset+8, SEEK_SET ) != 0 )
    goto error;
  if ( fread ( mem, (size_t) chunk->length, 1, sf->fres ) != 1 )
    goto error;

  return true;
  
 error:
  msgerror ( err,
             "Failed to read chunk (type:'%s',offset:%ld,length:%u): %s",
             chunk->type, chunk->offset, chunk->length, file_name );
  return false;
  
} // end load_chunk


static bool
load_optional_chunks (
                      StoryFile   *sf,
                      const IFF   *iff,
                      const char  *file_name,
                      char       **err
                      )
{

  size_t n;
  const IFFChunk *chunk;
  char buf[4];
  uint32_t tmp32;
  

  for ( n= 0; n < iff->N; ++n )
    {
      
      chunk= &(iff->chunks[n]);
      
      if ( strcmp ( chunk->type, "IFhd" ) == 0 )
        ww ( "Support for Game Identifier chunk not implemented" );
      else if ( strcmp ( chunk->type, "Plte" ) == 0 )
        ww ( "Support for Color Palette chunk not implemented" );

      // Frontispiece chunk
      else if ( strcmp ( chunk->type, "Fspc" ) == 0 )
        {
          if ( !load_chunk ( sf, chunk, (uint8_t *) &buf, file_name, err ) )
            return false;
          tmp32= BUF_TO_U32(buf);
          if ( tmp32 >= sf->Nres ||
               (sf->resources[tmp32].type != STORY_FILE_RESOURCE_PICTURE_PNG &&
                sf->resources[tmp32].type != STORY_FILE_RESOURCE_PICTURE_JPEG &&
                sf->resources[tmp32].type !=
                STORY_FILE_RESOURCE_PICTURE_PLACEHOLDER) )
            ww ( "Invalid frontispiece identifier %u: %s",
                 tmp32, file_name );
          else
            sf->frontispiece= tmp32;
        }
      
      else if ( strcmp ( chunk->type, "RDes" ) == 0 )
        ww ( "Support for Resource Description chunk not implemented" );

      //  Metadata chunk
      else if ( strcmp ( chunk->type, "IFmd" ) == 0 )
        {
          sf->raw_metadata= g_new ( char, chunk->length+1 );
          if ( !load_chunk ( sf, chunk, (uint8_t *) sf->raw_metadata,
                             file_name, err ) )
            return false;
          sf->raw_metadata[chunk->length]= '\0';
        }
      
      else if ( strcmp ( chunk->type, "RelN" ) == 0 )
        ww ( "Support for Release Number chunk not implemented" );
      else if ( strcmp ( chunk->type, "Reso" ) == 0 )
        ww ( "Support for Resolution chunk not implemented" );
      else if ( strcmp ( chunk->type, "APal" ) == 0 )
        ww ( "Support for Adaptive Palette chunk not implemented" );
      else if ( strcmp ( chunk->type, "Loop" ) == 0 )
        ww ( "Support for Looping chunk not implemented" );
      else if ( strcmp ( chunk->type, "AUTH" ) == 0 )
        ww ( "Support for 'AUTH' chunk not implemented" );
      else if ( strcmp ( chunk->type, "(c) " ) == 0 )
        ww ( "Support for '(c) ' chunk not implemented" );
      else if ( strcmp ( chunk->type, "ANNO" ) == 0 )
        ww ( "Support for 'ANNO' chunk not implemented" );
      else if ( strcmp ( chunk->type, "SNam" ) == 0 )
        ww ( "Support for Story Name chunk not implemented" );
      
    }
  
  return true;
  
} // end load_optional_chunks


static StoryFile *
new_from_blorb (
                const char  *file_name,
                char       **err
                )
{

  StoryFile *ret;
  IFF *iff;


  // Inicialitza i prepara.
  ret= g_new ( StoryFile, 1 );
  ret->data= NULL;
  ret->resources= NULL;
  ret->Nres= 0;
  ret->fres= NULL;
  ret->frontispiece= UINT32_MAX;
  ret->raw_metadata= NULL;
  iff= NULL;

  // Llig estructura IFF
  iff= iff_new_from_file_name ( file_name, err );
  if ( iff == NULL )
    goto error;
  if ( strcmp ( iff->type, "IFRS" ) != 0 )
    {
      msgerror ( err, "Unknown FORM type '%s': %s",
                 iff->type, file_name );
      goto error;
    }

  // Inicialitza resources
  if ( !init_resources ( ret, iff, file_name, err ) )
    goto error;

  // Carrega el zcode
  if ( !load_zcode_chunk ( ret, file_name, err ) )
    goto error;

  // Optional chunks
  if ( !load_optional_chunks ( ret, iff, file_name, err ) )
    goto error;
  
  // Allibera recursos
  iff_free ( iff );
  
  return ret;
  
 error:
  if ( iff != NULL ) iff_free ( iff );
  story_file_free ( ret );
  return NULL;
  
} // end new_from_blorb




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
story_file_free (
                 StoryFile *sf
                 )
{

  uint32_t n;


  if ( sf->raw_metadata != NULL ) g_free ( sf->raw_metadata );
  if ( sf->fres != NULL ) fclose ( sf->fres );
  if ( sf->data != NULL ) g_free ( sf->data );
  if ( sf->resources != NULL )
    {
      for ( n= 0; n < sf->Nres; ++n )
        if ( sf->resources[n].desc != NULL )
          g_free ( sf->resources[n].desc );
      g_free ( sf->resources );
    }
  g_free ( sf );
  
} // end story_file_free


StoryFile *
story_file_new_from_file_name (
                               const char  *file_name,
                               char       **err
                               )
{

  StoryFile *ret;
  FILE *f;
  char buf[4];
  

  // Llig capçalera
  f= fopen ( file_name, "rb" );
  if ( f == NULL )
    {
      error_open_file ( err, file_name );
      return NULL;
    }
  if ( fread ( buf, sizeof(buf), 1, f ) != 1 )
    {
      error_read_file ( err, file_name );
      fclose ( f );
      return NULL;
    }
  fclose ( f );

  // Comprova el tipus
  if ( strncmp ( buf, "FORM", 4 ) == 0 )
    ret= new_from_blorb ( file_name, err );
  else
    ret= new_from_zfile ( file_name, err );
  
  return ret;
  
} // end story_file_new_from_file_name


bool
story_file_read_resource (
                          StoryFile       *sf,
                          const uint32_t   resource,
                          uint8_t         *mem,
                          char           **err
                          )
{

  assert ( resource < sf->Nres );

  if ( fseek ( sf->fres, sf->resources[resource].offset, SEEK_SET ) != 0 )
    goto error;
  if ( fread ( mem, sf->resources[resource].size, 1, sf->fres ) != 1 )
    goto error;
  
  return true;

 error:
  msgerror ( err, "Failed to read resource %u", resource );
  return false;
  
} // end story_file_read_resource
