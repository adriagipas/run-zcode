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
 *  interpreter.h - Implementa l'intèrpret de la màquina Z.
 *
 */

#ifndef __CORE__INTERPRETER_H__
#define __CORE__INTERPRETER_H__

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "dictionary.h"
#include "disassembler.h"
#include "memory_map.h"
#include "state.h"
#include "story_file.h"
#include "tracer.h"

#include "frontend/conf.h"
#include "frontend/saves.h"
#include "frontend/screen.h"

#define INTP_MAX_OSTREAM3 16

#define INTP_OSTREAM_SCREEN     0x01
#define INTP_OSTREAM_TRANSCRIPT 0x02
#define INTP_OSTREAM_TABLE      0x04
#define INTP_OSTREAM_SCRIPT     0x08

typedef struct
{

  // TOT ÉS PRIVAT ///
  StoryFile    *sf;
  State        *state;
  MemoryMap    *mem;
  Instruction  *ins;
  Tracer       *tracer; // Pot ser NULL.
  Screen       *screen;
  Dictionary   *std_dict;
  Dictionary   *usr_dict;
  Saves        *saves;
  gboolean      verbose;
  
  // Altres
  uint8_t version;
  uint32_t routine_offset;
  uint32_t static_strings_offset;
  uint32_t object_table_offset;
  uint32_t abbr_table_addr;
  struct
  {
    size_t  size;
    size_t  N;
    char   *v;
  }        text;
  struct
  {
    size_t   size;
    size_t   N;
    uint8_t *v;  // ZSCII
  }        input_text;

  // Output streams
  struct
  {
    uint8_t active; // Mascara de bits
    int     N3;     // Nombre de streams 3 niats
    struct
    {
      uint32_t addr;
      uint16_t N;
    }       o3[INTP_MAX_OSTREAM3];
  } ostreams;

  // Extra characters
  struct
  {
    bool     enabled;
    uint8_t  N;
    uint16_t v[256];
  } echars;

  // Alphabet table
  struct
  {
    bool    enabled;
    uint8_t v[3][26];
  } alph_table;
  
} Interpreter;

void
interpreter_free (
                  Interpreter *intp
                  );

Interpreter *
interpreter_new_from_file_name (
                                const char      *file_name,
                                Conf            *conf,
                                const gboolean   verbose,
                                Tracer          *tracer, // Pot ser NULL
                                char           **err
                                );

// Torna cert si tot ha anat bé.
bool
interpreter_run (
                 Interpreter  *intp,
                 char        **err
                 );

bool
interpreter_trace (
                   Interpreter          *intp,
                   const unsigned long   iters,
                   char                **err
                   );

#endif // __CORE__INTERPRETER_H__
