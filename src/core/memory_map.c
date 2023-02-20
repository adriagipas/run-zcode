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
 *  memory_map.c - Implementació de 'memory_map.h'.
 *
 */


#include <glib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "memory_map.h"
#include "utils/error.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static bool
read_byte (
           const MemoryMap *mem,
           const uint32_t   addr,
           uint8_t         *val,
           const bool       high_mem_allowed,
           char           **err
           )
{
  
  if ( addr < mem->dyn_mem_size ) // Memòria dinàmica.
    *val= mem->dyn_mem[addr];
  else if ( addr < mem->sf_mem_size ) // Mèmòria estàtica i "high mem"
    {
      // NOTA!! Tinc dubtes si loadb i loadw poden accedir al
      // solapament amb la "high mem" [HIGH_MEM_MARK,FFFF]. Estes
      // dos frases em resulten contradictòries:
      //
      // Except for its (possible) overlap with static memory,
      // high memory cannot be directly accessed at all by a game
      // program.
      //
      // Stores array->byte-index (i.e., the byte at address
      // array+byte-index, which must lie in static or dynamic
      // memory).
      //
      // L'alternativa seria:
      // if ( addr <= 0xFFFF || high_mem_allowed )
      if ( addr < mem->high_mem_mark || high_mem_allowed )
        *val= mem->sf_mem[addr];
      else
        {
          msgerror ( err, "Failed to read a byte from memory: "
                     "access with address %X to high memory [%X,%X] "
                     "not allowed",
                     addr, mem->high_mem_mark, mem->sf_mem_size-1 );
          return false;
        }
    }
  else // Error
    {
      msgerror ( err, "Failed to read a byte from memory: "
                 "address (%X) is out of bounds [0,%X]",
                 addr, mem->sf_mem_size-1 );
      return false;
    }
  
  return true;
  
} // end read_byte


static bool
read_byte_trace (
                 const MemoryMap *mem,
                 const uint32_t   addr,
                 uint8_t         *val,
                 const bool       high_mem_allowed,
                 char           **err
                 )
{

  if ( !read_byte ( mem, addr, val, high_mem_allowed, err ) )
    return false;
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, addr, (uint16_t) *val, MEM_ACCESS_READB );

  return true;
  
} // end read_byte_trace


// NOTA!!! No sé si les adreces han d'estar aliniades (assumire que
// igual no) i que ocorre si una paraula està a mitat camí entre dos
// seccions (assumisc diverses coses).
static bool
read_word (
           const MemoryMap *mem,
           const uint32_t   addr,
           uint16_t        *val,
           const bool       high_mem_allowed,
           char           **err
           )
{
  
  if ( addr < mem->dyn_mem_size-1 ) // Memòria dinàmica.
    *val= (((uint16_t) mem->dyn_mem[addr])<<8) |
      ((uint16_t) mem->dyn_mem[addr+1]);
  else if ( addr == mem->dyn_mem_size-1 )
    {
      msgerror ( err, "Failed to read a word from memory: "
                 "address (%X) is located at the end of dynamic memory [0,%X]",
                 addr, mem->dyn_mem_size-1 );
      return false;
    }
  else if ( addr < (mem->sf_mem_size-1) ) // Mèmòria estàtica i "high mem"
    {
      if ( addr < mem->high_mem_mark-1 || high_mem_allowed )
        *val= (((uint16_t) mem->sf_mem[addr])<<8) |
          ((uint16_t) mem->sf_mem[addr+1]);
      else
        {
          msgerror ( err, "Failed to read a word from memory: "
                     "access with address %X to high memory [%X,%X] "
                     "not allowed",
                     addr, mem->high_mem_mark, mem->sf_mem_size-2 );
          return false;
        }
    }
  else // Error
    {
      msgerror ( err, "Failed to read a word from memory: "
                 "address (%X) is out of bounds [0,%X]",
                 addr, mem->sf_mem_size-2 );
      return false;
    }
  
  return true;
  
} // end read_word


