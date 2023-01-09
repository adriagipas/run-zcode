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
 *  tracer.c - Implementació de 'tracer.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "tracer.h"
#include "core/disassembler.h"




/**********/
/* MACROS */
/**********/

#define DEBUG_TRACER(PTR) ((DebugTracer *) (PTR))




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static const char *
get_inst_name (
               const InstructionName name
               )
{

  switch ( name )
    {
    case INSTRUCTION_NAME_ADD             : return "add             ";
    case INSTRUCTION_NAME_CALL            : return "call            ";
    case INSTRUCTION_NAME_CHECK_ARG_COUNT : return "check_arg_count ";
    case INSTRUCTION_NAME_JE              : return "je              ";
    case INSTRUCTION_NAME_JG              : return "jg              ";
    case INSTRUCTION_NAME_JL              : return "jl              ";
    case INSTRUCTION_NAME_SUB             : return "sub             ";
    case INSTRUCTION_NAME_UNK             :
    default                               : return "unknown         ";
    }
  
} // end get_inst_name


static void
print_inst_op (
               const InstructionOp *op,
               const uint32_t       next_addr
               )
{

  putchar ( ' ' );
  switch ( op->type )
    {
    case INSTRUCTION_OP_TYPE_TOP_STACK:
      printf ( "st" );
      break;
    case INSTRUCTION_OP_TYPE_LOCAL_VARIABLE:
      printf ( "l%u", op->u8 );
      break;
    case INSTRUCTION_OP_TYPE_GLOBAL_VARIABLE:
      printf ( "g%u", op->u8 );
      break;
    case INSTRUCTION_OP_TYPE_LARGE_CONSTANT:
      printf ( "%04Xh", op->u16 );
      break;
    case INSTRUCTION_OP_TYPE_SMALL_CONSTANT:
      printf ( "%02Xh", op->u8 );
      break;
    case INSTRUCTION_OP_TYPE_ROUTINE:
      printf ( "ROUTINE:%X [PADDR:%04X]", op->u32, op->u16 );
      break;
    case INSTRUCTION_OP_TYPE_BRANCH_IF_TRUE:
      printf ( "GOTO %08X (%d) IF true",
               next_addr+op->u32, (int32_t) op->u32 );
      break;
    case INSTRUCTION_OP_TYPE_BRANCH_IF_FALSE:
      printf ( "GOTO %08X (%d) IF false",
               next_addr+op->u32, (int32_t) op->u32 );
      break;
    case INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_TRUE:
      printf ( "RETURN true IF true" );
      break;
    case INSTRUCTION_OP_TYPE_RETURN_TRUE_IF_FALSE:
      printf ( "RETURN true IF false" );
      break;
    case INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_TRUE:
      printf ( "RETURN false IF true" );
      break;
    case INSTRUCTION_OP_TYPE_RETURN_FALSE_IF_FALSE:
      printf ( "RETURN false IF false" );
      break;
    default:
      printf ( "??? %d", op->type );
    }
  
} // end print_inst_op


static void
exec_inst (
           Tracer            *self_,
           const Instruction *ins
           )
{

  DebugTracer *self;
  int n;
  uint32_t next_addr;
  

  self= DEBUG_TRACER(self_);
  if ( (self->flags&DEBUG_TRACER_FLAGS_CPU) == 0  ) return;

  next_addr= ins->addr + ((uint32_t) ins->nbytes);
  
  printf ( "[CPU]  ADDR: %08X  ", ins->addr );
  for ( n= 0; n < ins->nbytes; ++n ) printf ( " %02X", ins->bytes[n] );
  for ( ; n < 23; ++n ) printf ( "   " );
  printf ( "%s", get_inst_name ( ins->name ) );
  if ( ins->nops > 0 )
    {
      print_inst_op ( &(ins->ops[0]), next_addr );
      for ( n= 1; n < ins->nops; ++n )
        {
          putchar ( ',' );
          print_inst_op ( &(ins->ops[n]), next_addr );
        }
    }
  if ( ins->store )
    {
      printf ( " -->" );
      print_inst_op ( &(ins->store_op), next_addr );
    }
  if ( ins->branch )
    {
      printf ( "   ?" );
      print_inst_op ( &(ins->branch_op), next_addr );
    }
  putchar ( '\n' );
  
} // end exec_inst


static void
mem_access (
            Tracer          *self_,
            const uint32_t   addr,
            const uint16_t   data,
            const MemAccess  type
            )
{

  DebugTracer *self;
  

  self= DEBUG_TRACER(self_);
  if ( (self->flags&DEBUG_TRACER_FLAGS_MEM) == 0  ) return;

  switch ( type )
    {
    case MEM_ACCESS_READB:
      printf ( "[MEM]        %08X   -->   %02X\n", addr, (uint8_t) data );
      break;
    case MEM_ACCESS_READW:
      printf ( "[MEM]        %08X   -->   %04X\n", addr, data );
      break;
    case MEM_ACCESS_WRITEB:
      printf ( "[MEM]        %08X   <--   %02X\n", addr, (uint8_t) data );
      break;
    case MEM_ACCESS_WRITEW:
      printf ( "[MEM]        %08X   <--   %04X\n", addr, data );
      break;
    case MEM_ACCESS_READVAR:
      printf ( "[MEM]        G%03d       -->   %04X\n", (int) addr, data );
      break;
    case MEM_ACCESS_WRITEVAR:
      printf ( "[MEM]        G%03d       <--   %04X\n", (int) addr, data );
      break;
    default: break;
    }
  
} // end mem_access


static void
stack_access (
              Tracer            *self_,
              const uint8_t      ind,
              const uint16_t     data,
              const StackAccess  type
            )
{

  DebugTracer *self;
  

  self= DEBUG_TRACER(self_);
  if ( (self->flags&DEBUG_TRACER_FLAGS_STACK) == 0  ) return;

  switch ( type )
    {
    case STACK_ACCESS_READ:
      if ( ind == 0 )
        printf ( "[STK]        ST         -->   %04X\n", data );
      else
        printf ( "[STK]        L%02d        -->   %04X\n", (int) ind-1, data );
      break;
    case STACK_ACCESS_WRITE:
      if ( ind == 0 )
        printf ( "[STK]        ST         <--   %04X\n", data );
      else
        printf ( "[STK]        L%02d        <--   %04X\n", (int) ind-1, data );
      break;
    default: break;
    }
  
} // end stack_access




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
debug_tracer_free (
                   DebugTracer *t
                   )
{
  g_free ( t );
} // end debug_tracer_free


DebugTracer *
debug_tracer_new (
                  const uint32_t init_flags
                  )
{

  DebugTracer *ret;


  ret= g_new ( DebugTracer, 1 );
  ret->exec_inst= exec_inst;
  ret->mem_access= mem_access;
  ret->stack_access= stack_access;
  ret->flags= init_flags;

  return ret;
  
} // end debug_tracer_new
