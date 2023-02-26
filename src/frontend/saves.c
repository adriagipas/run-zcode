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
 *  saves.c - Implementació de 'saves.h'.
 *
 */


#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "saves.h"
#include "utils/log.h"
#include "utils/error.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
remove_undo_fn (
                Saves *s
                )
{

  assert ( s->_undo_fn != NULL );
  
  if ( s->_verbose )
    ii ( "Removing undo save file: '%s'", s->_undo_fn );
  errno= 0;
  if ( remove ( s->_undo_fn ) != 0 )
    ww ( "Failed to remove '%s': %s",
         s->_undo_fn, errno!=0 ? strerror ( errno ) : "unknown error" );
  g_free ( s->_undo_fn );
  s->_undo_fn= NULL;
  
} // end remove_undo_fn




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
saves_free (
            Saves *s
            )
{

  if ( s->_undo_fn )
    {
      remove_undo_fn ( s );
      g_free ( s->_undo_fn );
    }
  g_free ( s );
  
} // end saves_free


Saves *
saves_new (
           const gboolean verbose
           )
{

  Saves *ret;

  
  // Prepara.
  ret= g_new ( Saves, 1 );
  ret->_verbose= verbose;
  ret->_undo_fn= NULL;
  
  return ret;
  
} // end saves_new


const gchar *
saves_get_new_undo_file_name (
                              Saves  *s,
                              char  **err
                              )
{

  GString *buffer;
  GDateTime *dt;
  gchar *time_str;
  

  // Prepara.
  buffer= NULL;
  dt= NULL;
  time_str= NULL;

  // Crea el nom del fitxer.
  buffer= g_string_new ( NULL );
  dt= g_date_time_new_now_local ();
  if ( dt == NULL )
    {
      msgerror ( err,
                 "Failed to build undo file name. Error occurred while"
                 " calling g_date_time_new_now_local" );
      goto error;
    }
  time_str= g_date_time_format_iso8601 ( dt );
  g_date_time_unref ( dt ); dt= NULL;
  g_string_printf ( buffer, "%s/run_zcode-undo-%d-%s.sav",
                    g_get_tmp_dir (),
                    getpid (),
                    time_str );
  g_free ( time_str ); time_str= NULL;

  // Torna el nom del fitxer.
  if ( s->_undo_fn != NULL ) remove_undo_fn ( s );
  s->_undo_fn= buffer->str;
  g_string_free ( buffer, FALSE );

  return s->_undo_fn;
  
 error:
  if ( buffer != NULL ) g_string_free ( buffer, TRUE );
  if ( dt != NULL ) g_date_time_unref ( dt );
  g_free ( time_str );
  return NULL;
  
} // end saves_get_new_undo_file_name


const gchar *
saves_get_undo_file_name (
                          Saves  *s
                          )
{
  return s->_undo_fn;
} // end saves_get_undo_file_name
