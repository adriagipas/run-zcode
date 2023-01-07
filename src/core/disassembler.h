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
 *  disassembler.h - Per a descodificar instruccions.
 *
 */

#ifndef __CORE__DISASSEMBLER_H__
#define __CORE__DISASSEMBLER_H__

#include <stdbool.h>
#include <stdint.h>

#include "instruction.h"
#include "memory_map.h"

void
instruction_free (
                  Instruction *ins
                  );

Instruction *
instruction_new (void);

bool
instruction_disassemble (
                         Instruction      *ins,
                         const MemoryMap  *mem,
                         const uint32_t    addr,
                         char            **err
                         );

#endif // __CORE__DISASSEMBLER_H__
