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
 *  memory_map.h - Mapa de memòria de l'intèrpret.
 *
 */

#ifndef __CORE__MEMORY_MAP_H__
#define __CORE__MEMORY_MAP_H__

#include <stdint.h>

#include "state.h"
#include "story_file.h"

typedef struct _MemoryMap MemoryMap;

struct _MemoryMap
{

  // Camps Privats. Valors estrets d'altres estructures, no s'han
  // d'alliberar.
  uint8_t       *dyn_mem; // Punter a la memòria dinàmica
  uint32_t       dyn_mem_size;
  const uint8_t *sf_mem;   // Punter a la memòria del fitxer.
  uint32_t       sf_mem_size;
  uint32_t       high_mem_mark;

  uint8_t        version;
  uint32_t       global_var_offset;

  // Callbacks.
  bool (*readb) (const MemoryMap*,const uint32_t,uint8_t*,const bool,char**);
  bool (*readw) (const MemoryMap*,const uint32_t,uint16_t*,const bool,char**);
  bool (*writeb) (const MemoryMap*,const uint32_t,
                  const uint8_t,const bool,char**);
  bool (*writew) (const MemoryMap*,const uint32_t,
                  const uint16_t,const bool,char**);
  
};

void
memory_map_free (
                 MemoryMap *mem
                 );

MemoryMap *
memory_map_new (
                const StoryFile  *sf,
                const State      *state,
                char            **err
                );

#define memory_map_READB(MEM,ADDR,DST,HMEM,ERR)                 \
  ((MEM)->readb ( (MEM), (ADDR), (DST), (HMEM), (ERR) ))
#define memory_map_READW(MEM,ADDR,DST,HMEM,ERR)                 \
  ((MEM)->readw ( (MEM), (ADDR), (DST), (HMEM), (ERR) ))
#define memory_map_WRITEB(MEM,ADDR,VAL,HMEM,ERR)                \
  ((MEM)->writeb ( (MEM), (ADDR), (VAL), (HMEM), (ERR) ))
#define memory_map_WRITEW(MEM,ADDR,VAL,HMEM,ERR)                \
  ((MEM)->writew ( (MEM), (ADDR), (VAL), (HMEM), (ERR) ))

// NOTA!! No comprova res.
uint16_t
memory_map_readvar (
                    const MemoryMap *mem,
                    const int        ind
                    );

// NOTA!! No comprova res.
void
memory_map_writevar (
                     MemoryMap      *mem,
                     const int       ind,
                     const uint16_t  val
                     );

#endif // __CORE__MEMORY_MAP_H__
