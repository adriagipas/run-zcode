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
 *  fonts.c - Implementació de 'fonts.h'.
 *
 */


#include <fontconfig/fontconfig.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL_ttf.h>

#include "fonts.h"
#include "utils/error.h"
#include "utils/log.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// Torna NULL en cas d'error
static char *
get_font_file (
               FcConfig    *fconf,
               const char  *font_desc,
               char       **err
               )
{

  FcPattern *pat;
  FcFontSet *fs;
  FcResult res;
  FcValue val;
  char *ret;
  
  
  // Crea patró
  pat= FcNameParse ( (FcChar8 *) font_desc );
  if ( pat == NULL )
    {
      msgerror ( err, "Failed to parse font description: '%s'", font_desc );
      return NULL;
    }
  FcConfigSubstitute ( fconf, pat, FcMatchPattern );
  FcDefaultSubstitute ( pat );

  // Cerca font.
  fs= FcFontSort ( fconf, pat, FcTrue, 0, &res );
  FcPatternDestroy ( pat );
  if ( fs == NULL || fs->nfont == 0 )
    {
      msgerror ( err, "Failed to locate a matching font for: %s", font_desc );
      return NULL;
    }

  // Obté valor i allibera memòria.
  FcPatternGet ( fs->fonts[0], FC_FILE, 0, &val );
  ret= (char *) g_strdup ( (char *) val.u.f );
  FcFontSetDestroy ( fs );

  return ret;
  
} // end get_font_file


static bool
set_font_file_name (
                    FcConfig        *fconf,
                    const gboolean   verbose,
                    const char      *font_long_desc,
                    const char      *font_desc,
                    char           **dst,
                    char           **err
                    )
{

  *dst= get_font_file ( fconf, font_desc, err );
  if ( *dst == NULL ) return false;
  if ( verbose )
    ii ( "[Fonts] %s: %s", font_long_desc, *dst );

  return true;
  
} // end set_font_file_name


static bool
get_font_files (
                Fonts       *f,
                char       **err
                )
{

  FcConfig *fconf;


  // Inicialitza fontconfig
  fconf= FcInitLoadConfigAndFonts ();
  if ( fconf == NULL )
    {
      msgerror ( err, "Failed to initialize fontconfig" );
      return false;
    }

  // Obté nom dels fitxers.
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Normal Roman",
                             f->_conf->font_normal_roman,
                             &(f->_font_normal_roman_fn),
                             err ) )
    goto error;
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Normal Bold",
                             f->_conf->font_normal_bold,
                             &(f->_font_normal_bold_fn),
                             err ) )
    goto error;
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Normal Italic",
                             f->_conf->font_normal_italic,
                             &(f->_font_normal_italic_fn),
                             err ) )
    goto error;
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Fixed-pitch Roman",
                             f->_conf->font_fpitch_roman,
                             &(f->_font_fpitch_roman_fn),
                             err ) )
    goto error;
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Fixed-pitch Bold",
                             f->_conf->font_fpitch_bold,
                             &(f->_font_fpitch_bold_fn),
                             err ) )
    goto error;
  if ( !set_font_file_name ( fconf, f->_conf->_verbose,
                             "Fixed-pitch Italic",
                             f->_conf->font_fpitch_italic,
                             &(f->_font_fpitch_italic_fn),
                             err ) )
    goto error;
  
  // Allibera memòria.
  FcConfigDestroy ( fconf );
  FcFini ();

  return true;

 error:
  FcConfigDestroy ( fconf );
  FcFini ();
  return false;
  
} // end get_font_files


static bool
open_font (
           const int    ptsize,
           const char  *file_name,
           TTF_Font   **dst,
           const bool   check_monospace,
           char       **err
           )
{

  *dst= TTF_OpenFont ( file_name, ptsize );
  if ( *dst == NULL )
    {
      msgerror ( err, "Failed to open font '%s' with point size %d",
                 file_name, ptsize );
      return false;
    }
  if ( check_monospace && TTF_FontFaceIsFixedWidth ( *dst ) == 0 )
    {
      msgerror ( err, "Invalid fixed width font: %s", file_name );
      return false;
    }
 
  return true;
  
} // end open_font


static bool
open_fonts (
            Fonts  *f,
            char  **err
            )
{

  // Fonts normal
  if ( !open_font ( f->_conf->font_size,
                    f->_font_normal_roman_fn,
                    &(f->_fonts[F_NORMAL][F_ROMAN]),
                    false, err ) )
    return false;
  if ( !open_font ( f->_conf->font_size,
                    f->_font_normal_bold_fn,
                    &(f->_fonts[F_NORMAL][F_BOLD]),
                    false, err ) )
    return false;
  if ( !open_font ( f->_conf->font_size,
                    f->_font_normal_italic_fn,
                    &(f->_fonts[F_NORMAL][F_ITALIC]),
                    false, err ) )
    return false;

  // Fonts fixed-pitch
  if ( !open_font ( f->_conf->font_size,
                    f->_font_fpitch_roman_fn,
                    &(f->_fonts[F_FPITCH][F_ROMAN]),
                    true, err ) )
    return false;
  if ( !open_font ( f->_conf->font_size,
                    f->_font_fpitch_bold_fn,
                    &(f->_fonts[F_FPITCH][F_BOLD]),
                    true, err ) )
    return false;
  if ( !open_font ( f->_conf->font_size,
                    f->_font_fpitch_italic_fn,
                    &(f->_fonts[F_FPITCH][F_ITALIC]),
                    true, err ) )
    return false;
  
  return true;
  
} // end open_fonts




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
fonts_free (
            Fonts *f
            )
{

  int i,j;


  for ( i= 0; i < F_NUM_FONTS; ++i )
    for ( j= 0; j < F_NUM_STYLES; ++j )
      if ( f->_fonts[i][j] != NULL )
        TTF_CloseFont ( f->_fonts[i][j] );
  g_free ( f->_font_normal_roman_fn );
  g_free ( f->_font_normal_bold_fn );
  g_free ( f->_font_normal_italic_fn );
  g_free ( f->_font_fpitch_roman_fn );
  g_free ( f->_font_fpitch_bold_fn );
  g_free ( f->_font_fpitch_italic_fn );
  g_free ( f );
  TTF_Quit ();
  
} // end fonts_free


Fonts *
fonts_new (
           Conf            *conf,
           const gboolean   verbose,
           char           **err
           )
{

  Fonts *ret;
  int i,j;
  

  // Prepara.
  ret= g_new ( Fonts, 1 );
  ret->_verbose= verbose;
  ret->_conf= conf;
  ret->_font_normal_roman_fn= NULL;
  ret->_font_normal_bold_fn= NULL;
  ret->_font_normal_italic_fn= NULL;
  ret->_font_fpitch_roman_fn= NULL;
  ret->_font_fpitch_bold_fn= NULL;
  ret->_font_fpitch_italic_fn= NULL;
  for ( i= 0; i < F_NUM_FONTS; ++i )
    for ( j= 0; j < F_NUM_STYLES; ++j )
      ret->_fonts[i][j]= NULL;

  // Inicialitza TTF
  if ( TTF_Init () == -1 )
    {
      msgerror ( err, "Failed to initialize SDL2_TTF" );
      goto error;
    }
  
  // Inicialitza.
  if ( !get_font_files ( ret, err ) )
    goto error;
  if ( !open_fonts ( ret, err ) )
    goto error;
  
  return ret;
  
 error:
  if ( ret != NULL ) fonts_free ( ret );
  return NULL;
  
} // end fonts_new
