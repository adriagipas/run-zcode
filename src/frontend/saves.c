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
#include <gio/gio.h>
#include <glib.h>
#include <libintl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "saves.h"
#include "utils/log.h"
#include "utils/error.h"




/**********/
/* MACROS */
/**********/

#define _(String) gettext (String)

#define NSLOTS 5

#define TIME_SLEEP 10000




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
remove_undo_fn (
                Saves     *s,
                const int  n
                )
{

  assert ( s->_undo_fn[n] != NULL );
  
  if ( s->_verbose )
    ii ( "Removing undo save file: '%s'", s->_undo_fn[n] );
  errno= 0;
  if ( remove ( s->_undo_fn[n] ) != 0 )
    ww ( "Failed to remove '%s': %s",
         s->_undo_fn[n], errno!=0 ? strerror ( errno ) : "unknown error" );
  g_free ( s->_undo_fn[n] );
  s->_undo_fn[n]= NULL;
  
} // end remove_undo_fn


// Obté el nom del fitxer. S'ha d'esborrar
static gchar *
get_save_slot_name (
                    Saves      *s,
                    const char *name,
                    const int   num
                    )
{

  GString *buffer;
  gchar *ret;
  
  
  // Crea el nom del fitxer.
  buffer= g_string_new ( NULL );
  g_string_printf ( buffer, "%s.slot%d.sav", name, num );

  // Nom que torna.
  ret= g_build_filename ( s->_savedir, buffer->str, NULL );

  // Allibera memòria.
  g_string_free ( buffer, FALSE );
  
  return ret;
  
} // end get_save_slot_name


// Torna la data de modificació del fitxer o NULL si no existeix o no
// és un fitxer regular.
static GDateTime *
get_path_datetime (
                   const gchar *path
                   )
{

  GFile *f;
  GFileInfo *info;
  GDateTime *ret;
  

  // Prepara
  f= NULL;
  info= NULL;

  // Obté
  f= g_file_new_for_path ( path );
  info= g_file_query_info ( f,
                            "access::can-read,access::can-write,"
                            "owner::user,"
                            "time::modified,"
                            "standard::*",
                            G_FILE_QUERY_INFO_NONE,
                            NULL, NULL );
  if ( info == NULL ) goto error;
  if ( g_file_info_get_file_type ( info ) != G_FILE_TYPE_REGULAR )
    goto error;
  if ( g_file_info_get_is_backup ( info ) ||
       g_file_info_get_is_hidden ( info ) ||
       g_file_info_get_is_symlink ( info ) )
    goto error;
  ret= g_file_info_get_modification_date_time ( info );

  // Allibera memòria
  g_object_unref ( info );
  g_object_unref ( f );
  
  return ret;
  
 error:
  if ( info != NULL ) g_object_unref ( info );
  if ( f != NULL ) g_object_unref ( f );
  return NULL;
  
} // end get_path_datetime


static bool
print_save_slots (
                  Saves       *s,
                  Screen      *screen,
                  const char  *id,
                  char       **err
                  )
{

  int i;
  gchar *slot, *date;
  char buf[10];
  GDateTime *dt;
  

  for ( i= 1; i <= NSLOTS; ++i )
    {

      // Número
      sprintf ( buf, " (%d) ", i );
      if ( !screen_print ( screen, buf, err ) )
        return false;

      // Buit o data
      slot= get_save_slot_name ( s, id, i );
      dt= get_path_datetime ( slot  );
      if ( dt == NULL )
        {
          if ( !screen_print ( screen, _("--EMPTY--"), err ) )
            return false;
        }
      else
        {
          date= g_date_time_format ( dt, "\%x (%X)" );
          if ( !screen_print ( screen, date, err ) )
            return false;
          g_free ( date );
          g_date_time_unref ( dt );
        }
      g_free ( slot );

      // Bot de línia
      if ( !screen_print ( screen, "\n", err ) )
        return false;
      
    }
  
  return true;
  
} // end print_save_slots


