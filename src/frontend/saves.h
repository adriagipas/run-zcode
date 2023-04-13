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
 *  saves.h - Gestiona els fitxers utilitzats per a desar.
 *
 */

#ifndef __FRONTEND__SAVES_H__
#define __FRONTEND__SAVES_H__

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAVES_MAX_UNDO 10

typedef struct
{

  // TOT PRIVAT!!
  gboolean  _verbose;
  gchar    *_undo_fn[SAVES_MAX_UNDO];
  int       _N_undo;
  int       _pos; // Principi de la llista
  
} Saves;

void
saves_free (
            Saves *s
            );

Saves *
saves_new (
           const gboolean verbose
           );

// NULL en cas d'error. Si està ple esborra la més antiga.
const gchar *
saves_get_new_undo_file_name (
                              Saves  *s,
                              char  **err
                              );

// Torna l'últim undo fet, però no l'esborra. Podria torna NULL si no
// s'ha cridat abans a saves_get_new_undo_file_name
const gchar *
saves_get_undo_file_name (
                          Saves  *s
                          );

// Elimina l'últim fitxer undo. S'ha de cridar sabent que hi ha un
// fitxer.
void
saves_remove_last_undo_file_name (
                                  Saves *s
                                  );

#endif // __FRONTEND__SAVES_H__
