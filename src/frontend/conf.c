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

#define GROUP_FONTS "Fonts"

#define DIRNAME "runzcode"

#define DEFAULT_FONT_SIZE          12
#define DEFAULT_FONT_NORMAL_ROMAN  "sans"
#define DEFAULT_FONT_NORMAL_BOLD   "sans:style=bold"
#define DEFAULT_FONT_NORMAL_ITALIC "sans:style=oblique"
#define DEFAULT_FONT_FPITCH_ROMAN  "mono"
#define DEFAULT_FONT_FPITCH_BOLD   "mono:style=bold"
#define DEFAULT_FONT_FPITCH_ITALIC "mono:style=oblique"

#define MIN_FONT_SIZE 8
#define MAX_FONT_SIZE 64




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

  conf->font_size= DEFAULT_FONT_SIZE;
  conf->font_normal_roman= g_strdup ( DEFAULT_FONT_NORMAL_ROMAN );
  conf->font_normal_bold= g_strdup ( DEFAULT_FONT_NORMAL_BOLD );
  conf->font_normal_italic= g_strdup ( DEFAULT_FONT_NORMAL_ITALIC );
  conf->font_fpitch_roman= g_strdup ( DEFAULT_FONT_FPITCH_ROMAN );
  conf->font_fpitch_bold= g_strdup ( DEFAULT_FONT_FPITCH_BOLD );
  conf->font_fpitch_italic= g_strdup ( DEFAULT_FONT_FPITCH_ITALIC );
  
} // end set_default_values


static void
read_string (
             GKeyFile     *f,
             const gchar  *group,
             const gchar  *key,
             gchar       **dst
             )
{

  gchar *val;


  val= g_key_file_get_string ( f, group, key, NULL );
  if ( val != NULL )
    {
      g_free ( *dst );
      *dst= val;
    }
  
} // end read_string


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
  
  // Fonts.
  val_i= g_key_file_get_integer ( f, GROUP_FONTS, "size", NULL );
  if ( val_i < MIN_FONT_SIZE || val_i > MAX_FONT_SIZE )
    {
      ww ( "Invalid font-size %d. Using default font-size", val_i );
      val_i= DEFAULT_FONT_SIZE;
    }
  conf->font_size= val_i;
  read_string ( f, GROUP_FONTS, "normal-roman",
                &(conf->font_normal_roman) );
  read_string ( f, GROUP_FONTS, "normal-bold",
                &(conf->font_normal_bold) );
  read_string ( f, GROUP_FONTS, "normal-italic",
                &(conf->font_normal_italic) );
  read_string ( f, GROUP_FONTS, "fpitch-roman",
                &(conf->font_fpitch_roman) );
  read_string ( f, GROUP_FONTS, "fpitch-bold",
                &(conf->font_fpitch_bold) );
  read_string ( f, GROUP_FONTS, "fpitch-italic",
                &(conf->font_fpitch_italic) );
  
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

  g_free ( conf->font_normal_roman );
  g_free ( conf->font_normal_bold );
  g_free ( conf->font_normal_italic );
  g_free ( conf->font_fpitch_roman );
  g_free ( conf->font_fpitch_bold );
  g_free ( conf->font_fpitch_italic );
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
  ret->font_normal_roman= NULL;
  ret->font_normal_bold= NULL;
  ret->font_normal_italic= NULL;
  ret->font_fpitch_roman= NULL;
  ret->font_fpitch_bold= NULL;
  ret->font_fpitch_italic= NULL;
  
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

  // Fonts
  g_key_file_set_integer ( f, GROUP_FONTS, "size", conf->font_size );
  g_key_file_set_string ( f, GROUP_FONTS, "normal-roman",
                          conf->font_normal_roman );
  g_key_file_set_string ( f, GROUP_FONTS, "normal-bold",
                          conf->font_normal_bold );
  g_key_file_set_string ( f, GROUP_FONTS, "normal-italic",
                          conf->font_normal_italic );
  g_key_file_set_string ( f, GROUP_FONTS, "fpitch-roman",
                          conf->font_fpitch_roman );
  g_key_file_set_string ( f, GROUP_FONTS, "fpitch-bold",
                          conf->font_fpitch_bold );
  g_key_file_set_string ( f, GROUP_FONTS, "fpitch-italic",
                          conf->font_fpitch_italic );
  
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