static bool
read_word_trace (
                 const MemoryMap *mem,
                 const uint32_t   addr,
                 uint16_t        *val,
                 const bool       high_mem_allowed,
                 char           **err
                 )
{

  if ( !read_word ( mem, addr, val, high_mem_allowed, err ) )
    return false;
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, addr, *val, MEM_ACCESS_READW );
  
  return true;
  
} // end read_word_trace


static bool
write_byte (
            const MemoryMap *mem,
            const uint32_t   addr,
            const uint8_t    val,
            const bool       high_mem_allowed,
            char           **err
            )
{

  uint8_t mask;
  
  
  if ( addr < 64 ) // Capçalera
    {
      if ( addr == 0x10 ) // Flags2
        {
          if ( mem->version < 3 ) mask= 0x01;
          else if ( mem->version == 6 ) mask= 0x7;
          else mask= 0x3;
          mem->dyn_mem[0x10]= (mem->dyn_mem[0x10]&(~mask)) | (val&mask);
        }
      else
        {
          msgerror ( err, "Failed to write a byte into memory: "
                     "access with address %X to header [0,3F] "
                     "not allowed",
                     addr );
          return false;
        }
    }
  else if ( addr < mem->dyn_mem_size ) // Memòria dinàmica.
    mem->dyn_mem[addr]= val;
  else // Error
    {
      if ( addr < mem->sf_mem_size )
        msgerror ( err, "Failed to write a byte into memory: "
                   "access with address %X to static memory [%X,%X] "
                   "not allowed",
                   addr, mem->dyn_mem_size, mem->sf_mem_size-1 );
      else
        msgerror ( err, "Failed to write a byte into memory: "
                   "address (%X) is out of bounds [0,%X]",
                   addr, mem->sf_mem_size-1 );
      return false;
    }
  
  return true;
  
} // end write_byte


static bool
write_byte_trace (
                  const MemoryMap *mem,
                  const uint32_t   addr,
                  const uint8_t    val,
                  const bool       high_mem_allowed,
                  char           **err
                  )
{

  if ( !write_byte ( mem, addr, val, high_mem_allowed, err ) )
    return false;
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, addr, (uint16_t) val, MEM_ACCESS_WRITEB );

  return true;
  
} // end write_byte_trace


static bool
write_word (
            const MemoryMap *mem,
            const uint32_t   addr,
            const uint16_t   val,
            const bool       high_mem_allowed,
            char           **err
            )
{

  uint8_t mask,data;
  
  
  if ( addr < 64 ) // Capçalera
    {
      if ( addr == 0x09 || addr == 0x10 ) // Flags2
        {
          if ( mem->version < 3 ) mask= 0x01;
          else if ( mem->version == 6 ) mask= 0x7;
          else mask= 0x3;
          data= addr==0x09 ? ((uint8_t) val) : ((uint8_t) (val>>8));
          mem->dyn_mem[0x10]= (mem->dyn_mem[0x10]&(~mask)) | (data&mask);
        }
      else
        {
          msgerror ( err, "Failed to write a word into memory: "
                     "access with address %X to header [0,3F] "
                     "not allowed",
                     addr );
          return false;
        }
    }
  else if ( addr < mem->dyn_mem_size-1 ) // Memòria dinàmica.
    {
      mem->dyn_mem[addr]= (uint8_t) (val>>8);
      mem->dyn_mem[addr+1]= (uint8_t) val;
    }
  else // Error
    {
      if ( addr < mem->sf_mem_size-1 )
        msgerror ( err, "Failed to write a word into memory: "
                   "access with address %X to static memory [%X,%X] "
                   "not allowed",
                   addr, mem->dyn_mem_size, mem->sf_mem_size-2 );
      else
        msgerror ( err, "Failed to write a word into memory: "
                   "address (%X) is out of bounds [0,%X]",
                   addr, mem->sf_mem_size-1 );
      return false;
    }
  
  return true;
  
} // end write_word


static bool
write_word_trace (
                  const MemoryMap  *mem,
                  const uint32_t    addr,
                  const uint16_t    val,
                  const bool        high_mem_allowed,
                  char            **err
                  )
{

  if ( !write_word ( mem, addr, val, high_mem_allowed, err ) )
    return false;
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, addr, val, MEM_ACCESS_WRITEW );

  return true;
  
} // end write_word_trace


