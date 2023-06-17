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
              const bool        extra_byte,
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

  // Extra byte
  if ( extra_byte )
    {

      // Llig tipus
      if ( !memory_map_READB ( mem, *addr, &ops_type, true, err ) )
        return false;
      ++(*addr);
      ins->bytes[ins->nbytes++]= ops_type;
      
      // Processa tipus
      if ( !stop )
        {
          do {
            switch ( ops_type>>6 )
              {
              case 0:
                ins->ops[N++].type= INSTRUCTION_OP_TYPE_LARGE_CONSTANT;
                break;
              case 1:
                ins->ops[N++].type= INSTRUCTION_OP_TYPE_SMALL_CONSTANT;
                break;
              case 2: // Provisional
                ins->ops[N++].type= INSTRUCTION_OP_TYPE_TOP_STACK;
                break;
              case 3:
                ins->ops[N].type= INSTRUCTION_OP_TYPE_NONE;
                stop= true;
                break;
              }
            ops_type<<= 2;
          } while ( N < 8 && !stop );
        }
      
    }
  
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
                    const bool        extra_byte,
                    char            **err
                    )
{

  if ( !read_var_ops ( ins, mem, addr, extra_byte, err ) )
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
        ins->branch_op.u32=
          (uint32_t) -((int32_t) (16384 - ins->branch_op.u32));
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
op_to_ref (
           Instruction  *ins,
           const int     op,
           char        **err
           )
{

  if ( op >= ins->nops )
    {
      msgerror ( err,
                 "Failed to disassemble instruction: operand %d required",
                 op+1 );
      return false;
    }
  if ( ins->ops[op].type == INSTRUCTION_OP_TYPE_SMALL_CONSTANT )
      set_variable_type ( &(ins->ops[op]) );
  else
    {
      switch ( ins->ops[op].type )
        {
        case INSTRUCTION_OP_TYPE_TOP_STACK:
          ins->ops[op].type= INSTRUCTION_OP_TYPE_REF_TOP_STACK;
          break;
        case INSTRUCTION_OP_TYPE_LOCAL_VARIABLE:
          ins->ops[op].type= INSTRUCTION_OP_TYPE_REF_LOCAL_VARIABLE;
          break;
        case INSTRUCTION_OP_TYPE_GLOBAL_VARIABLE:
          ins->ops[op].type= INSTRUCTION_OP_TYPE_REF_GLOBAL_VARIABLE;
          break;
        default:
          msgerror ( err,
                     "Failed to disassemble instruction: operand %d is not a"
                     " valid reference to variable", op+1 );
          return false;
        }
    }
  
  return true;
  
} // end op_to_ref

  
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
      ins->bytes[ins->nbytes++]= (uint8_t) ins->ops[ins->nops].u8;
      set_variable_type ( &(ins->ops[ins->nops]) );
      ++(ins->nops);
      break;
    default:
      ee ( "disassembler.c - ins_2op - WTF!!" );
    }
  
  return true;
  
} // end ins_1op


