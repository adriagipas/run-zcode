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

#include "disassembler.h"
#include "interpreter.h"
#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define RET_CONTINUE 0
#define RET_STOP     1
#define RET_ERROR    -1

#define SET_U8TOU16(SRC,DST)                                            \
  {                                                                     \
    (DST)= (uint16_t) (SRC);                                            \
    if ( (SRC) >= 0x80 )                                                \
      ww ( "interpreter.c - %d - CAL COMPROVAR SI ESTÀ BÉ EL SIGNE!",   \
           __LINE__ );                                                  \
  }




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
  if ( var <= 0x0f ) // Pila o variables locals
    {
      if ( !state_readvar ( state, var, val, err ) )
        return false;
    }
  else // Variables globals
    *val= memory_map_readvar ( intp->mem, (int) ((uint32_t) (var-0x10)) );
  
  return true;
  
} // end read_var


static bool
write_var (
           Interpreter     *intp,
           const uint8_t    var,
           const uint16_t   val,
           char           **err
          )
{

  State *state;
  
  
  state= intp->state;
  if ( var <= 0x0f ) // Pila o variables locals
    {
      if ( !state_writevar ( state, var, val, err ) )
        return false;
    }
  else // Variables globals
    memory_map_writevar ( intp->mem, (int) ((uint32_t) (var-0x10)), val );
  
  return true;
  
} // end write_var


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
          SET_U8TOU16(ops[i].u8.val,local_vars[i-1]);
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
  if ( !state_new_frame ( intp->state, addr, num_local_vars,
                          discard_result, result_var, args_mask, err ) )
    return false;
  for ( n= 0; n < num_local_vars; ++n )
    if ( !state_writevar ( intp->state, n+1, local_vars[n], err ) )
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


static bool
read_small_small (
                  Interpreter  *intp,
                  uint8_t      *op1,
                  uint8_t      *op2,
                  char        **err
                  )
{

  if ( !memory_map_READB ( intp->mem, intp->state->PC++, op1, true, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++, op2, true, err ) )
    return false;

  return true;
  
} // end read_small_small


static bool
read_small_var (
                Interpreter  *intp,
                uint8_t      *op1,
                uint16_t     *op2,
                char        **err
                )
{

  uint8_t tmp2;

  
  if ( !read_small_small ( intp, op1, &tmp2, err ) )
    return false;
  if ( !read_var ( intp, tmp2, op2, err ) )
    return false;
  
  return true;
  
} // end read_small_var


static bool
read_var_small (
                Interpreter  *intp,
                uint16_t     *op1,
                uint8_t      *op2,
                char        **err
                )
{

  uint8_t tmp1;

  
  if ( !read_small_small ( intp, &tmp1, op2, err ) )
    return false;
  if ( !read_var ( intp, tmp1, op1, err ) )
    return false;
  
  return true;
  
} // end read_var_small


static bool
read_var_var (
              Interpreter  *intp,
              uint16_t     *op1,
              uint16_t     *op2,
              char        **err
              )
{

  uint8_t tmp1,tmp2;

  
  if ( !read_small_small ( intp, &tmp1, &tmp2, err ) )
    return false;
  if ( !read_var ( intp, tmp1, op1, err ) )
    return false;
  if ( !read_var ( intp, tmp2, op2, err ) )
    return false;
  
  return true;
  
} // end read_var_var


static bool
read_var_ops_store (
                    Interpreter  *intp,
                    operand_t    *ops,
                    int          *nops,
                    uint8_t      *store_var,
                    char        **err
                    )
{

  if ( !read_var_ops ( intp, ops, nops, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;

  return true;
  
} // end read_var_ops_store


static bool
op_to_u16 (
           Interpreter      *intp,
           const operand_t  *op,
           uint16_t         *ret,
           char            **err
           )
{

  switch ( op->type )
    {
    case OP_LARGE: *ret= op->u16.val; break;
    case OP_SMALL: SET_U8TOU16(op->u8.val,*ret); break;
    case OP_VARIABLE:
      if ( !read_var ( intp, op->u8.val, ret, err ) )
        return false;
      break;
    default:
      ee ( "interpreter.c - op_to_u16 - WTF!!" );
    }

  return true;
  
} // end op_to_u16


static bool
ops_to_u16_u16 (
                Interpreter      *intp,
                const operand_t  *ops,
                const int         nops,
                uint16_t         *op1,
                uint16_t         *op2,
                char            **err
                )
{

  if ( nops != 2 )
    {
      msgerror ( err, "Failed to execute 2OP instruction in VAR format:"
                 " %d operands supplied", nops );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), op1, err ) )
    return false;
  if ( !op_to_u16 ( intp, &(ops[1]), op2, err ) )
    return false;

  return true;
  
} // end ops_to_u16_u16


