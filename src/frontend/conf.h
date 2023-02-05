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
 *  conf.h - Fitxer de configuració.
 *
 */

#ifndef __FRONTEND__CONF_H__
#define __FRONTEND__CONF_H__

#include <glib.h>

typedef struct
{

  // CAMPS PÚBLICS
  // --> Screen
  gint     screen_lines;
  gint     screen_width; // Mesurat en caràcters
  gboolean screen_fullscreen;
  
  // --> Fonts
  gint   font_size;
  gchar *font_normal_roman;
  gchar *font_normal_bold;
  gchar *font_normal_italic;
  gchar *font_normal_bold_italic;
  gchar *font_fpitch_roman;
  gchar *font_fpitch_bold;
  gchar *font_fpitch_italic;
  gchar *font_fpitch_bold_italic;
  
  // CAMPS PRIVATS
  gboolean  _verbose;
  gchar    *_file_name;
  
} Conf;

void
conf_free (
           Conf *conf
           );

// Si no es proporciona un 'file_name' es llig el fitxer per defecte.
Conf *
conf_new (
          const gboolean   verbose,
          const gchar     *file_name, // Pot ser NULL
          char           **err
          );

// Escriu en disc la configuració actual.
bool
conf_write (
            Conf  *conf,
            char **err
            );

#endif // __FRONTEND__CONF_H__