static bool
ins_1op_store (
               Instruction            *ins,
               const MemoryMap        *mem,
               uint32_t               *addr,
               const InstructionName   name,
               char                  **err
               )
{

  if ( !ins_1op ( ins, mem, addr, name, err ) )
    return false;
  if ( !memory_map_READB ( mem, *addr, &(ins->store_op.u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->store_op.u8;
  set_variable_type ( &(ins->store_op) );
  ins->store= true;
  
  return true;
  
} // end ins_1op_store


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
ins_0op_store (
               Instruction            *ins,
               const MemoryMap        *mem,
               uint32_t               *addr,
               const InstructionName   name,
               char                  **err
               )
{

  ins->name= name;
  if ( !memory_map_READB ( mem, *addr, &(ins->store_op.u8), true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= ins->store_op.u8;
  set_variable_type ( &(ins->store_op) );
  ins->store= true;
  
  return true;
  
} // end ins_0op_store


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
  if ( ins->ops[0].type == INSTRUCTION_OP_TYPE_LARGE_CONSTANT )
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
inst_be (
         Instruction      *ins,
         const MemoryMap  *mem,
         uint32_t         *addr,
         char            **err
         )
{

  uint8_t opcode;
  
  
  // Opcode
  if ( !memory_map_READB ( mem, *addr, &opcode, true, err ) )
    return false;
  ++(*addr);
  ins->bytes[ins->nbytes++]= opcode;
  switch ( opcode )
    {
    case 0x00: // save
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_SAVE;
      break;
    case 0x01: // restore
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_RESTORE;
      break;
    case 0x02: // log_shift
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_LOG_SHIFT, err ) )
        return false;
      break;
    case 0x03: // art_shift
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_ART_SHIFT, err ) )
        return false;
      break;
    case 0x04: // set_font
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_SET_FONT;
      break;
      
    case 0x09: // save_undo
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_SAVE_UNDO;
      break;
    case 0x0a: // restore_undo
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_RESTORE_UNDO;
      break;
    case 0x0b: // print_unicode
      if ( !read_var_ops ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_PRINT_UNICODE;
      break;
    case 0x0c: // check_unicode
      if ( !read_var_ops_store ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_CHECK_UNICODE;
      break;
    case 0x0d: // set_true_colour
      if ( !read_var_ops ( ins, mem, addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_SET_TRUE_COLOUR;
      break;
      
    default: // Descodifica com UNK
      break;
    }
  
  return true;
  
} // end inst_be


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
    case 0x04: // dec_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_DEC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x05: // inc_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_INC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x06: // jin
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JIN, err ) )
        return false;
      break;
    case 0x07: // test
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_TEST, err ) )
        return false;
      break;
    case 0x08: // or
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_OR, err ) )
        return false;
      break;
    case 0x09: // and
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_AND, err ) )
        return false;
      break;
    case 0x0a: // test_attr
      if ( !ins_2op_branch ( ins, mem, &addr,
                             INSTRUCTION_NAME_TEST_ATTR, err ) )
        return false;
      break;
    case 0x0b: // set_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_ATTR, err ) )
        return false;
      break;
    case 0x0c: // clear_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CLEAR_ATTR, err ) )
        return false;
      break;
    case 0x0d: // store
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_STORE, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x0e: // insert_obj
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_INSERT_OBJ, err ) )
        return false;
      break;
    case 0x0f: // loadw
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADW, err ) )
        return false;
      break;
    case 0x10: // loadb
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADB, err ) )
        return false;
      break;
    case 0x11: // get_prop
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_GET_PROP, err ) )
        return false;
      break;
    case 0x12: // get_prop_addr
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_ADDR, err ) )
        return false;
      break;
    case 0x13: // get_next_prop
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_NEXT_PROP, err ) )
        return false;
      break;
    case 0x14: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;
    case 0x15: // sub
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_SUB, err ) )
        return false;
      break;
    case 0x16: // mul
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MUL, err ) )
        return false;
      break;
    case 0x17: // div
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_DIV, err ) )
        return false;
      break;
    case 0x18: // mod
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MOD, err ) )
        return false;
      break;
    case 0x19: // call_2s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x1a: // call_2n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x1b: // set_colour
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_COLOUR, err ) )
            return false;
        }
      break;
    case 0x1c: // throw
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_THROW, err ) )
            return false;
        }
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
    case 0x24: // dec_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_DEC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x25: // inc_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_INC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x26: // jin
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JIN, err ) )
        return false;
      break;
    case 0x27: // test
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_TEST, err ) )
        return false;
      break;
    case 0x28: // or
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_OR, err ) )
        return false;
      break;
    case 0x29: // and
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_AND, err ) )
        return false;
      break;
    case 0x2a: // test_attr
      if ( !ins_2op_branch ( ins, mem, &addr,
                             INSTRUCTION_NAME_TEST_ATTR, err ) )
        return false;
      break;
    case 0x2b: // set_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_ATTR, err ) )
        return false;
      break;
    case 0x2c: // clear_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CLEAR_ATTR, err ) )
        return false;
      break;
    case 0x2d: // store
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_STORE, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x2e: // insert_obj
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_INSERT_OBJ, err ) )
        return false;
      break;
    case 0x2f: // loadw
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADW, err ) )
        return false;
      break;
    case 0x30: // loadb
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADB, err ) )
        return false;
      break;
    case 0x31: // get_prop
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_GET_PROP, err ) )
        return false;
      break;
    case 0x32: // get_prop_addr
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_ADDR, err ) )
        return false;
      break;
    case 0x33: // get_next_prop
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_NEXT_PROP, err ) )
        return false;
      break;
    case 0x34: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;
    case 0x35: // sub
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_SUB, err ) )
        return false;
      break;
    case 0x36: // mul
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MUL, err ) )
        return false;
      break;
    case 0x37: // div
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_DIV, err ) )
        return false;
      break;
    case 0x38: // mod
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MOD, err ) )
        return false;
      break;
    case 0x39: // call_2s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x3a: // call_2n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x3b: // set_colour
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_COLOUR, err ) )
            return false;
        }
      break;
    case 0x3c: // throw
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_THROW, err ) )
            return false;
        }
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
    case 0x44: // dec_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_DEC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x45: // inc_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_INC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x46: // jin
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JIN, err ) )
        return false;
      break;
    case 0x47: // test
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_TEST, err ) )
        return false;
      break;
    case 0x48: // or
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_OR, err ) )
        return false;
      break;
    case 0x49: // and
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_AND, err ) )
        return false;
      break;
    case 0x4a: // test_attr
      if ( !ins_2op_branch ( ins, mem, &addr,
                             INSTRUCTION_NAME_TEST_ATTR, err ) )
        return false;
      break;
    case 0x4b: // set_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_ATTR, err ) )
        return false;
      break;
    case 0x4c: // clear_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CLEAR_ATTR, err ) )
        return false;
      break;
    case 0x4d: // store
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_STORE, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x4e: // insert_obj
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_INSERT_OBJ, err ) )
        return false;
      break;
    case 0x4f: // loadw
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADW, err ) )
        return false;
      break;
    case 0x50: // loadb
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADB, err ) )
        return false;
      break;
    case 0x51: // get_prop
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_GET_PROP, err ) )
        return false;
      break;
    case 0x52: // get_prop_addr
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_ADDR, err ) )
        return false;
      break;
    case 0x53: // get_next_prop
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_NEXT_PROP, err ) )
        return false;
      break;
    case 0x54: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;
    case 0x55: // sub
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_SUB, err ) )
        return false;
      break;
    case 0x56: // mul
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MUL, err ) )
        return false;
      break;
    case 0x57: // div
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_DIV, err ) )
        return false;
      break;
    case 0x58: // mod
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MOD, err ) )
        return false;
      break;
    case 0x59: // call_2s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x5a: // call_2n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x5b: // set_colour
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_COLOUR, err ) )
            return false;
        }
      break;
    case 0x5c: // throw
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_THROW, err ) )
            return false;
        }
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
    case 0x64: // dec_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_DEC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x65: // inc_chk
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_INC_CHK, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x66: // jin
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JIN, err ) )
        return false;
      break;
    case 0x67: // test
      if ( !ins_2op_branch ( ins, mem, &addr, INSTRUCTION_NAME_TEST, err ) )
        return false;
      break;
    case 0x68: // or
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_OR, err ) )
        return false;
      break;
    case 0x69: // and
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_AND, err ) )
        return false;
      break;
    case 0x6a: // test_attr
      if ( !ins_2op_branch ( ins, mem, &addr,
                             INSTRUCTION_NAME_TEST_ATTR, err ) )
        return false;
      break;
    case 0x6b: // set_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_ATTR, err ) )
        return false;
      break;
    case 0x6c: // clear_attr
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CLEAR_ATTR, err ) )
        return false;
      break;
    case 0x6d: // store
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_STORE, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x6e: // insert_obj
      if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_INSERT_OBJ, err ) )
        return false;
      break;
    case 0x6f: // loadw
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADW, err ) )
        return false;
      break;
    case 0x70: // loadb
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOADB, err ) )
        return false;
      break;
    case 0x71: // get_prop
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_GET_PROP, err ) )
        return false;
      break;
    case 0x72: // get_prop_addr
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_ADDR, err ) )
        return false;
      break;
    case 0x73: // get_next_prop
      if ( !ins_2op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_NEXT_PROP, err ) )
        return false;
      break;
    case 0x74: // add
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_ADD, err ) )
        return false;
      break;
    case 0x75: // sub
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_SUB, err ) )
        return false;
      break;
    case 0x76: // mul
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MUL, err ) )
        return false;
      break;
    case 0x77: // div
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_DIV, err ) )
        return false;
      break;
    case 0x78: // mod
      if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_MOD, err ) )
        return false;
      break;
    case 0x79: // call_2s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_2op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x7a: // call_2n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x7b: // set_colour
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_SET_COLOUR, err ) )
            return false;
        }
      break;
    case 0x7c: // throw
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_2op ( ins, mem, &addr, INSTRUCTION_NAME_THROW, err ) )
            return false;
        }
      break;
      
    case 0x80: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;
    case 0x81: // get_sibling
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_SIBLING, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0x82: // get_child
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_CHILD, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0x83: // get_parent
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PARENT, err ) )
        return false;
      break;
    case 0x84: // get_prop_len
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_LEN, err ) )
        return false;
      break;
    case 0x85: // inc
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_INC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x86: // dec
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_DEC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x87: // print_addr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_ADDR, err ) )
        return false;
      break;
    case 0x88: // call_1s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x89: // remove_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_REMOVE_OBJ, err ) )
        return false;
      break;
    case 0x8a: // print_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_OBJ, err ) )
        return false;
      break;
    case 0x8b: // ret
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_RET, err ) )
        return false;
      break;
    case 0x8c: // jump
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_JUMP, err ) )
        return false;
      break;
    case 0x8d: // print_paddr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_PADDR, err ) )
        return false;
      break;
    case 0x8e: // load
      if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOAD, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x8f: // call_1n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      else
        {
          // TODO !!
        }
      break;
    case 0x90: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;
    case 0x91: // get_sibling
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_SIBLING, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0x92: // get_child
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_CHILD, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0x93: // get_parent
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PARENT, err ) )
        return false;
      break;
    case 0x94: // get_prop_len
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_LEN, err ) )
        return false;
      break;
    case 0x95: // inc
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_INC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x96: // dec
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_DEC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x97: // print_addr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_ADDR, err ) )
        return false;
      break;
    case 0x98: // call_1s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0x99: // remove_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_REMOVE_OBJ, err ) )
        return false;
      break;
    case 0x9a: // print_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_OBJ, err ) )
        return false;
      break;
    case 0x9b: // ret
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_RET, err ) )
        return false;
      break;
    case 0x9c: // jump
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_JUMP, err ) )
        return false;
      break;
    case 0x9d: // print_paddr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_PADDR, err ) )
        return false;
      break;
    case 0x9e: // load
      if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOAD, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0x9f: // call_1n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      else
        {
          // TODO !!
        }
      break;
    case 0xa0: // jz
      if ( !ins_1op_branch ( ins, mem, &addr, INSTRUCTION_NAME_JZ, err ) )
        return false;
      break;
    case 0xa1: // get_sibling
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_SIBLING, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xa2: // get_child
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_CHILD, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xa3: // get_parent
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PARENT, err ) )
        return false;
      break;
    case 0xa4: // get_prop_len
      if ( !ins_1op_store ( ins, mem, &addr,
                            INSTRUCTION_NAME_GET_PROP_LEN, err ) )
        return false;
      break;
    case 0xa5: // inc
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_INC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0xa6: // dec
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_DEC, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0xa7: // print_addr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_ADDR, err ) )
        return false;
      break;
    case 0xa8: // call_1s
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      break;
    case 0xa9: // remove_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_REMOVE_OBJ, err ) )
        return false;
      break;
    case 0xaa: // print_obj
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_OBJ, err ) )
        return false;
      break;
    case 0xab: // ret
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_RET, err ) )
        return false;
      break;
    case 0xac: // jump
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_JUMP, err ) )
        return false;
      break;
    case 0xad: // print_paddr
      if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_PRINT_PADDR, err ) )
        return false;
      break;
    case 0xae: // load
      if ( !ins_1op_store ( ins, mem, &addr, INSTRUCTION_NAME_LOAD, err ) )
        return false;
      if ( !op_to_ref ( ins, 0, err ) ) return false;
      break;
    case 0xaf: // call_1n
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_1op ( ins, mem, &addr, INSTRUCTION_NAME_CALL, err ) )
            return false;
        }
      else
        {
          // TODO !!
        }
      break;
    case 0xb0: // rtrue
      ins->name= INSTRUCTION_NAME_RTRUE;
      break;
    case 0xb1: // rfalse
      ins->name= INSTRUCTION_NAME_RFALSE;
      break;
    case 0xb2: // print
      ins->name= INSTRUCTION_NAME_PRINT;
      break;
    case 0xb3: // print_ret
      ins->name= INSTRUCTION_NAME_PRINT_RET;
      break;
    case 0xb4: // nop
      ins->name= INSTRUCTION_NAME_NOP;
      break;
    case 0xb5: // save
      if ( mem->sf_mem[0] < 4 )
        {
          ins->name= INSTRUCTION_NAME_SAVE;
          if ( !read_branch ( ins, mem, &addr, err ) ) return false;
        }
      else if ( mem->sf_mem[0] == 4 )
        {
          if ( !ins_0op_store ( ins, mem, &addr, INSTRUCTION_NAME_SAVE, err ) )
            return false;
        }
      break;
    case 0xb6: // restore
      if ( mem->sf_mem[0] < 4 )
        {
          ins->name= INSTRUCTION_NAME_RESTORE;
          if ( !read_branch ( ins, mem, &addr, err ) ) return false;
        }
      else if ( mem->sf_mem[0] == 4 )
        {
          if ( !ins_0op_store ( ins, mem, &addr,
                                INSTRUCTION_NAME_RESTORE, err ) )
            return false;
        }
      break;
      
    case 0xb8: // ret_popped;
      ins->name= INSTRUCTION_NAME_RET_POPPED;
      break;
    case 0xb9: // catch
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !ins_0op_store ( ins, mem, &addr, INSTRUCTION_NAME_CATCH, err ) )
            return false;
        }
      break;
    case 0xba: // quit;
      ins->name= INSTRUCTION_NAME_QUIT;
      break;
    case 0xbb: // new_line
      ins->name= INSTRUCTION_NAME_NEW_LINE;
      break;
    case 0xbc: // show_status
      if ( mem->sf_mem[0] >= 3 )
        ins->name= INSTRUCTION_NAME_SHOW_STATUS;
      break;

    case 0xbe: // extended
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !inst_be ( ins, mem, &addr, err ) ) return false;
        }
      break;
      
    case 0xc1: // je
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_JE;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc2: // jl
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_JE;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc3: // jg
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_JG;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc4: // dec_chk
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_DEC_CHK, err ) ) return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc5: // inc_chk
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_INC_CHK, err ) ) return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc6: // jin
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_JIN, err ) ) return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc7: // test
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_TEST, err ) ) return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xc8: // or
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_OR, err ) ) return false;
      break;
    case 0xc9: // and
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_AND, err ) ) return false;
      break;
    case 0xca: // test_attr
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_TEST_ATTR, err ) )
        return false;
      if ( !read_branch ( ins, mem, &addr, err ) ) return false;
      break;
    case 0xcb: // set_attr
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_SET_ATTR, err ) )
        return false;
      break;
    case 0xcc: // clear_attr
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_CLEAR_ATTR, err ) )
        return false;
      break;
    case 0xcd: // store
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_STORE, err ) ) return false;
      break;
    case 0xce: // insert_obj
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_INSERT_OBJ, err ) )
        return false;
      break;
    case 0xcf: // loadw
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_LOADW, err ) ) return false;
      break;
    case 0xd0: // loadb
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_LOADB, err ) ) return false;
      break;
    case 0xd1: // get_prop
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_GET_PROP, err ) ) return false;
      break;
    case 0xd2: // get_prop_addr
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_GET_PROP_ADDR, err ) )
        return false;
      break;
    case 0xd3: // get_next_prop
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_GET_NEXT_PROP, err ) )
        return false;
      break;
    case 0xd4: // add
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_ADD, err ) ) return false;
      break;
    case 0xd5: // sub
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_SUB, err ) ) return false;
      break;
    case 0xd6: // mul
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_MUL, err ) ) return false;
      break;
    case 0xd7: // div
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_DIV, err ) ) return false;
      break;
    case 0xd8: // mod
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_MOD, err ) ) return false;
      break;
    case 0xd9: // call_2s NOTA!! No cal comprovar que han de ser 2
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xda: // call_2n NOTA!! No cal comprovar que han de ser 2
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xdb: // set_colour
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          ins->name= INSTRUCTION_NAME_SET_COLOUR;
        }
      break;
    case 0xdc: // throw
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          if ( !ins_var_2ops ( ins, INSTRUCTION_NAME_THROW, err ) )
            return false;
        }
      break;
      
    case 0xe0: // call_vs
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_call ( ins, mem, err ) ) return false;
      break;
    case 0xe1: // storew
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_STOREW;
      break;
    case 0xe2: // storeb
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_STOREB;
      break;
    case 0xe3: // put_prop
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_PUT_PROP;
      break;
    case 0xe4: // read
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          ins->name= INSTRUCTION_NAME_READ;
        }
      else
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) )
            return false;
          ins->name= INSTRUCTION_NAME_READ;
        }
      break;
    case 0xe5: // print_char
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_PRINT_CHAR;
      break;
    case 0xe6: // print_num
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_PRINT_NUM;
      break;
    case 0xe7: // random
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
        return false;
      ins->name= INSTRUCTION_NAME_RANDOM;
      break;
    case 0xe8: // push
      if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
      ins->name= INSTRUCTION_NAME_PUSH;
      break;
    case 0xe9: // pull
      if ( mem->sf_mem[0] == 6 )
        {
        }
      else
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          if ( !op_to_ref ( ins, 0, err ) ) return false;
          ins->name= INSTRUCTION_NAME_PULL;
        }
      break;
    case 0xea: // split_window
      if ( mem->sf_mem[0] >= 3 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_SPLIT_WINDOW;
        }
      break;
    case 0xeb: // set_window
      if ( mem->sf_mem[0] >= 3 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_SET_WINDOW;
        }
      break;
    case 0xec: // call_vs2
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, true, err ) )
            return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xed: // erase_window
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_ERASE_WINDOW;
        }
      break;

    case 0xef: // set_cursor
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_SET_CURSOR;
        }
      break;
      
    case 0xf1: // set_text_style
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          if ( !ins_var_1op ( ins, INSTRUCTION_NAME_SET_TEXT_STYLE, err ) )
            return false;
        }
      break;
    case 0xf2: // buffer_mode
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          if ( !ins_var_1op ( ins, INSTRUCTION_NAME_BUFFER_MODE, err ) )
            return false;
        }
      break;
    case 0xf3: // output_stream
      if ( mem->sf_mem[0] >= 3 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_OUTPUT_STREAM;
        }
      break;

    case 0xf6: // read_char
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          ins->name= INSTRUCTION_NAME_READ_CHAR;
        }
      break;
    case 0xf7: // scan_table
      if ( mem->sf_mem[0] >= 4 )
        {
          if ( !read_var_ops_store ( ins, mem, &addr, false, err ) )
            return false;
          ins->name= INSTRUCTION_NAME_SCAN_TABLE;
          if ( !read_branch ( ins, mem, &addr, err ) ) return false;
        }
      break;
    case 0xf8: // not
      if ( !read_var_ops_store ( ins, mem, &addr, false, err ) ) return false;
      if ( !ins_var_1op ( ins, INSTRUCTION_NAME_NOT, err ) ) return false;
      break;
    case 0xf9: // call_vn
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xfa: // call_vn2
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, true, err ) ) return false;
          if ( !ins_call ( ins, mem, err ) ) return false;
        }
      break;
    case 0xfb: // tokenise
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_TOKENISE;
        }
      break;

    case 0xfd: // copy_table
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_COPY_TABLE;
        }
      break;
    case 0xfe: // print_table
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
          ins->name= INSTRUCTION_NAME_PRINT_TABLE;
        }
      break;
    case 0xff: // check_arg_count
      if ( mem->sf_mem[0] >= 5 )
        {
          if ( !read_var_ops ( ins, mem, &addr, false, err ) ) return false;
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
