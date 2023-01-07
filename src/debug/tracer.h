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
 *  tracer.h - Implementa una classe 'Tracer'.
 *
 */

#ifndef __DEBUG__TRACER_H__
#define __DEBUG__TRACER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/disassembler.h"
#include "core/tracer.h"

#define DEBUG_TRACER_FLAGS_CPU 0x01
#define DEBUG_TRACER_FLAGS_MEM 0x02

typedef struct
{

  // Tots els camps són privats
  TRACER_CLASS;
  uint32_t flags; // Indica que s'imprimeix per pantalla i què no.
  
} DebugTracer;

void
debug_tracer_free (
                   DebugTracer *t
                   );

DebugTracer *
debug_tracer_new (
                  const uint32_t init_flags
                  );

#endif // __DEBUG__TRACER_H__
