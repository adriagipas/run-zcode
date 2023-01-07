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
 *  disassembler.c - Implementació de 'disassembler.h'.
 *
 */


#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "disassembler.h"
#include "utils/error.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

// Aquesta funció assumeix que en op ja s'ha llegit el valor en 'u8' i
// segons el seu valor fixa el tipus de variable.
static void
set_variable_type (
                   InstructionOp *op
                   )
{

  if ( op->u8 == 0x00 )
    op->type= INSTRUCTION_OP_TYPE_TOP_STACK;
  else if ( op->u8 <= 0x0f )
    {
      op->type= INSTRUCTION_OP_TYPE_LOCAL_VARIABLE;
      op->u8-= 1;
    }
  else
    {
      op->type= INSTRUCTION_OP_TYPE_GLOBAL_VARIABLE;
      op->u8-= 0x10;
    }
  
} // end set_variable_type


// Assumeix que en 'u16' està l'addreça packed.
static void
set_routine_type (
                  Instruction     *ins,
                  const MemoryMap *mem,
                  const int        op
                  )
{

  uint8_t version;
  uint32_t offset;
  
  
  version= mem->sf_mem[0];
  ins->ops[op].type= INSTRUCTION_OP_TYPE_ROUTINE;
  if ( version <= 3 )
    ins->ops[op].u32= ((uint32_t) ins->ops[op].u16)<<1;
  else if ( version <= 5 )
    ins->ops[op].u32= ((uint32_t) ins->ops[op].u16)<<2;
  else if ( version <= 7 )
    {
      ins->ops[op].u32= ((uint32_t) ins->ops[op].u16)<<2;
      offset=
        (((uint32_t) mem->sf_mem[0x28])<<8) |
        ((uint32_t) mem->sf_mem[0x29])
        ;
      ins->ops[op].u32+= offset;
    }
  else
    ins->ops[op].u32= ((uint32_t) ins->ops[op].u16)<<3;
  
} // end set_routine_type


static bool
read_var_ops (
              Instruction      *ins,
              const MemoryMap  *mem,
              uint32_t         *addr,
              char            **err
              )
{

  uint8_t ops_type;
  int N,n;

  
  // Llig tipus
  if ( !memory_map_READB ( mem, *addr, &ops_type, true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ops_type;

  // Processa tipus
  N= 0;
  do {
    switch ( ops_type>>6 )
      {
      case 0:
        ins->ops[N++].type= INSTRUCTION_OP_TYPE_LARGE_CONSTANT;
        break;
      case 1:
        ins->ops[N++].type= INSTRUCTION_OP_TYPE_SMALL_CONSTANT;
        break;
      case 2:
        ins->ops[N++].type= INSTRUCTION_OP_TYPE_TOP_STACK; // Provisional
        break;
      case 3:
        ins->ops[N].type= INSTRUCTION_OP_TYPE_NONE;
        break;
      }
    ops_type<<= 2;
  } while ( N < 4 && ins->ops[N].type != INSTRUCTION_OP_TYPE_NONE );

  // Llig valors
  for ( n= 0; n < N; ++n )
    if ( ins->ops[n].type == INSTRUCTION_OP_TYPE_LARGE_CONSTANT )
      {
        if ( !memory_map_READW ( mem, *addr, &(ins->ops[n].u16), true, err ) )
          return false;
        (*addr)+= 2;
        ins->bytes[ins->nbytes++]= (uint8_t) (ins->ops[n].u16>>8);
        ins->bytes[ins->nbytes++]= (uint8_t) ins->ops[n].u16;
      }
    else
      {
        if ( !memory_map_READB ( mem, *addr, &(ins->ops[n].u8), true, err ) )
          return false;
        ++(*addr);
        ins->bytes[ins->nbytes++]= ins->ops[n].u8;
        if ( ins->ops[n].type == INSTRUCTION_OP_TYPE_TOP_STACK )
          set_variable_type ( &(ins->ops[n]) );
      }
  ins->nops= N;
  
  return true;
  
} // end read_var_ops


static bool
read_var_ops_store (
                    Instruction      *ins,
                    const MemoryMap  *mem,
                    uint32_t         *addr,
                    char            **err
                    )
{

  if ( !read_var_ops ( ins, mem, addr, err ) )
    return false;
  if ( !memory_map_READB ( mem, *addr, &(ins->store_op.u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->store_op.u8;
  set_variable_type ( &(ins->store_op) );
  ins->store= true;
  
  return true;
  
} // end read_var_ops_store


static bool
ins_call (
          Instruction      *ins,
          const MemoryMap  *mem,
          char            **err
          )
{
  
  if ( ins->nops == 0 )
    {
      msgerror ( err, "Failed to diassemble call instruction: missing"
                 " routine argument" );
      return false;
    }
  if ( ins->ops[0].type != INSTRUCTION_OP_TYPE_LARGE_CONSTANT )
    {
      msgerror ( err, "Failed to diassemble call instruction: "
                 "invalid operand type for routine argument" );
      return false;
    }
  set_routine_type ( ins, mem, 0 );
  ins->name= INSTRUCTION_NAME_CALL;
  
  return true;
  
} // end ins_call


static bool
decode_next_inst (
                  Instruction      *ins,
                  const MemoryMap  *mem,
                  char            **err
                  )
{

  uint8_t opcode;
  uint32_t addr;

  addr= ins->addr;
  
  // Opcode
  if ( !memory_map_READB ( mem, addr++, &opcode, true, err ) )
    return false;
  ins->bytes[0]= opcode;
  ins->nbytes= 1;
  switch ( opcode )
    {

    case 0xe0: // call_vs
      if ( !read_var_ops_store ( ins, mem, &addr, err ) ) return false;
      if ( !ins_call ( ins, mem, err ) ) return false;
      break;
      
    default: // Descodifica com UNK
      break;
    }

  return true;
  
} // end decode_next_inst




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
instruction_free (
                  Instruction *ins
                  )
{

  g_free ( ins->bytes );
  g_free ( ins );
  
} // end instruction_free


Instruction *
instruction_new (void)
{

  Instruction *ret;


  ret= g_new ( Instruction, 1 );
  // 23 són els bytes màxims necessaris sense comptar les cadenes de text.
  ret->bytes_size= 23;
  ret->bytes= g_new ( uint8_t, ret->bytes_size );

  return ret;
  
} // end instruction_new


bool
instruction_disassemble (
                         Instruction      *ins,
                         const MemoryMap  *mem,
                         const uint32_t    addr,
                         char            **err
                         )
{

  // Prepara.
  ins->addr= addr;
  ins->name= INSTRUCTION_NAME_UNK;
  ins->nbytes= 0;
  ins->nops= 0;
  ins->store= false;

  // Decodifica.
  if ( !decode_next_inst ( ins, mem, err ) )
    return false;
  
  return true;
  
} // end instruction_disassemble
