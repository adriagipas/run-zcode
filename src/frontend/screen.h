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

#include "conf.h"
#include "fonts.h"
#include "window.h"

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

#endif // __FRONTEND__SCREEN_H__
