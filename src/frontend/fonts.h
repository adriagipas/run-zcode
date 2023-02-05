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
 *  fonts.h - S'encarrega de gestionar les fonts.
 *
 */

#ifndef __FRONTEND__FONTS_H__
#define __FRONTEND__FONTS_H__

#include <glib.h>
#include <stdbool.h>
#include <SDL_ttf.h>

#include "conf.h"

enum {
  F_NORMAL= 0,
  F_FPITCH,
  F_NUM_FONTS
};

enum {
  F_ROMAN= 0,
  F_BOLD,
  F_ITALIC,
  F_BOLD_ITALIC,
  F_NUM_STYLES
};

typedef struct
{

  // CAMPS PRIVATS.
  gboolean  _verbose;
  Conf     *_conf; // No s'allibera

  // Nom de fitxers
  char *_font_normal_roman_fn;
  char *_font_normal_bold_fn;
  char *_font_normal_italic_fn;
  char *_font_normal_bold_italic_fn;
  char *_font_fpitch_roman_fn;
  char *_font_fpitch_bold_fn;
  char *_font_fpitch_italic_fn;
  char *_font_fpitch_bold_italic_fn;

  // Fonts.
  TTF_Font *_fonts[F_NUM_FONTS][F_NUM_STYLES];
  
} Fonts;

void
fonts_free (
            Fonts *f
            );

Fonts *
fonts_new (
           Conf            *conf,
           const gboolean   verbose,
           char           **err
           );

int
fonts_char_height (
                   const Fonts *f
                   );

bool
fonts_char0_width (
                   const Fonts  *f,
                   int          *width,
                   char        **err
                   );

#endif // __FRONTEND__FONTS_H__
