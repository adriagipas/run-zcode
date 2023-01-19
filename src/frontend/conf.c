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
 *  conf.c - Implementació de 'conf.h'.
 *
 */


#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define GROUP "main"

#define DIRNAME "runzcode"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static gchar *
make_dir (
          char **err
          )
{

  gchar *ret;
  
  
  ret= g_build_path ( G_DIR_SEPARATOR_S,
                      g_get_user_config_dir (),
                      DIRNAME, NULL );
  errno= 0;
  if ( g_mkdir_with_parents ( ret, 0755 ) == -1 )
    {
      msgerror ( err, "Failed to create directory '%s': %s",
                 ret, errno==0 ? "unknown" : strerror ( errno ) );
      g_free ( ret );
      return NULL;
    }

  return ret;
  
} // end make_dir


static bool
build_file_name (
                 Conf  *conf,
                 char **err
                 )
{

  gchar *dir;
  

  dir= make_dir ( err );
  if ( dir == NULL ) return false;
  conf->_file_name= g_build_filename ( dir, "conf.cfg", NULL );
  g_free ( dir );
  
  return true;
  
} // end build_file_name


static void
set_default_values (
                    Conf *conf
                    )
{

  conf->font_size= 12;
  
} // end set_default_values


static bool
read_conf (
           Conf  *conf,
           char **err
           )
{

  GKeyFile *f;
  gboolean ret;
  GError *gerr;
  gint val_i;

  // Prepara.
  f= NULL;
  gerr= NULL;
  if ( conf->_verbose )
    ii ( "Reading configuration file: %s", conf->_file_name );

  // Obri
  f= g_key_file_new ();
  gerr= NULL;
  ret= g_key_file_load_from_file ( f, conf->_file_name,
                                   G_KEY_FILE_NONE, &gerr );
  if ( !ret ) goto error;
  
  // Llig valors.
  // --> Font size
  val_i= g_key_file_get_integer ( f, GROUP, "font-size", NULL );
  if ( val_i < 8 || val_i > 64 )
    {
      ww ( "Invalid font-size %d. Using default font-size", val_i );
      val_i= 12;
    }
  conf->font_size= val_i;
  
  // Allibera.
  g_key_file_free ( f );
  
  return true;
  
 error:
  if ( f != NULL ) g_key_file_free ( f );
  msgerror ( err, "Failed to read configuration file: %s", gerr->message );
  g_error_free ( gerr );
  return false;
  
} // end read_conf




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
conf_free (
           Conf *conf
           )
{

  g_free ( conf->_file_name );
  g_free ( conf );
  
} // end conf_free


Conf *
conf_new (
          const gboolean   verbose,
          const gchar     *file_name, // Pot ser NULL
          char           **err
          )
{
  
  Conf *ret;
  
  
  // Preparació
  ret= g_new ( Conf, 1 );
  ret->_verbose= verbose;
  ret->_file_name= NULL;

  // Obté el nom
  if ( file_name != NULL )
    ret->_file_name= g_strdup ( file_name );
  else
    {
      if ( !build_file_name ( ret, err ) ) goto error;
    }
  
  // Inicialitza i si cal llig.
  set_default_values ( ret );
  if ( g_file_test ( ret->_file_name, G_FILE_TEST_IS_REGULAR ) )
    {
      if ( !read_conf ( ret, err ) ) goto error;
    }
  
  return ret;
  
 error:
  conf_free ( ret );
  return NULL;
  
} // end conf_new


bool
conf_write (
            Conf  *conf,
            char **err
            )
{

  GKeyFile *f;
  GError *gerr;
  gboolean ret;
  
  
  // Inicialitza.
  if ( conf->_verbose )
    ii ( "Writing configuration file: '%s'", conf->_file_name );
  f= g_key_file_new ();

  // Font size
  g_key_file_set_integer ( f, GROUP, "font-size", conf->font_size );

  // Escriu
  gerr= NULL;
  ret= g_key_file_save_to_file ( f, conf->_file_name, &gerr );
  g_key_file_free ( f );
  if ( !ret )
    {
      msgerror ( err, "Failed to write configuration file: %s", gerr->message );
      g_error_free ( gerr );
      return false;
    }
  
  return true;
  
} // end conf_write
