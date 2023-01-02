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

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "story_file.h"
#include "utils/error.h"




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




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
story_file_free (
                 StoryFile *sf
                 )
{

  uint32_t n;

  
  if ( sf->fres != NULL ) fclose ( sf->fres );
  if ( sf->data != NULL ) free ( sf->data );
  if ( sf->resources != NULL )
    {
      for ( n= 0; n < sf->Nres; ++n )
        if ( sf->resources[n].desc != NULL )
          free ( sf->resources[n].desc );
      free ( sf->resources );
    }
  free ( sf );
  
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
    {
      fprintf ( stderr, "INTENTA BLORB!!!\n" );
      exit ( EXIT_FAILURE );
    }
  else
    ret= new_from_zfile ( file_name, err );
  
  return ret;
  
} // end story_file_new_from_file_name
