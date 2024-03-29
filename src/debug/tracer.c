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

static void
print_cc (
          const DebugTracer *tracer
          )
{
  if ( tracer->flags&DEBUG_TRACER_FLAGS_PRINT_CC )
  printf ( "CC:%016lu  ", tracer->cc-1 );
} // end princ_cc


static const char *
get_inst_name (
               const InstructionName name
               )
{

  switch ( name )
    {
    case INSTRUCTION_NAME_ADD             : return "add             ";
    case INSTRUCTION_NAME_AND             : return "and             ";
    case INSTRUCTION_NAME_ART_SHIFT       : return "art_shift       ";
    case INSTRUCTION_NAME_BUFFER_MODE     : return "buffer_mode     ";
    case INSTRUCTION_NAME_CALL            : return "call            ";
    case INSTRUCTION_NAME_CATCH           : return "catch           ";
    case INSTRUCTION_NAME_CHECK_ARG_COUNT : return "check_arg_count ";
    case INSTRUCTION_NAME_CHECK_UNICODE   : return "check_unicode   ";
    case INSTRUCTION_NAME_CLEAR_ATTR      : return "clear_attr      ";
    case INSTRUCTION_NAME_COPY_TABLE      : return "copy_table      ";
    case INSTRUCTION_NAME_DEC             : return "dec             ";
    case INSTRUCTION_NAME_DEC_CHK         : return "dec_chk         ";
    case INSTRUCTION_NAME_DIV             : return "div             ";
    case INSTRUCTION_NAME_ERASE_WINDOW    : return "erase_window    ";
    case INSTRUCTION_NAME_GET_CHILD       : return "get_child       ";
    case INSTRUCTION_NAME_GET_NEXT_PROP   : return "get_next_prop   ";
    case INSTRUCTION_NAME_GET_PARENT      : return "get_parent      ";
    case INSTRUCTION_NAME_GET_PROP        : return "get_prop        ";
    case INSTRUCTION_NAME_GET_PROP_ADDR   : return "get_prop_addr   ";
    case INSTRUCTION_NAME_GET_PROP_LEN    : return "get_prop_len    ";
    case INSTRUCTION_NAME_GET_SIBLING     : return "get_sibling     ";
    case INSTRUCTION_NAME_INC             : return "inc             ";
    case INSTRUCTION_NAME_INC_CHK         : return "inc_chk         ";
    case INSTRUCTION_NAME_INSERT_OBJ      : return "insert_obj      ";
    case INSTRUCTION_NAME_JE              : return "je              ";
    case INSTRUCTION_NAME_JG              : return "jg              ";
    case INSTRUCTION_NAME_JIN             : return "jin             ";
    case INSTRUCTION_NAME_JL              : return "jl              ";
    case INSTRUCTION_NAME_JUMP            : return "jump            ";
    case INSTRUCTION_NAME_JZ              : return "jz              ";
    case INSTRUCTION_NAME_LOAD            : return "load            ";
    case INSTRUCTION_NAME_LOADB           : return "loadb           ";
    case INSTRUCTION_NAME_LOADW           : return "loadw           ";
    case INSTRUCTION_NAME_LOG_SHIFT       : return "log_shift       ";
    case INSTRUCTION_NAME_MOD             : return "mod             ";
    case INSTRUCTION_NAME_MUL             : return "mul             ";
    case INSTRUCTION_NAME_NEW_LINE        : return "new_line        ";
    case INSTRUCTION_NAME_NOP             : return "nop             ";
    case INSTRUCTION_NAME_NOT             : return "not             ";
    case INSTRUCTION_NAME_OR              : return "or              ";
    case INSTRUCTION_NAME_OUTPUT_STREAM   : return "output_stream   ";
    case INSTRUCTION_NAME_PRINT           : return "print           ";
    case INSTRUCTION_NAME_PRINT_ADDR      : return "print_addr      ";
    case INSTRUCTION_NAME_PRINT_CHAR      : return "print_char      ";
    case INSTRUCTION_NAME_PRINT_NUM       : return "print_num       ";
    case INSTRUCTION_NAME_PRINT_OBJ       : return "print_obj       ";
    case INSTRUCTION_NAME_PRINT_PADDR     : return "print_paddr     ";
    case INSTRUCTION_NAME_PRINT_RET       : return "print_ret       ";
    case INSTRUCTION_NAME_PRINT_TABLE     : return "print_table     ";
    case INSTRUCTION_NAME_PRINT_UNICODE   : return "print_unicode   ";
    case INSTRUCTION_NAME_PULL            : return "pull            ";
    case INSTRUCTION_NAME_PUSH            : return "push            ";
    case INSTRUCTION_NAME_PUT_PROP        : return "put_prop        ";
    case INSTRUCTION_NAME_RANDOM          : return "random          ";
    case INSTRUCTION_NAME_QUIT            : return "quit            ";
    case INSTRUCTION_NAME_READ            : return "read            ";
    case INSTRUCTION_NAME_READ_CHAR       : return "read_char       ";
    case INSTRUCTION_NAME_REMOVE_OBJ      : return "remove_obj      ";
    case INSTRUCTION_NAME_RESTART         : return "restart         ";
    case INSTRUCTION_NAME_RESTORE         : return "restore         ";
    case INSTRUCTION_NAME_RESTORE_UNDO    : return "restore_undo    ";
    case INSTRUCTION_NAME_RET             : return "ret             ";
    case INSTRUCTION_NAME_RET_POPPED      : return "ret_popped      ";
    case INSTRUCTION_NAME_RFALSE          : return "rfalse          ";
    case INSTRUCTION_NAME_RTRUE           : return "rtrue           ";
    case INSTRUCTION_NAME_SAVE            : return "save            ";
    case INSTRUCTION_NAME_SAVE_UNDO       : return "save_undo       ";
    case INSTRUCTION_NAME_SCAN_TABLE      : return "scan_table      ";
    case INSTRUCTION_NAME_SET_ATTR        : return "set_attr        ";
    case INSTRUCTION_NAME_SET_COLOUR      : return "set_colour      ";
    case INSTRUCTION_NAME_SET_CURSOR      : return "set_cursor      ";
    case INSTRUCTION_NAME_SET_FONT        : return "set_font        ";
    case INSTRUCTION_NAME_SET_TEXT_STYLE  : return "set_text_style  ";
    case INSTRUCTION_NAME_SET_TRUE_COLOUR : return "set_true_colour ";
    case INSTRUCTION_NAME_SET_WINDOW      : return "set_window      ";
    case INSTRUCTION_NAME_SHOW_STATUS     : return "show_status     ";
    case INSTRUCTION_NAME_SPLIT_WINDOW    : return "split_window    ";
    case INSTRUCTION_NAME_STORE           : return "store           ";
    case INSTRUCTION_NAME_STOREB          : return "storeb          ";
    case INSTRUCTION_NAME_STOREW          : return "storew          ";
    case INSTRUCTION_NAME_SUB             : return "sub             ";
    case INSTRUCTION_NAME_TEST            : return "test            ";
    case INSTRUCTION_NAME_TEST_ATTR       : return "test_attr       ";
    case INSTRUCTION_NAME_THROW           : return "throw           ";
    case INSTRUCTION_NAME_TOKENISE        : return "tokenise        ";
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
    case INSTRUCTION_OP_TYPE_REF_TOP_STACK:
      printf ( "(st)" );
      break;
    case INSTRUCTION_OP_TYPE_REF_LOCAL_VARIABLE:
      printf ( "(l%u)", op->u8 );
      break;
    case INSTRUCTION_OP_TYPE_REF_GLOBAL_VARIABLE:
      printf ( "(g%u)", op->u8 );
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
  ++(self->cc);
  if ( (self->flags&DEBUG_TRACER_FLAGS_CPU) == 0  ) return;

  next_addr= ins->addr + ((uint32_t) ins->nbytes);

  print_cc ( self );
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

  print_cc ( self );
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

  print_cc ( self );
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
  ret->cc= 0;

  return ret;
  
} // end debug_tracer_new