static uint16_t
readvar (
         const MemoryMap *mem,
         const int        ind
         )
{
  
  uint32_t offset;
  uint16_t ret;
  
  
  offset= mem->global_var_offset + ind*2;
  ret=
    (((uint16_t) mem->dyn_mem[offset])<<8) |
    ((uint16_t) mem->dyn_mem[offset+1])
    ;
  
  return ret;
  
} // end readvar


static uint16_t
readvar_trace (
               const MemoryMap *mem,
               const int        ind
               )
{
  
  uint16_t ret;


  ret= readvar ( mem, ind );
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, (uint16_t) ind, ret, MEM_ACCESS_READVAR );

  return ret;
  
} // end readvar_trace


static void
writevar (
          MemoryMap      *mem,
          const int       ind,
          const uint16_t  val
          )
{

  uint32_t offset;
  

  offset= mem->global_var_offset + ind*2;
  mem->dyn_mem[offset]= (uint8_t) (val>>8);
  mem->dyn_mem[offset+1]= (uint8_t) val;
  
} // end writevar


static void
writevar_trace (
                MemoryMap      *mem,
                const int       ind,
                const uint16_t  val
                )
{

  writevar ( mem, ind, val );
  if ( mem->tracer != NULL && mem->tracer->mem_access != NULL )
    mem->tracer->mem_access
      ( mem->tracer, (uint16_t) ind, val, MEM_ACCESS_WRITEVAR );
  
} // end writevar_trace




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
memory_map_free (
                 MemoryMap *mem
                 )
{
  g_free ( mem );
} // end memory_map_free


MemoryMap *
memory_map_new (
                const StoryFile  *sf,
                const State      *state,
                Tracer           *tracer,
                char            **err
                )
{

  MemoryMap *ret;


  // Inicialitza.
  ret= g_new ( MemoryMap, 1 );
  ret->dyn_mem= state->mem;
  ret->dyn_mem_size= state->mem_size;
  ret->sf_mem= sf->data;
  ret->sf_mem_size= (uint32_t) (sf->size);
  ret->tracer= tracer;

  // High memory mark.
  ret->high_mem_mark=
    (((uint32_t) ret->dyn_mem[0x4])<<8) |
    ((uint32_t) ret->dyn_mem[0x5])
    ;
  if ( ret->high_mem_mark < ret->dyn_mem_size )
    {
      msgerror ( err,
                 "High memory mark (%04X) overlaps with dynamic memory",
                 ret->high_mem_mark );
      goto error;
    }

  // Version.
  ret->version= ret->dyn_mem[0];

  // Callbacks.
  ret->readb= read_byte;
  ret->readw= read_word;
  ret->writeb= write_byte;
  ret->writew= write_word;
  ret->readvar= readvar;
  ret->writevar= writevar;

  // Altres.
  ret->global_var_offset=
    (((uint32_t) ret->sf_mem[0xc])<<8) |
    ((uint32_t) ret->sf_mem[0xd])
    ;
  if ( ret->global_var_offset >= ret->dyn_mem_size ||
       ret->global_var_offset+(240*2) > ret->dyn_mem_size )
    {
      msgerror ( err,
                 "Global variables table [%X,%X] is not located in"
                 " dynamic memory [0,%X]",
                 ret->global_var_offset, ret->global_var_offset+(240*2)-1,
                 ret->dyn_mem_size-1 );
      goto error;
    }
  
  return ret;
  
 error:
  if ( ret != NULL ) memory_map_free ( ret );
  return NULL;
  
} // end memory_map_new


void
memory_map_enable_trace (
                         MemoryMap  *mem,
                         const bool  enable
                         )
{

  if ( enable )
    {
      mem->readb= read_byte_trace;
      mem->readw= read_word_trace;
      mem->writeb= write_byte_trace;
      mem->writew= write_word_trace;
      mem->readvar= readvar_trace;
      mem->writevar= writevar_trace;
    }
  else
    {
      mem->readb= read_byte;
      mem->readw= read_word;
      mem->writeb= write_byte;
      mem->writew= write_word;
      mem->readvar= readvar;
      mem->writevar= writevar;
    }
  
} // end memory_map_enable_trace
