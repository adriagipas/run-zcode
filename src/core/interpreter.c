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
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define RET_CONTINUE 0
#define RET_STOP     1
#define RET_ERROR    -1




/*********/
/* TIPUS */
/*********/

// IMPORTANT!!! El valor coincideix amb els bits codificats.
typedef enum
  {
    OP_LARGE= 0,
    OP_SMALL= 1,
    OP_VARIABLE= 2,
    OP_NONE= 3
  } op_type_t;

typedef union
{
  op_type_t type;
  struct
  {
    op_type_t type;
    uint16_t  val;
  }         u16;
  struct
  {
    op_type_t type;
    uint8_t   val;
  }         u8;
} operand_t;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static uint32_t
unpack_addr (
             const Interpreter *intp,
             const uint16_t     paddr,
             const bool         is_call
             )
{

  uint32_t ret;

  
  if ( intp->version <= 3 )
    ret= ((uint32_t) paddr)<<1;
  else if ( intp->version <= 5 )
    ret= ((uint32_t) paddr)<<2;
  else if ( intp->version <= 7 )
    {
      ret= ((uint32_t) paddr)<<2;
      ret+= is_call ? intp->routine_offset : intp->static_strings_offset;
    }
  else
    {
      assert ( intp->version == 8 );
      ret= ((uint32_t) paddr)<<3;
    }

  return ret;
  
} // end unpack_addr


static bool
read_var (
          Interpreter    *intp,
          const uint8_t   var,
          uint16_t       *val,
          char          **err
          )
{

  State *state;
  

  state= intp->state;
  if ( var == 0x00 ) // Pila
    {
      if ( !state_stack_pop ( state, val, err ) )
        return false;
    }
  else if ( var <= 0x0f ) // Variables locals
    {
      if ( var-1 >= FRAME_NLOCAL(state) )
        {
          msgerror ( err, "Failed to read local variable: index %u"
                     " out of bounds [0,%u[", var-1, FRAME_NLOCAL(state) );
          return false;
        }
      *val= FRAME_LOCAL(state,var-1);
    }
  else // Variables globals
    *val= memory_map_readvar ( intp->mem, (int) ((uint32_t) (var-0x10)) );
  
  return true;
  
} // end read_var


// NOTA!! S'espera sempre que el primer argument siga la rutina però
// no s'ha comprovat res.
static bool
call_routine (
              Interpreter      *intp,
              const operand_t  *ops,
              const int         nops,
              const uint8_t     result_var,
              const bool        discard_result,
              char            **err
              )
{

  uint32_t addr;
  uint8_t num_local_vars,n,args_mask;
  uint16_t local_vars[15];
  int i;
  
  
  // Comprovacions
  if ( nops == 0 )
    {
      msgerror ( err, "Failed to call routine: missing routine argument" );
      return false;
    }
  if ( ops[0].type != OP_LARGE )
    {
      msgerror ( err, "Failed to call routine: invalid operand"
                 " type for routine argument" );
      return false;
    }

  // Descodifica rutina.
  // --> Adreça real
  addr= unpack_addr ( intp, ops[0].u16.val, true );
  // --> Nombre variables locals
  if ( !memory_map_READB ( intp->mem, addr++,
                           &num_local_vars, true, err ) )
    return false;
  if ( num_local_vars > 15 )
    {
      msgerror ( err, "Failed to call routine (PADDR: %X): "
                 "invalid number of local variables %u",
                 ops[0].u16.val, num_local_vars );
      return false;
    }
  if ( nops-1 > (int) num_local_vars )
    {
      msgerror ( err, "Failed to call routine (PADDR: %X): "
                 "supplied more arguments (%d) than local variables (%u)",
                 ops[0].u16.val, nops-1, num_local_vars );
      return false;
    }
  // --> Valors inicials
  if ( intp->version <= 4 )
    {
      for ( n= 0; n < num_local_vars; ++n )
        {
          if ( !memory_map_READW ( intp->mem, addr,
                                   &(local_vars[n]), true, err ) )
            return false;
          addr+= 2;
        }
    }
  else
    {
      for ( n= 0; n < num_local_vars; ++n )
        local_vars[n]= 0x0000;
    }

  // Assigna arguments
  args_mask= 0x00;
  for ( i= 1; i < nops; ++i )
    {
      args_mask|= 0x1<<(i-1);
      switch ( ops[i].type )
        {
        case OP_LARGE:
          local_vars[i-1]= ops[i].u16.val;
          break;
        case OP_SMALL:
          local_vars[i-1]= (uint16_t) ops[i].u8.val; // Amb o sense signe
          ww ( "interpreter.c - call_routine - CAL"
               " COMPROVAR SI ESTÀ BÉ EL SIGNE!" );
          break;
        case OP_VARIABLE:
          if ( !read_var ( intp, ops[i].u8.val, &(local_vars[i-1]), err ) )
            return false;
          break;
        case OP_NONE:
          ee ( "interpreter.c - call_routine - unexpected error" );
        }
    }

  // Crea nou frame
  if ( !state_new_frame ( intp->state, addr, local_vars, num_local_vars,
                          discard_result, result_var, args_mask, err ) )
    return false;

  
  return true;
  
} // end call_routine


