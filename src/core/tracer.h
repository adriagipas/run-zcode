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
 *  tracer.h - Definició dels 'callbacks' per a tracejar.
 *
 */

#ifndef __CORE__TRACER_H__
#define __CORE__TRACER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "instruction.h"

typedef enum
  {
    MEM_ACCESS_READB,
    MEM_ACCESS_READW,
    MEM_ACCESS_WRITEB,
    MEM_ACCESS_WRITEW,
    MEM_ACCESS_READVAR,
    MEM_ACCESS_WRITEVAR
  } MemAccess;
typedef struct _Tracer Tracer;

// Tots els mètodes poden ser NULL.
#define TRACER_CLASS                                                    \
  /* Es crida cada vegada que s'executa una instrucció en mode trace. */ \
  void (*exec_inst) (Tracer *self,const Instruction *ins);              \
                                                                        \
  /* Es crida cada vegada que s'accedeix a la memòria. */               \
  void (*mem_access) (Tracer *self,const uint32_t addr,                 \
                      const uint16_t data,const MemAccess type);

struct _Tracer
{
  TRACER_CLASS;
};

#define TRACER(PTR) ((Tracer *) (PTR))

#endif // __CORE__TRACER_H__
