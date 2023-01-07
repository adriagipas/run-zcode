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
    case INSTRUCTION_NAME_CALL: return "call    ";
    case INSTRUCTION_NAME_UNK :
    default                   : return "unknown ";
    }
  
} // end get_inst_name


static void
print_inst_op (
               const InstructionOp *op
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
  

  self= DEBUG_TRACER(self_);
  if ( (self->flags&DEBUG_TRACER_FLAGS_CPU) == 0  ) return;
  
  printf ( "[CPU]  ADDR: %08X  ", ins->addr );
  for ( n= 0; n < ins->nbytes; ++n ) printf ( " %02X", ins->bytes[n] );
  for ( ; n < 23; ++n ) printf ( "   " );
  printf ( "%s", get_inst_name ( ins->name ) );
  if ( ins->nops > 0 )
    {
      print_inst_op ( &(ins->ops[0]) );
      for ( n= 1; n < ins->nops; ++n )
        {
          putchar ( ',' );
          print_inst_op ( &(ins->ops[n]) );
        }
    }
  if ( ins->store )
    {
      printf ( " -->" );
      print_inst_op ( &(ins->store_op) );
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
  ret->flags= init_flags;

  return ret;
  
} // end debug_tracer_new
