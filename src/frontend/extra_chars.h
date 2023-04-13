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
 *  extra_chars.h - Utilitzat per 'screen' per a gestionar caràcters
 *                  adicionals que poden ser llegits.
 *
 */

#ifndef __FRONTEND__EXTRA_CHARS_H__
#define __FRONTEND__EXTRA_CHARS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint16_t unicode;
  uint8_t  zcode;
} ExtraCharsEntry;

typedef struct
{

  // TOT PRIVAT
  bool             _sorted;
  int              _size;
  int              _N;
  ExtraCharsEntry *_v;
  
} ExtraChars;

void
extra_chars_free (
                  ExtraChars *ec
                  );

ExtraChars *
extra_chars_new (void);

bool
extra_chars_add (
                 ExtraChars      *ec,
                 const uint16_t   unicode,
                 const uint8_t    zcode,
                 char           **err
                 );

// Intenta descodificar un caràcter ZSCII extra de 'text'. 'end_pos'
// apunta al primer byte de 'text' no processat. Si no pot
// descodificar-lo torna 0 (caràcter null).
uint8_t
extra_chars_decode_next_char (
                              ExtraChars  *ec,
                              const char  *text,
                              int         *end_pos
                              );

#endif // __FRONTEND__EXTRA_CHARS_H__
