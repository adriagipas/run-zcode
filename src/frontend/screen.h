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
 *  screen.h - Pantalla de la màquina Z (menys la versió 6).
 *
 */

#ifndef __FRONTEND__SCREEN_H__
#define __FRONTEND__SCREEN_H__

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <SDL.h>

#include "conf.h"
#include "fonts.h"
#include "window.h"

// Desa estat per a renderitzat en cada cursor. En realitat desa
// informació sobre l'últim tros de de text renderitzat per a poder
// re-renderitzar si es continuen afegint caràcters a la paraula.
typedef struct
{

  // Estil actual
  int       font;
  int       style;
  uint16_t  fg_color;
  uint16_t  bg_color;
  
  // Posició
  int       line;
  int       x; // En píxels.
  int       width; // Píxels que ocupa el text actual

  // Contingut.
  char     *text; // Inclou '\0'
  size_t    size; // Memòria reserva en text
  size_t    N;    // Nombre de bytes (no inclou '\0')
  size_t    Nc;   // Nombre de caràcters UTF-8 (no inclou '\0')

  // Altres
  bool buffered; // En realitat el que fa és que si és true intenta no
                 // tallar paraules quan no cap
  bool space;    // Indica que l'últim caràcter imprés era un espai.
  
} ScreenCursor;


typedef struct
{

  // CAMPS PRIVATS.
  Conf   *_conf;
  Window *_win;
  Fonts  *_fonts;
  int     _version;
  
  // Dimensions.
  int _lines;
  int _width_chars;
  int _height,_width; // En píxels
  int _line_height; // En píxels
  int _char_width; // En píxels

  // Frame buffer
  uint32_t *_fb;
  uint16_t  _bg_color;
  uint16_t  _fg_color;
  bool      _reverse_color;

  // Finestres i altres.
  int          _upwin_lines;
  int          _current_win;
  int          _current_font;  // Efectiva, no la seleccionada. Sols afecta LOW.
  int          _current_style; // Efectiva, no la seleccionada
  uint8_t      _current_style_val;
  uint8_t      _current_font_val; // 1-Normal, 2-picture, 3-graph, 4-courier
  ScreenCursor _cursors[2];
  
  // Tokenitzer text
  struct
  {
    char   *buf;
    size_t  size;
    char   *p;
    bool    nl_buffered;
  } _split;

  // Altres
  SDL_Surface *_render_buf;
  
} Screen;

void
screen_free (
             Screen *s
             );

// NOTA!!! La versió ha de ser 1,2,3,4,5,7 o 8. No genera un error.
Screen *
screen_new (
            Conf            *conf,
            const int        version,
            const char      *title,
            const gboolean   verbose,
            char           **err
            );

bool
screen_print (
              Screen      *screen,
              const char  *text,
              char       **err
              );

void
screen_set_style (
                  Screen         *screen,
                  const uint16_t  style
                  );

#define screen_GET_LINES(SCREEN) ((SCREEN)->_lines)
#define screen_GET_WIDTH_CHARS(SCREEN) ((SCREEN)->_width_chars)

#endif // __FRONTEND__SCREEN_H__
