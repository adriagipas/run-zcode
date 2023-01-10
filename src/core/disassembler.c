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
#include "utils/log.h"




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
  bool stop;

  
  // Llig tipus
  if ( !memory_map_READB ( mem, *addr, &ops_type, true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ops_type;

  // Processa tipus
  N= 0;
  stop= false;
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
        stop= true;
        break;
      }
    ops_type<<= 2;
  } while ( N < 4 && !stop );
  
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
read_branch (
             Instruction      *ins,
             const MemoryMap  *mem,
             uint32_t         *addr,
             char            **err
             )
{

  uint8_t b1,b2;
  bool cond_value;
  

  // Llig valors, calcula offset i valor condició.
  if ( !memory_map_READB ( mem, *addr, &b1, true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= b1;
  if ( (b1&0x40) == 0 )
    {
      if ( !memory_map_READB ( mem, *addr, &b2, true, err ) )
        return false;
      ++(*addr);
      ins->bytes[ins->nbytes++]= b2;
      ins->branch_op.u32= (((uint32_t) (b1&0x3F))<<8) | ((uint32_t) b2);
      if ( ins->branch_op.u32&0x2000 )
        ins->branch_op.u32= ((uint32_t) -((int32_t) ins->branch_op.u32));
    }
  else ins->branch_op.u32= (uint32_t) (b1&0x3F);
  cond_value= ((b1&0x80)!=0); // True si bit7 està actiu.

  // Assigna tipus
  ins->branch= true;
  if ( ins->branch_op.u32 == 0 )
    ins->branch_op.type=
      cond_value ?
      INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_TRUE :
      INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_FALSE
      ;
  else if ( ins->branch_op.u32 == 1 )
    ins->branch_op.type=
      cond_value ?
      INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_TRUE :
      INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_FALSE
      ;
  else
    {
      ins->branch_op.type=
      cond_value ?
      INSTRUCTION_OP_TYPE_BRANCH_IF_TRUE :
      INSTRUCTION_OP_TYPE_BRANCH_IF_FALSE
      ;
      ins->branch_op.u32-= 2;
    }

  return true;
  
} // end read_branch


static bool
ins_2op (
         Instruction            *ins,
         const MemoryMap        *mem,
         uint32_t               *addr,
         const InstructionName   name,
         char                  **err
         )
{

  uint8_t opcode;

  
  ins->name= name;
  opcode= ins->bytes[ins->nbytes-1]; // Byte anterior

  // Primer operador.
  if ( !memory_map_READB ( mem, *addr, &(ins->ops[ins->nops].u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->ops[ins->nops].u8;
  if ( opcode&0x40 ) set_variable_type ( &(ins->ops[ins->nops]) );
  else ins->ops[ins->nops].type= INSTRUCTION_OP_TYPE_SMALL_CONSTANT;
  ++(ins->nops);

  // Segon operador.
  if ( !memory_map_READB ( mem, *addr, &(ins->ops[ins->nops].u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->ops[ins->nops].u8;
  if ( opcode&0x20 ) set_variable_type ( &(ins->ops[ins->nops]) );
  else ins->ops[ins->nops].type= INSTRUCTION_OP_TYPE_SMALL_CONSTANT;
  ++(ins->nops);

  return true;
  
} // end ins_2op


static bool
ins_2op_branch (
                Instruction            *ins,
                const MemoryMap        *mem,
                uint32_t               *addr,
                const InstructionName   name,
                char                  **err
                )
{

  if ( !ins_2op ( ins, mem, addr, name, err ) )
    return false;
  if ( !read_branch ( ins, mem, addr, err ) )
    return false;

  return true;
  
} // end ins_2op_branch


static bool
ins_2op_store (
               Instruction            *ins,
               const MemoryMap        *mem,
               uint32_t               *addr,
               const InstructionName   name,
               char                  **err
               )
{

  if ( !ins_2op ( ins, mem, addr, name, err ) )
    return false;
  if ( !memory_map_READB ( mem, *addr, &(ins->store_op.u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->store_op.u8;
  set_variable_type ( &(ins->store_op) );
  ins->store= true;
  
  return true;
  
} // end ins_2op_store


static bool
ins_1op (
         Instruction            *ins,
         const MemoryMap        *mem,
         uint32_t               *addr,
         const InstructionName   name,
         char                  **err
         )
{

  uint8_t opcode;

  
  ins->name= name;
  opcode= ins->bytes[ins->nbytes-1]; // Byte anterior
  switch ( (opcode>>4)&0x3 )
    {
    case 0:
      if ( !memory_map_READW ( mem, *addr, &(ins->ops[ins->nops].u16),
                               true, err ) )
        return false;
      (*addr)+= 2;
      ins->ops[ins->nops].type= INSTRUCTION_OP_TYPE_LARGE_CONSTANT;
      ins->bytes[ins->nbytes++]= (uint8_t) (ins->ops[ins->nops].u16>>8);
      ins->bytes[ins->nbytes++]= (uint8_t) ins->ops[ins->nops].u16;
      ++(ins->nops);
      break;
    case 1:
      if ( !memory_map_READB ( mem, *addr, &(ins->ops[ins->nops].u8),
                               true, err ) )
        return false;
      ++(*addr);
      ins->ops[ins->nops].type= INSTRUCTION_OP_TYPE_SMALL_CONSTANT;
      ins->bytes[ins->nbytes++]= (uint8_t) ins->ops[ins->nops].u8;
      ++(ins->nops);
      break;
    case 2:
      if ( !memory_map_READB ( mem, *addr, &(ins->ops[ins->nops].u8),
                               true, err ) )
        return false;
      ++(*addr);
      set_variable_type ( &(ins->ops[ins->nops]) );
      ins->bytes[ins->nbytes++]= (uint8_t) ins->ops[ins->nops].u8;
      ++(ins->nops);
      break;
    default:
      ee ( "disassembler.c - ins_2op - WTF!!" );
    }
  
  return true;
  
} // end ins_1op


static bool
ins_1op_branch (
                Instruction            *ins,
                const MemoryMap        *mem,
                uint32_t               *addr,
                const InstructionName   name,
                char                  **err
                )
{

  if ( !ins_1op ( ins, mem, addr, name, err ) )
    return false;
  if ( !read_branch ( ins, mem, addr, err ) )
    return false;

  return true;
  
} // end ins_1op_branch


static bool
ins_call (
          Instruction      *ins,
          const MemoryMap  *mem,
          char            **err
          )
{
  
  if ( ins->nops == 0 )
    {
      msgerror ( err, "Failed to disassemble call instruction: missing"
                 " routine argument" );
      return false;
    }
  if ( ins->ops[0].type != INSTRUCTION_OP_TYPE_LARGE_CONSTANT )
    {
      msgerror ( err, "Failed to disassemble call instruction: "
                 "invalid operand type for routine argument" );
      return false;
    }
  set_routine_type ( ins, mem, 0 );
  ins->name= INSTRUCTION_NAME_CALL;
  
  return true;
  
} // end ins_call


static bool
ins_var_1op (
             Instruction            *ins,
             const InstructionName   name,
             char                  **err
              )
{
  
  if ( ins->nops != 1 )
    {
      msgerror ( err, "Failed to disassemble 1OP instruction in"
                 " VAR format: provided %d operands", ins->nops );
      return false;
    }
  ins->name= name;
  
  return true;
  
} // end ins_var_1op


static bool
ins_var_2ops (
              Instruction            *ins,
              const InstructionName   name,
              char                  **err
              )
{
  
  if ( ins->nops != 2 )
    {
      msgerror ( err, "Failed to disassemble 2OP instruction in"
                 " VAR format: provided %d operands", ins->nops );
      return false;
    }
  ins->name= name;
  
  return true;
  
} // end ins_var_2ops


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

    case 0x01: // je
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JE, err ) )
        return false;
      break;
    case 0x02: // jl
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JL, err ) )
        return false;
      break;
    case 0x03: // jg
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JG, err ) )
        return false;
      break;

    case 0x14: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;

    case 0x21: // je
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JE, err ) )
        return false;
      break;
    case 0x22: // jl
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JL, err ) )
        return false;
      break;
    case 0x23: // jg
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JG, err ) )
        return false;
      break;

    case 0x34: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;

    case 0x41: // je
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JE, err ) )
        return false;
      break;
    case 0x42: // jl
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JL, err ) )
        return false;
      break;
    case 0x43: // jg
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JG, err ) )
        return false;
      break;

    case 0x54: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;

    case 0x61: // je
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JE, err ) )
        return false;
      break;
    case 0x62: // jl
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JL, err ) )
        return false;
      break;
    case 0x63: // jg
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JG, err ) )
        return false;
      break;

    case 0x74: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;

    case 0x80: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;

    case 0x90: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;

    case 0xa0: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;
      
    case 0xc9: // and
      if ( !read_var_ops_store ( ins, mem, &addr, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_AND, err ) ) return false;
      break;
      
    case 0xd5: // sub
      if ( !read_var_ops_store ( ins, mem, &addr, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_SUB, err ) ) return false;
      break;
      
    case 0xe0: // call_vs
      if ( !read_var_ops_store ( ins, mem, &addr, err ) ) return false;
      if ( !ins_call ( ins, mem, err ) ) return false;
      break;

    case 0xf9: // call_vn
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, err ) ) return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xff: // check_arg_count
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, err ) ) return false;
          if ( !ins_var_1op ( ins, INSTRUCTION_NAME_CHECK_ARG_COUNT, err ) )
            return false;
          if ( !read_branch ( ins, mem, &addr, err ) ) return false;
        }
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
  ins->branch= false;

  // Decodifica.
  if ( !decode_next_inst ( ins, mem, err ) )
    return false;
  
  return true;
  
} // end instruction_disassemble
