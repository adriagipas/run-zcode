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
 *  dictionary.h - Diccionari màquina Z.
 *
 */

#ifndef __CORE__DICTIONARY_H__
#define __CORE__DICTIONARY_H__

#include <stdbool.h>
#include <stdint.h>

#include "memory_map.h"

typedef struct
{
  uint16_t addr;
  uint8_t  bytes[6];
} DictionaryEntry;

typedef struct
{
  uint8_t val;
  int     alph; // -1 vol dir no actiu.
} DictionaryAlphabetEntry;

typedef struct
{

  // PRIVAT
  MemoryMap       *_mem;
  uint8_t          _N_wseps;
  uint8_t          _wseps[256];
  uint16_t         _N;
  DictionaryEntry *_entries;
  uint16_t         _size;
  uint8_t          _text_length;
  int              _real_text_length;
  uint8_t          _version;
  
  // token buffer
  struct
  {
    size_t   N;
    size_t   size;
    uint8_t *v;
  } _token;

  // ZSCII -> Alphabet table
  DictionaryAlphabetEntry _zscii2alph[256];
  
} Dictionary;

void
dictionary_free (
                 Dictionary *d
                 );

Dictionary *
dictionary_new (
                MemoryMap  *mem,
                char      **err
                );

bool
dictionary_load (
                 Dictionary      *d,
                 const uint32_t   addr,
                 char           **err
                 );

bool
dictionary_parse (
                  Dictionary  *d,
                  uint16_t     text_buf,
                  uint16_t     parse_buf,
                  char       **err
                  );

#endif // __CORE_DICTIONARY_H__
