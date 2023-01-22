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
 *  screen.c - Implementació de 'screen.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "screen.h"
#include "utils/error.h"




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
screen_free (
             Screen *s
             )
{

  if ( s->_fonts != NULL ) fonts_free ( s->_fonts );
  if ( s->_win != NULL ) window_free ( s->_win );
  g_free ( s );
  
} // end screen_free


Screen *
screen_new (
            Conf            *conf,
            const int        version,
            const char      *title,
            const gboolean   verbose,
            char           **err
            )
{

  Screen *ret;


  assert ( version >= 1 && version <= 8 && version != 6 );
  
  // Prepara.
  ret= g_new ( Screen, 1 );
  ret->_conf= conf;
  ret->_win= NULL;
  ret->_fonts= NULL;
  ret->_version= version;

  // Inicialitza fonts i calcula dimensions pantalla.
  ret->_fonts= fonts_new ( conf, verbose, err );
  if ( ret->_fonts == NULL ) goto error;
  ret->_line_height= fonts_char_height ( ret->_fonts );
  if ( ret->_line_height <= 0 )
    {
      msgerror ( err, "Failed to create screen: invalid font height %d",
                 ret->_line_height );
      goto error;
    }
  if ( !fonts_char0_width ( ret->_fonts, &(ret->_char_width), err ) )
    goto error;
  if ( ret->_char_width <= 0 )
    {
      msgerror ( err, "Failed to create screen: invalid char width %d",
                 ret->_char_width );
      goto error;
    }
  ret->_lines= conf->screen_lines;
  ret->_height= ret->_lines*ret->_line_height;
  ret->_width_chars= conf->screen_width;
  ret->_width= ret->_width_chars*ret->_char_width;
  if ( ret->_width <= 0 || ret->_height <= 0 )
    {
      msgerror ( err, "Failed to create screen" );
      goto error;
    }

  // Crea finestra.
  ret->_win= window_new ( ret->_width, ret->_height,
                          ret->_width, ret->_height,
                          title, NULL, err );
  if ( ret->_win == NULL ) goto error;
  window_show ( ret->_win );
  
  return ret;

 error:
  screen_free ( ret );
  return NULL;
  
} // end screen_new
