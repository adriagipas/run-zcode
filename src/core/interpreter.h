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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory_map.h"
#include "state.h"
#include "story_file.h"

typedef struct
{

  // TOT ÉS PRIVAT ///
  StoryFile *sf;
  State     *state;
  MemoryMap *mem;

  // Altres
  uint8_t version;
  uint32_t routine_offset;
  uint32_t static_strings_offset;
  
} Interpreter;

void
interpreter_free (
                  Interpreter *intp
                  );

Interpreter *
interpreter_new_from_file_name (
                                const char  *file_name,
                                char       **err
                                );

// Torna cert si tot ha anat bé.
bool
interpreter_run (
                 Interpreter  *intp,
                 char        **err
                 );

#endif // __CORE__INTERPRETER_H__