// Torna -1 per a indicar error, 0 que no s'ha seleccionat res.
static int
select_save_slot (
                  Saves       *s,
                  Screen      *screen,
                  const char  *id,
                  char       **err
                  )
{

  uint8_t buf[SCREEN_INPUT_TEXT_BUF],zc;
  int nread,ret;

  
  if ( !screen_print ( screen, _("Choose a save slot:\n"), err ) )
    return -1;
  if ( !print_save_slots ( s, screen, id, err ) )
    return -1;
  do {
    if ( !screen_read_char ( screen, buf, &nread, err ) )
      return -1;
    g_usleep ( TIME_SLEEP );
  } while ( nread == 0 );
  if ( nread == 1 )
    {
      zc= buf[0];
      if ( zc >= '1' && zc <= ('0'+NSLOTS) )
        ret= zc-'0';
      else if ( zc >= 146 && zc <= (145+NSLOTS) )
        ret= zc-145;
      else ret= 0;
    }
  else ret= 0;
  
  return ret;
  
} // end select_save_slot




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
saves_free (
            Saves *s
            )
{

  int n;


  g_free ( s->_savedir );
  for ( n= 0; n < SAVES_MAX_UNDO; ++n )
    if ( s->_undo_fn[n] != NULL )
      {
        remove_undo_fn ( s, n );
        g_free ( s->_undo_fn[n] );
      }
  g_free ( s );
  
} // end saves_free


Saves *
saves_new (
           const gboolean verbose
           )
{

  Saves *ret;
  int n;
  
  
  // Prepara.
  ret= g_new ( Saves, 1 );
  ret->_verbose= verbose;
  ret->_N_undo= 0;
  ret->_pos= 0;
  for ( n= 0; n < SAVES_MAX_UNDO; ++n )
    ret->_undo_fn[n]= NULL;
  ret->_savedir= NULL;

  // Savedir
  ret->_savedir= g_build_path ( G_DIR_SEPARATOR_S,
                                g_get_user_data_dir (),
                                "run-zcode", "savs", NULL );
  g_mkdir_with_parents ( ret->_savedir, 0755 );
  
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
  int pos;
  
  
  // Comprova queda espai, si no esborra
  if ( s->_N_undo == SAVES_MAX_UNDO )
    {
      remove_undo_fn ( s, s->_pos );
      --(s->_N_undo);
      s->_pos= (s->_pos+1)%SAVES_MAX_UNDO;
    }
  
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
  g_string_printf ( buffer, "%s/run_zcode-undo-%d-%s.sav%d",
                    g_get_tmp_dir (),
                    getpid (),
                    time_str,
                    s->_N_undo );
  g_free ( time_str ); time_str= NULL;

  // Desa el nom del fitxer.
  pos= (s->_pos + s->_N_undo)%SAVES_MAX_UNDO;
  s->_undo_fn[pos]= buffer->str;
  ++(s->_N_undo);
  g_string_free ( buffer, FALSE );
  
  return s->_undo_fn[pos];
  
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

  int pos;

  
  if ( s->_N_undo == 0 )
    return NULL;
  else
    {
      pos= (s->_pos + s->_N_undo - 1)%SAVES_MAX_UNDO;
      return s->_undo_fn[pos];
    }
  
} // end saves_get_undo_file_name


void
saves_remove_last_undo_file_name (
                                  Saves *s
                                  )
{

  int pos;

  
  if ( s->_N_undo > 0 )
    {
      --(s->_N_undo);
      pos= (s->_pos + s->_N_undo)%SAVES_MAX_UNDO;
      remove_undo_fn ( s, pos );
    }
  
} // end saves_remove_undo_file_name


gchar *
saves_get_save_file_name (
                          Saves       *s,
                          Screen      *screen,
                          const char  *id,
                          char       **err
                          )
{

  int slot_num;

  
  slot_num= select_save_slot ( s, screen, id, err );
  if ( slot_num == -1 ) return NULL;
  else if ( slot_num == 0 )
    {
      msgerror ( err, "no save slot selected" );
      return NULL;
    }
  
  return get_save_slot_name ( s, id, slot_num );
  
} // end saves_get_save_file_name