static bool
read_var_ops (
              Interpreter  *intp,
              operand_t    *ops,
              int          *nops,
              char        **err
              )
{

  uint8_t ops_type;
  State *state;
  int n,N;

  
  state= intp->state;

  // Processa tipus
  if ( !memory_map_READB ( intp->mem, state->PC++, &ops_type, true, err ) )
    return false;
  N= 0;
  while ( N < 4 && (ops[N].type= (ops_type>>6)) != OP_NONE )
    {
      ++N;
      ops_type<<= 2;
    }
  *nops= N;

  // Llig valors
  for ( n= 0; n < N; ++n )
    if ( ops[n].type == OP_LARGE )
      {
        if ( !memory_map_READW ( intp->mem, state->PC,
                                 &(ops[n].u16.val), true, err ) )
          return false;
        state->PC+= 2;
      }
    else
      {
        if ( !memory_map_READB ( intp->mem, state->PC++,
                                 &(ops[n].u8.val), true, err ) )
          return false;
      }
  
  return true;
  
} // end read_var_ops


static int
exec_next_inst (
                Interpreter  *intp,
                char        **err
                )
{

  int nops;
  uint8_t opcode,result_var;
  State *state;
  operand_t ops[8];
  

  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &opcode, true, err ) )
    return RET_ERROR;
  switch ( opcode )
    {
      
    case 0xe0: // call_vs
      if ( !read_var_ops ( intp, ops, &nops, err ) ) return RET_ERROR;
      if ( !memory_map_READB ( intp->mem, state->PC++,
                               &result_var, true, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, result_var, false, err ) )
        return RET_ERROR;
      return RET_CONTINUE;
      break;
      
    default: // Unknown
      msgerror ( err, "Unknown instruction opcode %02X (%d)", opcode, opcode );
      return RET_ERROR;
    }
  
} // end exec_next_inst




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
interpreter_free (
                  Interpreter *intp
                  )
{

  if ( intp->mem != NULL ) memory_map_free ( intp->mem );
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
  ret->mem= NULL;

  // Obri story file
  ret->sf= story_file_new_from_file_name ( file_name, err );
  if ( ret->sf == NULL ) goto error;

  // Crea estat.
  ret->state= state_new ( ret->sf, err );
  if ( ret->state == NULL ) goto error;

  // Inicialitza mapa de memòria.
  ret->mem= memory_map_new ( ret->sf, ret->state, err );
  if ( ret == NULL ) goto error;

  // Altres
  ret->version= ret->mem->sf_mem[0];
  if ( ret->version >= 6 && ret->version <= 7 )
    {
      ret->routine_offset=
        (((uint32_t) ret->mem->sf_mem[0x28])<<8) |
        ((uint32_t) ret->mem->sf_mem[0x29])
        ;
      ret->static_strings_offset=
        (((uint32_t) ret->mem->sf_mem[0x2a])<<8) |
        ((uint32_t) ret->mem->sf_mem[0x2b])
        ;
    }
  
  return ret;

 error:
  interpreter_free ( ret );
  return NULL;
  
} // end interpreter_new_from_file_name


bool
interpreter_run (
                 Interpreter  *intp,
                 char        **err
                 )
{

  int ret;


  do {
    ret= exec_next_inst ( intp, err );
  } while ( ret == RET_CONTINUE );
  
  return ret==RET_ERROR ? false : true;
  
} // end interpreter_run