static bool
branch (
        Interpreter  *intp,
        const bool    cond,
        char        **err
        )
{

  uint8_t b1,b2;
  uint32_t offset;
  State *state;
  bool cond_value;
  
  
  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &b1, true, err ) )
    return false;
  if ( (b1&0x40) == 0 )
    {
      if ( !memory_map_READB ( intp->mem, state->PC++, &b2, true, err ) )
        return false;
      // 14bits amb signe
      offset= (((uint32_t) (b1&0x3F))<<8) | ((uint32_t) b2);
      if ( offset&0x2000 )
        offset= ((uint32_t) -((int32_t) offset));
    }
  else offset= (uint32_t) (b1&0x3F);
  cond_value= ((b1&0x80)!=0); // True si bit7 està actiu.

  // Bot
  if ( cond == cond_value )
    {
      if ( offset == 0 )
        ee ( "CAL_IMPLEMENTAR return false" );
      else if ( offset == 1 )
        ee ( "CAL_IMPLEMENTAR return true" );
      else
        state->PC+= offset-2;
    }
  
  return true;
  
} // end branch;


static int
exec_next_inst (
                Interpreter  *intp,
                char        **err
                )
{

  int nops;
  uint8_t opcode,result_var,op1_u8,op2_u8;
  uint16_t op1,op2,res;
  State *state;
  operand_t ops[8];
  

  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &opcode, true, err ) )
    return RET_ERROR;
  switch ( opcode )
    {
      
    case 0x02: // jl
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, ((int16_t) op1) < ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0x03: // jg
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, ((int16_t) op1) > ((int16_t) op2), err ) )
        return RET_ERROR;
      break;

    case 0x22: // jl
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !branch ( intp, ((int16_t) op1) < ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0x23: // jg
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !branch ( intp, ((int16_t) op1) > ((int16_t) op2), err ) )
        return RET_ERROR;
      break;

    case 0x42: // jl
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, ((int16_t) op1) < ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0x43: // jg
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, ((int16_t) op1) > ((int16_t) op2), err ) )
        return RET_ERROR;
      break;

    case 0x62: // jl
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, ((int16_t) op1) < ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0x63: // jg
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, ((int16_t) op1) > ((int16_t) op2), err ) )
        return RET_ERROR;
      break;

    case 0xd5: // sub
      if ( !read_var_ops_store ( intp, ops, &nops, &result_var, err ) )
        return RET_ERROR;
      if ( !ops_to_u16_u16 ( intp, ops, nops, &op1, &op2, err ) )
        return RET_ERROR;
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
      
    case 0xe0: // call_vs
      if ( !read_var_ops_store ( intp, ops, &nops, &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, result_var, false, err ) )
        return RET_ERROR;
      break;

    case 0xf9: // call_vn
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, 0x00, true, err ) )
        return RET_ERROR;
      break;
      
    default: // Unknown
      msgerror ( err, "Unknown instruction opcode %02X (%d)", opcode, opcode );
      return RET_ERROR;
    }

  return RET_CONTINUE;

 wrong_version:
  msgerror ( err, "Instruction opcode %02X (%d) is not supported in version %d",
             opcode, opcode, intp->version );
  return RET_ERROR;
  
} // end exec_next_inst




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
interpreter_free (
                  Interpreter *intp
                  )
{

  if ( intp->ins != NULL ) instruction_free ( intp->ins );
  if ( intp->mem != NULL ) memory_map_free ( intp->mem );
  if ( intp->sf != NULL ) story_file_free ( intp->sf );
  if ( intp->state != NULL ) state_free ( intp->state );
  g_free ( intp );
  
} // end interpreter_free


Interpreter *
interpreter_new_from_file_name (
                                const char  *file_name,
                                Tracer      *tracer,
                                char       **err
                                )
{

  Interpreter *ret;


  // Prepara.
  ret= g_new ( Interpreter, 1 );
  ret->sf= NULL;
  ret->state= NULL;
  ret->mem= NULL;
  ret->ins= NULL;
  ret->tracer= tracer;

  // Obri story file
  ret->sf= story_file_new_from_file_name ( file_name, err );
  if ( ret->sf == NULL ) goto error;

  // Crea estat.
  ret->state= state_new ( ret->sf, tracer, err );
  if ( ret->state == NULL ) goto error;

  // Inicialitza mapa de memòria.
  ret->mem= memory_map_new ( ret->sf, ret->state, tracer, err );
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


bool
interpreter_trace (
                   Interpreter          *intp,
                   const unsigned long   iters,
                   char                **err
                   )
{

  unsigned long i;
  int ret;
  
  
  // Prepara.
  if ( intp->ins == NULL )
    intp->ins= instruction_new ();

  // Itera
  ret= RET_CONTINUE;
  memory_map_enable_trace ( intp->mem, false );
  for ( i= 0; i < iters && ret == RET_CONTINUE; ++i )
    {

      // Descodifica instrucció
      if ( intp->tracer != NULL && intp->tracer->exec_inst != NULL )
        {
          if ( !instruction_disassemble ( intp->ins, intp->mem,
                                          intp->state->PC, err ) )
            return false;
          intp->tracer->exec_inst ( intp->tracer, intp->ins );
        }

      // Executa
      memory_map_enable_trace ( intp->mem, true );
      state_enable_trace ( intp->state, true );
      ret= exec_next_inst ( intp, err );
      memory_map_enable_trace ( intp->mem, false );
      state_enable_trace ( intp->state, false );
      if ( ret == RET_ERROR ) return false;
      
    }

  return true;
  
} // end interpreter_trace
