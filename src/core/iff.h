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
 *  iff.h - Llig l'estructura d'un fitxer en format IFF.
 *
 */

#ifndef __CORE__IFF_H__
#define __CORE__IFF_H__

#include <stdint.h>

typedef struct
{
  char     type[5]; // 4 caràcters NULL terminated
  uint32_t length;
  long     offset; // Posició on comença el chunk (inclou 8 bytes
                   // capçalera).
} IFFChunk;

typedef struct
{
  char      type[5];
  IFFChunk *chunks;
  size_t    N;
} IFF;

void
iff_free (
          IFF *iff
          );

IFF *
iff_new_from_file_name (
                        const char  *file_name,
                        char       **err
                        );

#endif // __CORE__IFF_H__
