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
 *  interpreter.c - Implementació 'interpreter.h'
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "interpreter.h"
#include "utils/error.h"




/**********/
/* MACROS */
/**********/

/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// MEM /////////////////////////////////////////////////////////////////////////

static bool
read_byte (
           Interpreter     *intp,
           const uint32_t   addr,
           uint8_t         *val,
           const bool       high_mem_allowed,
           char           **err
           )
{

  const State *st;
  const StoryFile *sf;
  
  
  st= intp->state;
  if ( addr < st->mem_size ) // Memòria dinàmica.
    *val= st->mem[addr];
  else
    {
      sf= intp->sf;
      if ( addr < (uint32_t) sf->size ) // Mèmòria estàtica i "high mem"
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
          if ( addr < intp->mem.high_mem_mark || high_mem_allowed )
            *val= sf->data[addr];
          else
            {
              msgerror ( err, "Failed to read a byte from memory: "
                         "access with address %X to high memory [%X,%X] "
                         "not allowed",
                         addr, intp->mem.high_mem_mark, sf->size-1 );
              return false;
            }
        }
      else // Error
        {
          msgerror ( err, "Failed to read a byte from memory: "
                     "address (%X) is out of bounds [0,%X]",
                     addr, sf->size-1 );
          return false;
        }
    }

  return true;
  
} // end read_byte


// NOTA!!! No sé si les adreces han d'estar aliniades (assumire que
// igual no) i que ocorre si una paraula està a mitat camí entre dos
// seccions (assumisc diverses coses).
static bool
read_word (
           Interpreter     *intp,
           const uint32_t   addr,
           uint16_t        *val,
           const bool       high_mem_allowed,
           char           **err
           )
{

  const State *st;
  const StoryFile *sf;
  
  
  st= intp->state;
  if ( addr < st->mem_size-1 ) // Memòria dinàmica.
    *val= (((uint16_t) st->mem[addr])<<8) | ((uint16_t) st->mem[addr+1]);
  else if ( addr == st->mem_size-1 )
    {
      msgerror ( err, "Failed to read a word from memory: "
                 "address (%X) is located at the end of dynamic memory [0,%X]",
                 addr, st->mem_size-1 );
      return false;
    }
  else
    {
      sf= intp->sf;
      if ( addr < (uint32_t) (sf->size-1) ) // Mèmòria estàtica i "high mem"
        {
          if ( addr < intp->mem.high_mem_mark-1 || high_mem_allowed )
            *val= (((uint16_t) sf->data[addr])<<8) |
              ((uint16_t) sf->data[addr+1]);
          else
            {
              msgerror ( err, "Failed to read a word from memory: "
                         "access with address %X to high memory [%X,%X] "
                         "not allowed",
                         addr, intp->mem.high_mem_mark, sf->size-2 );
              return false;
            }
        }
      else // Error
        {
          msgerror ( err, "Failed to read a word from memory: "
                     "address (%X) is out of bounds [0,%X]",
                     addr, sf->size-2 );
          return false;
        }
    }

  return true;
  
} // end read_word


static bool
init_mem (
          Interpreter  *intp,
          const char   *file_name,
          char        **err
          )
{

  // High memory mark.
  intp->mem.high_mem_mark=
    (((uint32_t) intp->state->mem[0x4])<<8) |
    ((uint32_t) intp->state->mem[0x5])
    ;
  if ( intp->mem.high_mem_mark < intp->state->mem_size )
    {
      msgerror ( err,
                 "High memory mark (%04X) overlaps with dynamic memory: %s",
                 intp->mem.high_mem_mark, file_name );
      return false;
    }
  
  return true;
  
} // end init_mem




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
interpreter_free (
                  Interpreter *intp
                  )
{

  if ( intp->sf != NULL ) story_file_free ( intp->sf );
  if ( intp->state != NULL ) state_free ( intp->state );
  g_free ( intp );
  
} // end interpreter_free


Interpreter *
interpreter_new_from_file_name (
                                const char  *file_name,
                                char       **err
                                )
{

  Interpreter *ret;


  // Prepara.
  ret= g_new ( Interpreter, 1 );
  ret->sf= NULL;
  ret->state= NULL;

  // Obri story file
  ret->sf= story_file_new_from_file_name ( file_name, err );
  if ( ret->sf == NULL ) goto error;

  // Crea estat.
  ret->state= state_new ( ret->sf, err );
  if ( ret->state == NULL ) goto error;

  // Inicialitza mapa de memòria.
  if ( !init_mem ( ret, file_name, err ) )
    goto error;

  uint8_t val;
  if ( !read_byte ( ret, ret->state->PC, &val, true, err ) )
    goto error;
  printf("READ BYTE: %X\n",val);
  
  return ret;

 error:
  interpreter_free ( ret );
  return NULL;
  
} // end interpreter_new_from_file_name
