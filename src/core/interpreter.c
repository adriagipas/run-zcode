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
#include <libintl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disassembler.h"
#include "interpreter.h"
#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define _(String) gettext (String)

#define RET_CONTINUE 0
#define RET_STOP     1
#define RET_ERROR    -1

#define SET_U8TOU16(SRC,DST)                                            \
  {                                                                     \
    (DST)= (uint16_t) (SRC);                                            \
  }

#define U16_S32(U16) ((int32_t) ((int16_t) (U16)))
#define S32_U16(S32) ((uint16_t) ((uint32_t) (S32)))

// ESPECIAL ZSCII CHARS
#define ZSCII_TAB     11
#define ZSCII_NEWLINE 13
#define ZSCII_DELETE  8

// 10 milisegons, de moment
#define TIME_SLEEP 10000

#define CURSOR "\u2588"




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




/*************/
/* CONSTANTS */
/*************/

// Alfabets estàndard.
static const char ZSCII_ENC[3][26]=
  {
    // A0
    {'a','b','c','d','e','f','g','h','i','j','k','l','m',
     'n','o','p','q','r','s','t','u','v','w','x','y','z'},
    // A1
    {'A','B','C','D','E','F','G','H','I','J','K','L','M',
     'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'},
    // A2
    {' ','\n','0','1','2','3','4','5','6','7','8','9','.',
     ',','!','?','_','#','\'','"','/','\\','-',':','(',')'}
  };

static const char ZSCII_ENC_V1[3][26]=
  {
    // A0
    {'a','b','c','d','e','f','g','h','i','j','k','l','m',
     'n','o','p','q','r','s','t','u','v','w','x','y','z'},
    // A1
    {'A','B','C','D','E','F','G','H','I','J','K','L','M',
     'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'},
    // A2
    {' ','0','1','2','3','4','5','6','7','8','9','.',',',
     '!','?','_','#','\'','"','/','\\','<','-',':','(',')'}
  };

static const int ZSCII_TO_UNICODE_SIZE= 69;

// 155-251
static const uint16_t ZSCII_TO_UNICODE[97]=
  {
    0x00e4, // ä
    0x00f6, // ö
    0x00fc, // ü
    0x00c4, // Ä
    0x00d6, // Ö
    0x00dc, // Ü
    0x00df, // ß
    0x00bb, // »
    0x00ab, // «
    0x00eb, // ë
    0x00ef, // ï
    0x00ff, // ÿ
    0x00cb, // Ë
    0x00cf, // Ï
    0x00e1, // á
    0x00e9, // é
    0x00ed, // í
    0x00f3, // ó
    0x00fa, // ú
    0x00fd, // ý
    0x00c1, // Á
    0x00c9, // É
    0x00cd, // Í
    0x00d3, // Ó
    0x00da, // Ú
    0x00dd, // Ý
    0x00e0, // à
    0x00e8, // è
    0x00ec, // ì
    0x00f2, // ò
    0x00f9, // ù
    0x00c0, // À
    0x00c8, // È
    0x00cc, // Ì
    0x00d2, // Ò
    0x00d9, // Ù
    0x00e2, // â
    0x00ea, // ê
    0x00ee, // î
    0x00f4, // ô
    0x00fb, // û
    0x00c2, // Â
    0x00ca, // Ê
    0x00ce, // Î
    0x00d4, // Ô
    0x00db, // Û
    0x00e5, // å
    0x00c5, // Å
    0x00f8, // ø
    0x00d8, // Ø
    0x00e3, // ã
    0x00f1, // ñ
    0x00f5, // õ
    0x00c3, // Ã
    0x00d1, // Ñ
    0x00d5, // Õ
    0x00e6, // æ
    0x00c6, // Æ
    0x00e7, // ç
    0x00c7, // Ç
    0x00fe, // þ
    0x00f0, // ð
    0x00de, // Þ
    0x00d0, // Ð
    0x00a3, // £
    0x0153, // œ
    0x0152, // Œ
    0x00a1, // ¡
    0x00bf, // ¿
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd, // Desconegut
    0xfffd  // Desconegut
  };




/************************/
/* DEFINICIONS PRIVADES */
/************************/

static int
exec_next_inst (
                Interpreter  *intp,
                char        **err
                );




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
random_reset (
              Interpreter *intp
              )
{

  intp->random.seed= 0;
  intp->random.current= 0;
  intp->random.mode= RAND_MODE_RANDOM;
  
} // end random_reset


static void
random_set_seed (
                 Interpreter    *intp,
                 const uint16_t  seed
                 )
{

  intp->random.seed= seed;
  if ( seed == 0 )
    intp->random.mode= RAND_MODE_RANDOM;
  else if ( seed >= 1000 )
    {
      srand ( (unsigned int) seed );
      intp->random.mode= RAND_MODE_PREDICTABLE2;
    }
  else
    {
      intp->random.current= 1;
      intp->random.mode= RAND_MODE_PREDICTABLE1;
    }
  
} // end random_set_seed


static uint16_t
random_next (
             Interpreter *intp
             )
{

  uint16_t ret;
  gint64 time;
  
  
  switch ( intp->random.mode )
    {
    case RAND_MODE_RANDOM:
      time= g_get_monotonic_time () / 1000;
      ret= (time%32767)+1;
      break;
    case RAND_MODE_PREDICTABLE1:
      ret= intp->random.current;
      if ( intp->random.current == intp->random.seed )
        intp->random.current= 1;
      else
        ++(intp->random.current);
      break;
    case RAND_MODE_PREDICTABLE2:
      ret= (uint16_t) ((rand ()%32767) + 1);
      break;
    default:
      ee ( "random_next - WTF!!!!" );
      ret= 0;
    }
  
  return ret;
  
} // end random_next


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
read_var_base (
               Interpreter    *intp,
               const uint8_t   var,
               uint16_t       *val,
               const bool      pop,
               char          **err
               )
{

  State *state;
  
  
  state= intp->state;
  if ( var <= 0x0f ) // Pila o variables locals
    {
      if ( !state_readvar ( state, var, val, pop, err ) )
        return false;
    }
  else // Variables globals
    *val= memory_map_readvar ( intp->mem, (int) ((uint32_t) (var-0x10)) );
  
  return true;
  
} // end read_var_base


static bool
read_var (
          Interpreter    *intp,
          const uint8_t   var,
          uint16_t       *val,
          char          **err
          )
{
  return read_var_base ( intp, var, val, true, err );
} // end read_var


static bool
read_var_nopop (
                Interpreter    *intp,
                const uint8_t   var,
                uint16_t       *val,
                char          **err
                )
{
  return read_var_base ( intp, var, val, false, err );
} // end read_var_nopop


static bool
read_ind_var_ref (
                  Interpreter    *intp,
                  const uint8_t   var,
                  uint8_t        *ref,
                  char          **err
                  )
{

  uint16_t tmp;

  
  if ( !read_var_base ( intp, var, &tmp, true, err ) )
    return false;
  if ( tmp >= 256 )
    {
      msgerror ( err, "Failed to read indirect variable, reference"
                 " is too large (%u)", tmp );
      return false;
    }
  *ref= (uint8_t) tmp;
  
  return true;
  
} // end read_ind_var_ref


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
op_to_refvar (
              Interpreter      *intp,
              const operand_t  *op,
              uint8_t          *ref,
              char            **err
              )
{

  switch ( op->type )
    {
    case OP_LARGE:
      msgerror ( err, "Trying to reference a variable with a large constant" );
      return false;
    case OP_SMALL:
      *ref= op->u8.val;
      break;
    case OP_VARIABLE:
      if ( !read_ind_var_ref ( intp, op->u8.val, ref, err ) )
        return false;
      break;
    default:
      ee ( "interpreter.c - op_to_u16 - WTF!!" );
    }

  return true;
  
} // op_to_refvar


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
  uint16_t local_vars[15],paddr;
  int i;
  
  
  // Comprovacions
  if ( nops == 0 )
    {
      msgerror ( err, "Failed to call routine: missing routine argument" );
      return false;
    }
  if ( ops[0].type != OP_LARGE && ops[0].type != OP_VARIABLE )
    {
      msgerror ( err, "Failed to call routine: invalid operand"
                 " type for routine argument" );
      return false;
    }

  // Obté paràmetre routine.
  if ( !op_to_u16 ( intp, &(ops[0]), &paddr, err ) )
    return false;
  
  // Descodifica rutina.
  // --> Cas especial. No fa res i retorna fals.
  if ( paddr == 0 )
    {
      if ( !discard_result )
        {
          if ( !write_var ( intp, result_var, 0, err ) )
            return false;
        }
      return true;
    }
  // --> Adreça real
  addr= unpack_addr ( intp, paddr, true );
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
  for ( i= 1; i < nops && i <= num_local_vars; ++i )
    {
      args_mask|= 0x1<<(i-1);
      if ( !op_to_u16 ( intp, &(ops[i]), &(local_vars[i-1]), err ) )
        return false;
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
              const int     wanted_ops, // -1 vol dir que no es demana
              const bool    extra_byte,
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
  if ( extra_byte )
    {
      if ( !memory_map_READB ( intp->mem, state->PC++, &ops_type, true, err ) )
        return false;
      if ( N == 4 ) // Llig si no hi han NONE
        {
          while ( N < 8 && (ops[N].type= (ops_type>>6)) != OP_NONE )
            {
              ++N;
              ops_type<<= 2;
            }
        }
    }
  if ( wanted_ops != -1 && wanted_ops != N )
    {
      msgerror ( err, "Expected %d operands but %d found", wanted_ops, N );
      return false;
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
read_small_small_store (
                        Interpreter  *intp,
                        uint8_t      *op1,
                        uint8_t      *op2,
                        uint8_t      *store_var,
                        char        **err
                        )
{

  if ( !memory_map_READB ( intp->mem, intp->state->PC++, op1, true, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++, op2, true, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;

  return true;
  
} // end read_small_small_store


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
read_small_var_store (
                      Interpreter  *intp,
                      uint8_t      *op1,
                      uint16_t     *op2,
                      uint8_t      *store_var,
                      char        **err
                      )
{

  uint8_t tmp2;

  
  if ( !read_small_small_store ( intp, op1, &tmp2, store_var, err ) )
    return false;
  if ( !read_var ( intp, tmp2, op2, err ) )
    return false;
  
  return true;
  
} // end read_small_var_store


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
read_var_small_store (
                      Interpreter  *intp,
                      uint16_t     *op1,
                      uint8_t      *op2,
                      uint8_t      *store_var,
                      char        **err
                      )
{

  uint8_t tmp1;

  
  if ( !read_small_small_store ( intp, &tmp1, op2, store_var, err ) )
    return false;
  if ( !read_var ( intp, tmp1, op1, err ) )
    return false;
  
  return true;
  
} // end read_var_small_store


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
read_var_var_store (
                    Interpreter  *intp,
                    uint16_t     *op1,
                    uint16_t     *op2,
                    uint8_t      *store_var,
                    char        **err
                    )
{

  uint8_t tmp1,tmp2;

  
  if ( !read_small_small_store ( intp, &tmp1, &tmp2, store_var, err ) )
    return false;
  if ( !read_var ( intp, tmp1, op1, err ) )
    return false;
  if ( !read_var ( intp, tmp2, op2, err ) )
    return false;
  
  return true;
  
} // end read_var_var_store


static bool
read_var_ops_store (
                    Interpreter  *intp,
                    operand_t    *ops,
                    int          *nops,
                    const int     wanted_ops,
                    const bool    extra_byte,
                    uint8_t      *store_var,
                    char        **err
                    )
{
  
  if ( !read_var_ops ( intp, ops, nops, wanted_ops, extra_byte, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;

  return true;
  
} // end read_var_ops_store


static bool
read_op1_var (
              Interpreter  *intp,
              uint16_t     *op,
              char        **err
              )
{

  uint8_t tmp;


  if ( !memory_map_READB ( intp->mem, intp->state->PC++, &tmp, true, err ) )
    return false;
  if ( !read_var ( intp, tmp, op, err ) )
    return false;
  
  return true;
  
} // end read_op1_var


static bool
read_op1_large_store (
                      Interpreter  *intp,
                      uint16_t     *op,
                      uint8_t      *store_var,
                      char        **err
                      )
{
  
  if ( !memory_map_READW ( intp->mem, intp->state->PC, op, true, err ) )
    return false;
  intp->state->PC+= 2;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;
  
  return true;
  
} // end read_op1_large_store


static bool
read_op1_small_store (
                      Interpreter  *intp,
                      uint8_t      *op,
                      uint8_t      *store_var,
                      char        **err
                      )
{
  
  if ( !memory_map_READB ( intp->mem, intp->state->PC++, op, true, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;
  
  return true;
  
} // end read_op1_small_store


static bool
read_op1_var_store (
                    Interpreter  *intp,
                    uint16_t     *op,
                    uint8_t      *store_var,
                    char        **err
                    )
{
  
  if ( !read_op1_var ( intp, op, err ) )
    return false;
  if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                           store_var, true, err ) )
    return false;
  
  return true;
  
} // end read_op1_var_store


static bool
ret_val (
         Interpreter     *intp,
         const uint16_t   val,
         char           **err
         )
{

  bool discard;
  uint8_t res_var;
  
  
  discard= FRAME_DISCARD_RES(intp->state);
  res_var= (uint8_t) FRAME_NUM_RES(intp->state);
  if ( !state_free_frame ( intp->state, err ) )
    return false;
  if ( !discard )
    {
      if ( !write_var ( intp, res_var, val, err ) )
        return false;
    }
  
  return true;
  
} // end ret_val


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
        offset= (uint32_t) -((int32_t) (16384 - offset));
    }
  else offset= (uint32_t) (b1&0x3F);
  cond_value= ((b1&0x80)!=0); // True si bit7 està actiu.

  // Bot
  if ( cond == cond_value )
    {
      if ( offset == 0 )
        {
          if ( !ret_val ( intp, 0, err ) ) return false;
        }
      else if ( offset == 1 )
        {
          if ( !ret_val ( intp, 1, err ) ) return false;
        }
      else
        state->PC+= offset-2;
    }
  
  return true;
  
} // end branch;


static bool
get_object_offset (
                   Interpreter     *intp,
                   const uint16_t   object,
                   uint32_t        *offset
                   )
{

  // Comprovacions i obté property pointer
  if ( intp->version <= 3 )
    {
      if ( object < 1 || object > 255 )
        {
          ww ( "invalid object index %u", object );
          return false;
        }
      *offset=
        intp->object_table_offset +
        31*2 + // Default properties
        ((uint32_t) (object-1))*9
        ;
    }
  else
    {
      if ( object < 1 )
        {
          ww ( "invalid object index %u", object );
          return false;
        }
      *offset=
        intp->object_table_offset +
        63*2 + // Default properties
        ((uint32_t) (object-1))*14
        ;
    }

  return true;
  
} // end get_object_offset


// addr==0 indica que no s'ha trobat.
static bool
get_prop_addr_len (
                   Interpreter     *intp,
                   const uint16_t   object,
                   const uint16_t   property,
                   uint16_t        *addr,
                   uint8_t         *len,
                   char           **err
                   )
{

  uint32_t property_pointer_offset;
  uint16_t property_pointer,offset;
  uint8_t text_length,b0,b1,prop_num,prop_len;
  
  
  // Obté property pointer
  if ( !get_object_offset ( intp, object, &property_pointer_offset ) )
    {
      *addr= 0; *len= 0;
      return true;
    }
  property_pointer_offset+= (intp->version <= 3) ? 7 : 12;
  if ( !memory_map_READW ( intp->mem, property_pointer_offset,
                           &property_pointer, false, err ) )
    return false;
  
  // Descarta capçalera.
  if ( !memory_map_READB ( intp->mem, property_pointer,
                           &text_length, false, err ) )
    return false;
  offset= property_pointer + 1 + ((uint16_t) text_length)*2;
  
  // Busca propietat
  if ( intp->version <= 3 )
    {
      prop_len= 0;
      do {
        offset+= (uint16_t) prop_len;
        if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
          return false;
        prop_num= b0&0x1f;
        prop_len= (b0>>5)+1;
        ++offset;
      } while ( prop_num != 0 && ((uint16_t) prop_num) != property );
      if ( prop_num == 0 ) *addr= 0;
      else                 *addr= offset;
    }
  else
    {
      prop_len= 0;
      do {
        offset+= (uint16_t) prop_len;
        if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
          return false;
        if ( b0&0x80 )
          {
            if ( !memory_map_READB ( intp->mem, offset+1, &b1, false, err ) )
              return false;
            prop_num= b0&0x3f;
            prop_len= b1&0x3f;
            if ( prop_len == 0 ) prop_len= 64;
            offset+= 2;
          }
        else
          {
            prop_num= b0&0x3f;
            prop_len= (b0&0x40)!=0 ? 2 : 1;
            ++offset;
          }
      } while ( prop_num != 0 && ((uint16_t) prop_num) != property );
      if ( prop_num == 0 ) *addr= 0;
      else                 *addr= offset;
    }
  *len= prop_len;
  
  return true;
  
} // end get_prop_addr_len


static bool
get_prop_default_addr (
                       Interpreter     *intp,
                       const uint16_t   property,
                       uint16_t        *addr,
                       char           **err
                       )
{

  // Comprovacions i obté property pointer
  if ( (intp->version <= 3 && (property < 1 || property > 31)) ||
       (intp->version > 3 && (property < 1 || property > 63)) )
    {
      msgerror ( err, "Failed to get property value: invalid"
                 " property index %u", property );
      return false;
    }

  // Fixa adreça
  *addr= ((uint16_t) intp->object_table_offset) + (property-1)*2;
  
  return true;
  
} // end get_prop_default_addr


static bool
get_prop_addr (
               Interpreter     *intp,
               const uint16_t   object,
               const uint16_t   property,
               uint16_t        *addr,
               char           **err
               )
{
  
  uint8_t tmp;
  
  
  if ( !get_prop_addr_len ( intp, object, property, addr, &tmp, err ) )
    return false;
  /*
  if ( *addr == 0 )
    {
      if ( !get_prop_default_addr ( intp, property, addr, err ) )
        return false;
    }
  */
  
  return true;
  
} // end get_prop_addr


static bool
get_prop_default (
                  Interpreter     *intp,
                  const uint16_t   property,
                  uint16_t        *data,
                  char           **err
                  )
{

  uint16_t offset;
  

  if ( !get_prop_default_addr ( intp, property, &offset, err ) )
    return false;
  if ( !memory_map_READW ( intp->mem, (uint32_t) offset, data, false, err ) )
    return false;
  
  return true;
  
} // end get_prop_default


static bool
get_prop (
          Interpreter     *intp,
          const uint16_t   object,
          const uint16_t   property,
          uint16_t        *data,
          char           **err
          )
{
  
  uint16_t addr;
  uint8_t len,tmp;
  

  // Obté adreça i longitut
  if ( !get_prop_addr_len ( intp, object, property, &addr, &len, err ) )
    return false;
  
  // Obté contingut
  if ( addr == 0 ) // Torna valor de la taula per defecte
    {
      if ( !get_prop_default ( intp, property, data, err ) )
        return false;
    }
  else if ( len == 1 )
    {
      if ( !memory_map_READB ( intp->mem, (uint32_t) addr, &tmp, false, err ) )
        return false;
      *data= (uint16_t) tmp;
    }
  else if ( len == 2 )
    {
      if ( !memory_map_READW ( intp->mem, (uint32_t) addr, data, false, err ) )
        return false;
    }
  else
    {
      msgerror ( err, "Failed to get property data: unable to"
                 " read property of length %u", len );
      return false;
    }
  
  return true;
  
} // end get_prop


static bool
get_next_prop (
               Interpreter     *intp,
               const uint16_t   object,
               const uint16_t   property,
               uint16_t        *result,
               char           **err
               )
{

  uint32_t property_pointer_offset;
  uint16_t property_pointer,offset;
  uint8_t text_length,b0,b1,prop_num,prop_len;
  

  // Obté property pointer
  if ( !get_object_offset ( intp, object, &property_pointer_offset ) )
    {
      *result= 0;
      return true;
    }
  property_pointer_offset+= (intp->version <= 3) ? 7 : 12;
  if ( !memory_map_READW ( intp->mem, property_pointer_offset,
                           &property_pointer, false, err ) )
    return false;
  
  // Descarta capçalera.
  if ( !memory_map_READB ( intp->mem, property_pointer,
                           &text_length, false, err ) )
    return false;
  offset= property_pointer + 1 + ((uint16_t) text_length)*2;
  
  // Busca propietat
  if ( intp->version <= 3 )
    {
      // Busca primera propietat
      prop_len= 0;
      prop_num= 0;
      if ( property != 0 )
        do {
          offset+= (uint16_t) prop_len;
          if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
            return false;
          prop_num= b0&0x1f;
          prop_len= (b0>>5)+1;
          ++offset;
        } while ( prop_num != 0 && ((uint16_t) prop_num) != property );
      if ( prop_num != property ) *result= 0;
      else // Cerca següent
        {
          offset+= (uint16_t) prop_len;
          if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
            return false;
          *result= (uint16_t) (b0&0x1f); // Si no en té més serà 0
        }
    }
  else
    {
      // Busca primera propietat
      prop_len= 0;
      prop_num= 0;
      if ( property != 0 )
        do {
          offset+= (uint16_t) prop_len;
          if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
            return false;
          if ( b0&0x80 )
            {
              if ( !memory_map_READB ( intp->mem, offset+1, &b1, false, err ) )
                return false;
              prop_num= b0&0x3f;
              prop_len= b1&0x3f;
              if ( prop_len == 0 ) prop_len= 64;
              offset+= 2;
            }
          else
            {
              prop_num= b0&0x3f;
              prop_len= (b0&0x40)!=0 ? 2 : 1;
              ++offset;
            }
        } while ( prop_num != 0 && ((uint16_t) prop_num) != property );
      if ( prop_num != property ) *result= 0;
      else
        {
          offset+= (uint16_t) prop_len;
          if ( !memory_map_READB ( intp->mem, offset, &b0, false, err ) )
            return false;
          *result= (uint16_t) (b0&0x3f);
        }
    }
  
  return true;
  
} // end get_next_prop


static bool
get_prop_len (
              Interpreter     *intp,
              const uint16_t   addr,
              uint8_t         *len,
              char           **err
              )
{

  uint8_t b0;
  
  
  // Cas especial.
  if ( addr == 0 )
    {
      *len= 0;
      return true;
    }
  
  // Busca propietat (Està sempre en el byte anterior de data)
  if ( intp->version <= 3 )
    {
      if ( !memory_map_READB ( intp->mem, addr-1, &b0, false, err ) )
        return false;
      *len= (b0>>5)+1;
    }
  else
    {
      if ( !memory_map_READB ( intp->mem, addr-1, &b0, false, err ) )
        return false;
      if ( b0&0x80 )
        {
          *len= b0&0x3f;
          if ( *len == 0 ) *len= 64;
        }
      else *len= (b0&0x40)!=0 ? 2 : 1;
    }
  
  return true;
  
} // end get_prop_len


static bool
get_child (
           Interpreter     *intp,
           const uint16_t   object,
           uint16_t        *res,
           bool            *cond,
           char           **err
           )
{

  uint32_t object_offset,offset;
  uint8_t tmp8;
  
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    {
      *res= 0;
      *cond= false;
      return true;
    }
  
  // Offset fill.
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4 + 2;
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      *res= (uint16_t) tmp8;
    }
  else
    {
      offset= object_offset + 6 + 4;
      if ( !memory_map_READW ( intp->mem, offset, res, false, err ) )
        return false;
    }
  
  // Condició
  *cond= (*res!=0);
  
  return true;
  
} // end get_child


static bool
get_parent (
            Interpreter     *intp,
            const uint16_t   object,
            uint16_t        *res,
            char           **err
            )
{

  uint32_t object_offset,offset;
  uint8_t tmp8;
  
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    {
      *res= 0;
      return true;
    }
  
  // Offset pare.
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4;
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      *res= (uint16_t) tmp8;
    }
  else
    {
      offset= object_offset + 6;
      if ( !memory_map_READW ( intp->mem, offset, res, false, err ) )
        return false;
    }

  return true;
  
} // end get_parent


static bool
get_sibling (
             Interpreter     *intp,
             const uint16_t   object,
             uint16_t        *res,
             bool            *cond,
             char           **err
             )
{

  uint32_t object_offset,offset;
  uint8_t tmp8;
  
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    {
      *res= 0; *cond= false;
      return true;
    }
  
  // Offset 'next'.
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4 + 1;
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      *res= (uint16_t) tmp8;
    }
  else
    {
      offset= object_offset + 6 + 2;
      if ( !memory_map_READW ( intp->mem, offset, res, false, err ) )
        return false;
    }
  
  // Condició
  *cond= (*res!=0);
  
  return true;
  
} // end get_sibling


static bool
put_prop (
          Interpreter     *intp,
          const uint16_t   object,
          const uint16_t   property,
          const uint16_t   data,
          char           **err
          )
{
  
  uint16_t addr;
  uint8_t len;
  
  
  // Obté adreça i longitut
  if ( !get_prop_addr_len ( intp, object, property, &addr, &len, err ) )
    return false;

  // Obté contingut
  if ( len == 1 )
    {
      if ( !memory_map_WRITEB ( intp->mem, (uint32_t) addr,
                                (uint8_t) data, false, err ) )
        return false;
    }
  else if ( len == 2 )
    {
      if ( !memory_map_WRITEW ( intp->mem, (uint32_t) addr, data, false, err ) )
        return false;
    }
  else
    {
      msgerror ( err, "Failed to put property data: unable to"
                 " write property of length %u", len );
      return false;
    }
  
  return true;
  
} // end put_prop


static bool
test_attr (
           Interpreter     *intp,
           const uint16_t   object,
           const uint16_t   attr,
           bool            *ret,
           char           **err
           )
{

  uint32_t object_offset,offset;
  uint8_t mask,val;
  

  // Comprova rang
  if ( (intp->version <= 3 && attr >= 32) || (intp->version > 3 && attr >= 48) )
    {
      msgerror ( err, "Failed to test object attribute: %u is out of range",
                 attr );
      return false;
    }
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    {
      *ret= false;
      return true;
    }

  // Comprova atribut.
  offset= object_offset + attr/8;
  mask= 1<<(7-(attr%8));
  if ( !memory_map_READB ( intp->mem, offset, &val, false, err ) )
    return false;
  *ret= (val&mask)!=0;
  
  return true;
  
} // end test_attr


static bool
clear_attr (
            Interpreter     *intp,
            const uint16_t   object,
            const uint16_t   attr,
            char           **err
            )
{

  uint32_t object_offset,offset;
  uint8_t mask,val;
  

  // Comprova rang
  if ( (intp->version <= 3 && attr >= 32) || (intp->version > 3 && attr >= 48) )
    {
      msgerror ( err, "Failed to execute clear_attr: %u is out of range",
                 attr );
      return false;
    }
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    return true;
  
  // Comprova atribut.
  offset= object_offset + attr/8;
  mask= 1<<(7-(attr%8));
  if ( !memory_map_READB ( intp->mem, offset, &val, false, err ) )
    return false;
  val&= ~mask;
  if ( !memory_map_WRITEB ( intp->mem, offset, val, false, err ) )
    return false;
  
  return true;
  
} // end clear_attr


static bool
set_attr (
          Interpreter     *intp,
          const uint16_t   object,
          const uint16_t   attr,
          char           **err
          )
{

  uint32_t object_offset,offset;
  uint8_t mask,val;
  

  // Comprova rang
  if ( (intp->version <= 3 && attr >= 32) || (intp->version > 3 && attr >= 48) )
    {
      msgerror ( err, "Failed to execute set_attr: %u is out of range",
                 attr );
      return false;
    }
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    return true;
  
  // Comprova atribut.
  offset= object_offset + attr/8;
  mask= 1<<(7-(attr%8));
  if ( !memory_map_READB ( intp->mem, offset, &val, false, err ) )
    return false;
  val|= mask;
  if ( !memory_map_WRITEB ( intp->mem, offset, val, false, err ) )
    return false;
  
  return true;
  
} // end set_attr


static bool
jin (
     Interpreter     *intp,
     const uint16_t   a,
     const uint16_t   b,
     char           **err
     )
{

  uint32_t parent_offset;
  uint16_t parent_u16;
  uint8_t parent_u8;
  bool is_parent;
  
  
  if ( a == 0 && b == 0 )
    is_parent= true;
  else if ( !get_object_offset ( intp, a, &parent_offset ) )
    is_parent= false;
  else
    {
  
      // Llig parent
      if ( intp->version <= 3 )
        {
          parent_offset+= 4;
          if ( !memory_map_READB ( intp->mem, parent_offset,
                                   &parent_u8, false, err ) )
            return false;
          is_parent= (((uint16_t) parent_u8) == b);
        }
      else
        {
          parent_offset+= 6;
          if ( !memory_map_READW ( intp->mem, parent_offset,
                                   &parent_u16, false, err ) )
            return false;
          is_parent= (parent_u16 == b);
        }
    }

  // Bota.
  if ( !branch ( intp, is_parent, err ) ) return false;
  
  return true;
  
} // end jin


static bool
remove_obj (
            Interpreter     *intp,
            const uint16_t   object,
            char           **err
            )
{

  uint32_t object_offset,offset;
  uint8_t tmp8;
  uint16_t parent,next,p,p_next;
  bool stop;
  
  
  // Obté object offset
  if ( !get_object_offset ( intp, object, &object_offset ) )
    return true;
  
  // Obté pare, sibling i fica a null pare
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4;
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      parent= (uint16_t) tmp8;
      // Si no té pare no fa res.
      if ( parent == 0 ) return true;
      if ( !memory_map_READB ( intp->mem, offset+1, &tmp8, false, err ) )
        return false;
      next= (uint16_t) tmp8;
      if ( !memory_map_WRITEB ( intp->mem, offset, 0, false, err ) )
        return false;
      if ( !memory_map_WRITEB ( intp->mem, offset+1, 0, false, err ) )
        return false;
    }
  else
    {
      offset= object_offset + 6;
      if ( !memory_map_READW ( intp->mem, offset, &parent, false, err ) )
        return false;
      // Si no té pare no fa res.
      if ( parent == 0 ) return true;
      if ( !memory_map_READW ( intp->mem, offset+2, &next, false, err ) )
        return false;
      if ( !memory_map_WRITEW ( intp->mem, offset, 0, false, err ) )
        return false;
      if ( !memory_map_WRITEW ( intp->mem, offset+2, 0, false, err ) )
        return false;
    }

  
  // Elimina node de la llista de fills del pare
  if ( !get_object_offset ( intp, parent, &object_offset ) )
    return true;
  if ( intp->version <= 3 )
    {

      // Primer fill.
      offset= object_offset + 4 + 2;
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      p= (uint16_t) tmp8;
      if ( p == object )
        {
          if ( !memory_map_WRITEB ( intp->mem, offset,
                                    (uint8_t) next, false, err ) )
            return false;
          stop= true;
        }
      else stop= false;

      // Cerca i fica a 0
      while ( !stop && p != 0 )
        {
          if ( !get_object_offset ( intp, p, &object_offset ) )
            return true;
          offset= object_offset + 4 + 1;
          if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
            return false;
          p_next= (uint16_t) tmp8;
          if ( p_next == object )
            {
              if ( !memory_map_WRITEB ( intp->mem, offset,
                                        (uint8_t) next, false, err ) )
                return false;
              stop= true;
            }
          else p= p_next;
        }
      
    }
  else
    {

      // Primer fill.
      offset= object_offset + 6 + 4;
      if ( !memory_map_READW ( intp->mem, offset, &p, false, err ) )
        return false;
      if ( p == object )
        {
          if ( !memory_map_WRITEW ( intp->mem, offset, next, false, err ) )
            return false;
          stop= true;
        }
      else stop= false;
      
      // Cerca i fica a 0
      while ( !stop && p != 0 )
        {
          if ( !get_object_offset ( intp, p, &object_offset ) )
            return true;
          offset= object_offset + 6 + 2;
          if ( !memory_map_READW ( intp->mem, offset, &p_next, false, err ) )
            return false;
          if ( p_next == object )
            {
              if ( !memory_map_WRITEW ( intp->mem, offset, next, false, err ) )
                return false;
              stop= true;
            }
          else p= p_next;
        }
      
    }
  
  return true;
  
} // end remove_obj


static bool
insert_obj (
            Interpreter     *intp,
            const uint16_t   object,
            const uint16_t   destination,
            char           **err
            )
{

  uint32_t object_offset,offset;
  uint8_t tmp8;
  uint16_t next;
  
  
  // Comença llevant-lo del pare.
  if ( !remove_obj ( intp, object, err ) ) return false;

  // Modifica destination i obté next
  if ( !get_object_offset ( intp, destination, &object_offset ) )
    return true;
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4 + 2; // child
      if ( !memory_map_READB ( intp->mem, offset, &tmp8, false, err ) )
        return false;
      next= (uint16_t) tmp8;
      if ( !memory_map_WRITEB ( intp->mem, offset,
                                (uint8_t) object, false, err ) )
        return false;
    }
  else
    {
      offset= object_offset + 6 + 4; // child
      if ( !memory_map_READW ( intp->mem, offset, &next, false, err ) )
        return false;
      if ( !memory_map_WRITEW ( intp->mem, offset, object, false, err ) )
        return false;
    }

  // Modifica object
  if ( !get_object_offset ( intp, object, &object_offset ) )
    return true;
  if ( intp->version <= 3 )
    {
      offset= object_offset + 4;
      // Escriu pare
      if ( !memory_map_WRITEB ( intp->mem, offset,
                                (uint8_t) destination, false, err ) )
        return false;
      // Escriu sibling
      if ( !memory_map_WRITEB ( intp->mem, offset+1,
                                (uint8_t) next, false, err ) )
        return false;
    }
  else
    {
      offset= object_offset + 6;
      // Escriu pare
      if ( !memory_map_WRITEW ( intp->mem, offset, destination, false, err ) )
        return false;
      // Escriu sibling
      if ( !memory_map_WRITEW ( intp->mem, offset+2, next, false, err ) )
        return false;
    }
  
  return true;
  
} // end insert_obj


static bool
text_add (
          Interpreter  *intp,
          const char    c,
          char        **err
          )
{

  size_t nsize;

  
  if ( intp->text.size == intp->text.N )
    {
      nsize= intp->text.size*2;
      if ( nsize <= intp->text.size )
        {
          msgerror ( err, "Failed to allocate memory while decoding"
                     " ZSCII string" );
          return false;
        }
      intp->text.v= g_renew ( char, intp->text.v, nsize );
      intp->text.size= nsize;
    }
  intp->text.v[intp->text.N++]= c;

  return true;
  
} // end text_add


static bool
text_add_unicode (
                  Interpreter     *intp,
                  const uint16_t   c,
                  char           **err
                  )
{

  uint8_t val;

  
  if ( c <= 0x007f )
    {
      if ( !text_add ( intp, (char) c, err ) ) return false;
    }
  else if ( c <= 0x07ff )
    {
      val= ((uint8_t) (c>>6)) | 0xc0;
      if ( !text_add ( intp, (char) val, err ) ) return false;
      val= ((uint8_t) (c&0x3f)) | 0x80;
      if ( !text_add ( intp, (char) val, err ) ) return false;
    }
  else
    {
      val= ((uint8_t) (c>>12)) | 0xe0;
      if ( !text_add ( intp, (char) val, err ) ) return false;
      val= ((uint8_t) ((c>>6)&0x3f)) | 0x80;
      if ( !text_add ( intp, (char) val, err ) ) return false;
      val= ((uint8_t) (c&0x3f)) | 0x80;
      if ( !text_add ( intp, (char) val, err ) ) return false;
    }

  return true;
  
} // end text_add_unicode


static bool
zscii_char2utf8 (
                 Interpreter     *intp,
                 const uint16_t   val,
                 char           **err
                 )
{

  int16_t zc;
  

  zc= (int16_t) val;
  if ( zc < 0 ) goto wrong_zc;
  else if ( zc < 32 ) // Alguns caràcters especials
    {
      if ( zc == 0 )
        {
          if ( !text_add ( intp, '\0', err ) ) return false;
        }
      else if ( zc == 9 )
        {
          if ( intp->version == 6 )
            {
              if ( !text_add ( intp, '\t', err ) ) return false;
            }
          else goto wrong_zc;
        }
      else if ( zc == 11 ) // Sentence space???
        {
          if ( intp->version == 6 )
            {
              if ( !text_add ( intp, ' ', err ) ) return false;
            }
          else goto wrong_zc;
        }
      else if ( zc == 13 )
        {
          if ( !text_add ( intp, '\n', err ) ) return false;
        }
      else goto wrong_zc;
    }
  else if ( zc < 127 ) // standard ASCII
    {
      if ( !text_add ( intp, (char) zc, err ) ) return false;
    }
  else if ( zc < 155 ) goto wrong_zc;
  else if ( zc < 252 ) // extra characters
    {
      if ( intp->echars.enabled )
        {
          if ( !text_add_unicode ( intp, intp->echars.v[zc-155], err ) )
            return false;
        }
      else
        {
          if ( !text_add_unicode ( intp, ZSCII_TO_UNICODE[zc-155], err ) )
            return false;
        }
    }
  else goto wrong_zc;
  
  return true;

 wrong_zc:
  msgerror ( err, "Failed to print character: invalid code %d", zc );
  return false;
  
} // end zscii_char2utf8


// La longitut es mesura en paraules, i un -1 vol dir ignorar. No pot ser 0.
static bool
zscii2utf8 (
            Interpreter     *intp,
            const uint32_t   addr,
            uint32_t        *ret_addr, // Pot ser NULL
            const bool       hmem_allowed,
            const bool       is_abbr,
            int              length, // -1 Ignorar.
            char           **err
            )
{

  uint16_t word,abbr_addr,zscii;
  uint32_t caddr,tmp_addr;
  int alph,i,abbr_ind,prev_alph;
  uint8_t zc;
  bool end,lock_alph;
  enum {
    WAIT_ZC,
    WAIT_ZSCII_TOP,
    WAIT_ZSCII_LOW
  } mode;
  

  assert ( length == -1 || length > 0 );
  
  prev_alph= alph= 0;
  if ( !is_abbr ) intp->text.N= 0;
  abbr_ind= 0;
  lock_alph= false;
  caddr= addr;
  mode= WAIT_ZC;
  do {

    // Llig següent paraula
    if ( !memory_map_READW ( intp->mem, caddr, &word, hmem_allowed, err ) )
      return false;
    caddr+= 2;
    if ( length > 0 ) --length;
    
    // Descodifica
    end= (word&0x8000)!=0;
    for ( i= 0; i < 3; ++i )
      {
        zc= (word>>10)&0x1f;

        // ZSCII low
        if ( mode == WAIT_ZSCII_LOW )
          {
            zscii|= (uint16_t) zc;
            mode= WAIT_ZC;
            if ( !zscii_char2utf8 ( intp, zscii, err ) ) return false;
          }

        // ZSCII top
        else if ( mode == WAIT_ZSCII_TOP )
          {
            zscii= ((uint16_t) zc)<<5;
            mode= WAIT_ZSCII_LOW;
          }

        // Abbreviatures
        else if ( abbr_ind != 0 )
          {
            if ( is_abbr )
              {
                msgerror ( err, "Failed to print abbreviation: "
                           "found abbreviation inside abbreviation" );
                return false;
              }
            tmp_addr= intp->abbr_table_addr + (32*(abbr_ind-1) + zc)*2;
            if ( !memory_map_READW ( intp->mem, tmp_addr, &abbr_addr,
                                     false, err ) )
              return false;
            if ( !zscii2utf8 ( intp, ((uint32_t) abbr_addr)<<1, NULL,
                               false, true, -1, err ) )
              return false;
            abbr_ind= 0;
          }
        
        // Special characters
        else if ( zc < 6 )
          {
            switch ( zc )
              {
              case 0:
                if ( !text_add ( intp, ' ', err ) ) return false;
                break;
              case 1:
                if ( intp->version == 1 )
                  {
                    if ( !text_add ( intp, '\n', err ) ) return false;
                  }
                else abbr_ind= 1;
                break;
              case 2:
                if ( intp->version <= 2 )
                  {
                    if ( !lock_alph ) { prev_alph= alph; alph= (alph+1)%2; }
                  }
                else abbr_ind= 2;
                break;
              case 3:
                if ( intp->version <= 2 )
                  {
                    if ( !lock_alph )
                      { prev_alph= alph; if ( --alph == -1 ) alph= 2; }
                  }
                else abbr_ind= 3;
                break;
              case 4:
                if ( intp->version <= 2 )
                  {
                    lock_alph= true;
                    prev_alph= alph= (alph+1)%2;
                  }
                else { prev_alph= 0; alph= 1; }
                break;
              case 5:
                if ( intp->version <= 2 )
                  {
                    lock_alph= true;
                    if ( --alph == -1 ) alph= 2;
                    prev_alph= alph;
                  }
                else { prev_alph= 0; alph= 2; }
                break;
              }
          }
        
        // Valors
        else
          {
            if ( zc == 6 && alph == 2 ) mode= WAIT_ZSCII_TOP;
            else if ( intp->alph_table.enabled  )
              {
                zscii= (uint16_t) intp->alph_table.v[alph][zc-6];
                if ( !zscii_char2utf8 ( intp, zscii, err ) ) return false;
              }
            else if ( intp->version == 1 )
              {
                if ( !text_add ( intp, ZSCII_ENC_V1[alph][zc-6], err ) )
                  return false;
              }
            else
              {
                if ( !text_add ( intp, ZSCII_ENC[alph][zc-6], err ) )
                  return false;
              }
            alph= prev_alph;
          }
        
        word<<= 5;
      }
    
  } while ( !end && length != 0 );
  if ( !is_abbr ) if ( !text_add ( intp, '\0', err ) ) return false;
  if ( ret_addr != NULL ) *ret_addr= caddr;
  
  return true;
  
} // end zscii2utf8


static uint8_t
unicode2zscii (
               Interpreter    *intp,
               const uint32_t  val
               )
{

  uint8_t ret;
  int n;

  
  // No es suporten caràcters de més de 16bit
  if ( val >= 0xFFFF ) ret= '?';

  // Taula concreta.
  else if ( intp->echars.enabled )
    {
      ret= '?';
      for ( n= 0; n < (int) ((uint16_t) intp->echars.N); ++n )
        if ( intp->echars.v[n] == ((uint16_t) val) )
          {
            ret= n + 155;
            break;
          }
    }
  
  // Valors per defecte.
  else
    {
      switch ( (uint16_t) val )
        {
        case 0x00e4: ret= 155; break;
        case 0x00f6: ret= 156; break;
        case 0x00fc: ret= 157; break;
        case 0x00c4: ret= 158; break;
        case 0x00d6: ret= 159; break;
        case 0x00dc: ret= 160; break;
        case 0x00df: ret= 161; break;
        case 0x00bb: ret= 162; break;
        case 0x00ab: ret= 163; break;
        case 0x00eb: ret= 164; break;
        case 0x00ef: ret= 165; break;
        case 0x00ff: ret= 166; break;
        case 0x00cb: ret= 167; break;
        case 0x00cf: ret= 168; break;
        case 0x00e1: ret= 169; break;
        case 0x00e9: ret= 170; break;
        case 0x00ed: ret= 171; break;
        case 0x00f3: ret= 172; break;
        case 0x00fa: ret= 173; break;
        case 0x00fd: ret= 174; break;
        case 0x00c1: ret= 175; break;
        case 0x00c9: ret= 176; break;
        case 0x00cd: ret= 177; break;
        case 0x00d3: ret= 178; break;
        case 0x00da: ret= 179; break;
        case 0x00dd: ret= 180; break;
        case 0x00e0: ret= 181; break;
        case 0x00e8: ret= 182; break;
        case 0x00ec: ret= 183; break;
        case 0x00f2: ret= 184; break;
        case 0x00f9: ret= 185; break;
        case 0x00c0: ret= 186; break;
        case 0x00c8: ret= 187; break;
        case 0x00cc: ret= 188; break;
        case 0x00d2: ret= 189; break;
        case 0x00d9: ret= 190; break;
        case 0x00e2: ret= 191; break;
        case 0x00ea: ret= 192; break;
        case 0x00ee: ret= 193; break;
        case 0x00f4: ret= 194; break;
        case 0x00fb: ret= 195; break;
        case 0x00c2: ret= 196; break;
        case 0x00ca: ret= 197; break;
        case 0x00ce: ret= 198; break;
        case 0x00d4: ret= 199; break;
        case 0x00db: ret= 200; break;
        case 0x00e5: ret= 201; break;
        case 0x00c5: ret= 202; break;
        case 0x00f8: ret= 203; break;
        case 0x00d8: ret= 204; break;
        case 0x00e3: ret= 205; break;
        case 0x00f1: ret= 206; break;
        case 0x00f5: ret= 207; break;
        case 0x00c3: ret= 208; break;
        case 0x00d1: ret= 209; break;
        case 0x00d5: ret= 210; break;
        case 0x00e6: ret= 211; break;
        case 0x00c6: ret= 212; break;
        case 0x00e7: ret= 213; break;
        case 0x00c7: ret= 214; break;
        case 0x00fe: ret= 215; break;
        case 0x00f0: ret= 216; break;
        case 0x00de: ret= 217; break;
        case 0x00d0: ret= 218; break;
        case 0x00a3: ret= 219; break;
        case 0x0153: ret= 220; break;
        case 0x0152: ret= 221; break;
        case 0x00a1: ret= 222; break;
        case 0x00bf: ret= 223; break;
        default: ret= '?';
        }
    }

  return ret;
  
} // end unicode_zscii


static bool
print_output3 (
               Interpreter  *intp,
               const char   *text,
               char        **err
               )
{

  const char *p;
  uint8_t zc,val;
  uint32_t addr,unicode_val;
  int o_ind,unicode_count;
  
  
  o_ind= intp->ostreams.N3-1;
  unicode_count= 0;
  unicode_val= 0; // Calla.
  for ( p= text; *p != '\0'; ++p )
    {

      // Conversió UTF-8 -> ZSCII
      val= (uint8_t) *p;
      // --> Primers caràcters
      if ( val < 32 )
        {
          if ( *p == '\n' ) zc= ZSCII_NEWLINE;
          else if ( *p == '\t' && intp->version == 6 ) zc= ZSCII_TAB;
          // desconegut en ZSCII
          else zc= '?';
        }
      // --> ASCII
      else if ( val < 127 ) zc= (uint8_t) *p;
      // --> DELETE
      else if ( val == 127 ) zc= '?';
      // --> Resta caràcters
      else
        {
          // NOTA!!! Per simplificar els caràcters UTF-8 a mitat no
          // els pinte com a ?, simplement els ignore.
          if ( (val&0xf8) == 0xf0 )
            {
              unicode_count= 3;
              unicode_val= (uint32_t) (val&0x7);
              continue;
            }
          else if ( (val&0xf0) == 0xe0 )
            {
              unicode_count= 2;
              unicode_val= (uint32_t) (val&0xf);
              continue;
            }
          else if ( (val&0xe0) == 0xc0 )
            {
              unicode_count= 1;
              unicode_val= (uint32_t) (val&0x1f);
              continue;
            }
          else if ( (val&0xc0) == 0x80 )
            {
              unicode_val<<= 6;
              unicode_val|= (uint8_t) (val&0x3f);
              if ( --unicode_count == 0 )
                zc= unicode2zscii ( intp, unicode_val );
              else continue;
            }
          // Pot passar ??
          else zc= '?';
        }
      
      // Escriu
      addr=
        intp->ostreams.o3[o_ind].addr +
        2 +
        (uint32_t) intp->ostreams.o3[o_ind].N
        ;
      if ( !memory_map_WRITEB ( intp->mem, addr, zc, true, err ) )
        return false;
      ++(intp->ostreams.o3[o_ind].N);
      
    }
  
  return true;
  
} // end print_output3


static bool
print_output (
              Interpreter  *intp,
              const char   *text,
              const bool    is_input,
              char        **err
              )
{

  FILE *f;

  
  // Screen
  if ( (intp->ostreams.active&INTP_OSTREAM_SCREEN)!=0 &&
       (intp->ostreams.active&INTP_OSTREAM_TABLE)==0 )
    {
      if ( !screen_print ( intp->screen, text, err ) )
        return false;
    }

  // Transcript
  if ( (intp->ostreams.active&INTP_OSTREAM_TRANSCRIPT)!=0 )
    {
      f= intp->transcript_fd!=NULL ? intp->transcript_fd : stdout;
      fprintf ( f, "%s", text );
    }
  
  // Table
  if ( (intp->ostreams.active&INTP_OSTREAM_TABLE)!=0 )
    {
      if ( !print_output3 ( intp, text, err ) )
        return false;
    }
  
  // Script
  if ( (intp->ostreams.active&INTP_OSTREAM_SCRIPT)!=0 )
    {
      ee ( "print_output - CAL IMPLEMENTAR output stream 4 (script)" );
    }
  
  return true;
  
} // end print_output


static bool
print_addr (
            Interpreter     *intp,
            const uint32_t   addr,
            uint32_t        *ret_addr,
            const bool       hmem_allowed,
            char           **err
            )
{

  if ( !zscii2utf8 ( intp, addr, ret_addr, hmem_allowed, false, -1, err ) )
    return false;
  if ( !print_output ( intp, intp->text.v, false, err ) )
    return false;
  
  return true;
  
} // end print_addr


static bool
print_paddr (
             Interpreter     *intp,
             const uint16_t   paddr,
             char           **err
             )
{

  uint32_t addr;

  
  addr= unpack_addr ( intp, paddr, false );
  return print_addr ( intp, addr, NULL, true, err );
  
} // end print_paddr


static bool
print_obj (
           Interpreter     *intp,
           const uint16_t   object,
           char           **err
           )
{

  uint32_t property_pointer_offset;
  uint16_t property_pointer,offset;
  uint8_t text_length;
  int length;
  

  // Obté property pointer
  if ( !get_object_offset ( intp, object, &property_pointer_offset ) )
    return true;
  property_pointer_offset+= (intp->version <= 3) ? 7 : 12;
  if ( !memory_map_READW ( intp->mem, property_pointer_offset,
                           &property_pointer, false, err ) )
    return false;
  
  // Llig grandària text i adreça text.
  if ( !memory_map_READB ( intp->mem, property_pointer,
                           &text_length, false, err ) )
    return false;
  offset= property_pointer + 1;
  length= (int) ((uint16_t) text_length);

  // Imprimeix si hi ha alguna cosa que imprimir.
  if ( length > 0 )
    {
      if ( !zscii2utf8 ( intp, offset, NULL, true, false, length, err ) )
        return false;
      if ( !print_output ( intp, intp->text.v, false, err ) )
        return false;
    }

  return true;

} // end print_obj


static bool
print (
       Interpreter  *intp,
       char        **err
       )
{
  return print_addr ( intp, intp->state->PC, &(intp->state->PC), true, err );
} // end print


static bool
print_num (
           Interpreter     *intp,
           const uint16_t   val,
           char           **err
           )
{

  int16_t num;


  num= (int16_t) val;
  // NOTA!!! segur que cap
  if ( snprintf ( intp->text.v, intp->text.size, "%d", num ) < 0 )
    {
      msgerror ( err, "Failed to print number: %d", num );
      return false;
    }
  if ( !print_output ( intp, intp->text.v, false, err ) )
    return false;
  
  return true;
  
} // end print_num


static bool
print_char (
            Interpreter     *intp,
            const uint16_t   val,
            char           **err
            )
{

  // Prepara
  intp->text.N= 0;

  // Descodifica caràcter
  if ( !zscii_char2utf8 ( intp, val, err ) ) return false;
  if ( !text_add ( intp, '\0', err ) ) return false;
  
  // Imprimeix
  if ( !print_output ( intp, intp->text.v, false, err ) ) return false;
  
  return true;
  
} // end print_char


static bool
print_unicode (
               Interpreter     *intp,
               const uint16_t   val,
               char           **err
               )
{
  
  // Prepara
  intp->text.N= 0;
  
  // Descodifica caràcter
  if ( !text_add_unicode ( intp, val, err ) ) return false;
  if ( !text_add ( intp, '\0', err ) ) return false;
  
  // Imprimeix
  if ( !print_output ( intp, intp->text.v, false, err ) ) return false;
  
  return true;
  
} // end print_unicode


static bool
print_table (
             Interpreter      *intp,
             const operand_t  *ops,
             const int         nops,
             char            **err
             )
{

  uint16_t text,width,height,skip;
  uint32_t r,c,addr;
  uint8_t zc;
  
  
  // Obté paràmetres
  if ( nops < 2 || nops > 4 )
    {
      msgerror ( err, "Failed to print table: wrong number of arguments" );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &text, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[1]), &width, err ) ) return false;
  if ( nops >= 3 )
    {
      if ( !op_to_u16 ( intp, &(ops[2]), &height, err ) ) return false;
    }
  else height= 1;
  if ( nops == 4 )
    {
      if ( !op_to_u16 ( intp, &(ops[3]), &skip, err ) ) return false;
    }
  else skip= 0;
  if ( height == 0 || width == 0 ) return true;

  // Imprimeix.
  addr= (uint32_t) text;
  for ( r= 0; r < (uint32_t) height; ++r )
    {
      for ( c= 0; c < (uint32_t) width; ++c )
        {
          if ( !memory_map_READB ( intp->mem, addr++, &zc, true, err ) )
            return false;
          if ( !print_char ( intp, (uint16_t) zc, err ) ) return false;
        }
      if ( !print_char ( intp, ZSCII_NEWLINE, err ) ) return false;
      addr+= (uint32_t) skip;
    }

  return true;
  
} // end print_table


static bool
print_input_text (
                  Interpreter  *intp,
                  const bool    partial,
                  char        **err
                  )
{

  size_t n;
  uint8_t zc;
  

  // Codifica input text
  intp->text.N= 0;
  for ( n= 0; n < intp->input_text.N; ++n )
    {
      zc= intp->input_text.v[n];
      if ( zc == 0 ) // No deuria
        { if ( !text_add ( intp, '\0', err ) ) return false; }
      else if ( zc == 9 )
        { if ( !text_add ( intp, '\t', err ) ) return false; }
      else if ( zc == 11 )
        { if ( !text_add ( intp, ' ', err ) ) return false; }
      else if ( zc == 13 ) // No deuria
        { if ( !text_add ( intp, '\n', err ) ) return false; }
      else if ( zc >= 32 && zc <= 126 )
        { if ( !text_add ( intp, (char) zc, err ) ) return false; }
      else if ( zc >= 155 && zc <= 251 )
        {
          if ( intp->echars.enabled )
            {
              if ( !text_add_unicode ( intp, intp->echars.v[zc-155], err ) )
                return false;
            }
          else
            {
              if ( !text_add_unicode ( intp, ZSCII_TO_UNICODE[zc-155], err ) )
                return false;
            }
        }
    }
  if ( !text_add ( intp, '\0', err ) ) return false;
  
  // Imprimeix
  if ( partial )
    {
      if ( !screen_print ( intp->screen, intp->text.v, err ) )
        return false;
    }
  else
    {
      if ( !print_output ( intp, intp->text.v, true, err ) )
        return false;
    }
  
  return true;
  
} // end print_input_text


// NOTA!! Sols s'utilitza en versio<=3
static bool
show_status_line (
                  Interpreter  *intp,
                  char        **err
                  )
{

  uint16_t object;
  uint32_t property_pointer_offset;
  uint16_t property_pointer,offset,tmp;
  uint8_t text_length,flags;
  int length;
  bool score_game;
  int score_hours,turns_minutes;
  
  
  // Obté text status (G00).
  object= memory_map_readvar ( intp->mem, 0 );
  if ( get_object_offset ( intp, object, &property_pointer_offset ) )
    {
      property_pointer_offset+= 7;
      if ( !memory_map_READW ( intp->mem, property_pointer_offset,
                               &property_pointer, false, err ) )
        return false;
      
      // Llig grandària text i adreça text.
      if ( !memory_map_READB ( intp->mem, property_pointer,
                               &text_length, false, err ) )
        return false;
      offset= property_pointer + 1;
      length= (int) ((uint16_t) text_length);
      
      // Imprimeix si hi ha alguna cosa que imprimir.
      if ( length > 0 )
        {
          if ( !zscii2utf8 ( intp, offset, NULL, true, false, length, err ) )
            return false;
        }
    }
  else // Object no vàlid
    {
      ww ( "show_status_line - invalid object %u", object );
      intp->text.N= 0;
      if ( !text_add ( intp, '?', err ) ) return false;
      if ( !text_add ( intp, '?', err ) ) return false;
      if ( !text_add ( intp, '?', err ) ) return false;
      if ( !text_add ( intp, '\0', err ) ) return false;
    }

  // Tipus de joc
  if ( intp->version <= 2 ) score_game= true;
  else
    {
      if ( !memory_map_READB ( intp->mem, 1, &flags, false, err ) )
        return false;
      score_game= (flags&0x1)==0;
    }

  // Score/hours
  tmp= memory_map_readvar ( intp->mem, 1 );
  score_hours= (int) ((int16_t) tmp);
  if ( score_game )
    {
      if ( score_hours < -99 ) score_hours= -99;
      else if ( score_hours > 999 ) score_hours= 999;
    }
  else
    {
      if ( score_hours < 0 ) score_hours= 0;
      else if ( score_hours > 23 ) score_hours= 23;
    }

  // Turns/Minutes
  tmp= memory_map_readvar ( intp->mem, 2 );
  turns_minutes= (int) ((int16_t) tmp);
  if ( score_game )
    {
      if ( turns_minutes < 0 ) turns_minutes= 0;
      else if ( turns_minutes > 9999 ) turns_minutes= 9999;
    }
  else
    {
      if ( turns_minutes < 0 ) turns_minutes= 0;
      else if ( turns_minutes > 59 ) turns_minutes= 59;
    }
  
  // Mostra per pantalla
  if ( !screen_show_status_line ( intp->screen, intp->text.v, score_game,
                                  score_hours, turns_minutes, err ) )
    return false;
  
  return true;
  
} // end show_status_line


static bool
sread_call_routine (
                    Interpreter      *intp,
                    const uint16_t    routine,
                    uint16_t         *result,
                    char            **err
                    )
{

  uint32_t old_PC;
  uint16_t old_frame_ind;
  int ret_inst;
  operand_t ops[1];
  State *state;

  
  // Desa valors
  state= intp->state;
  old_PC= state->PC;
  old_frame_ind= state->frame_ind;
  
  // Crida rutina (Fica en la pila).
  ops[0].type= OP_LARGE;
  ops[0].u16.val= routine;
  if ( !call_routine ( intp, ops, 1, 0, false, err ) )
    return false;

  // Executa fins que tornem al mateix lloc
  do {
    ret_inst= exec_next_inst ( intp, err );
  } while ( ret_inst == RET_CONTINUE &&
            state->frame_ind != old_frame_ind );
  if ( ret_inst != RET_CONTINUE ) return false;
  assert ( old_PC == state->PC );
  
  // Obté resultat
  if ( !state_readvar ( state, 0, result, true, err ) )
    return false;
  
  return true;
  
} // end sread_call_routine


// read en verions >=5
static bool
sread (
       Interpreter      *intp,
       const operand_t  *ops,
       const int         nops,
       const uint8_t     result_var,
       char            **err
       )
{

  uint16_t text_buf,parse_buf,time,routine,result,result_routine;
  uint8_t max_letters,current_letters;
  int n,nread,real_max;
  uint8_t buf[SCREEN_INPUT_TEXT_BUF],zc;
  bool stop,changed,call_routine;
  gint64 time_microsecs,t0,t1,accum_t;

  
  // Parseja opcions.
  time= routine= 0;
  result= 13; // Newline
  if ( (intp->version >= 4 && (nops < 2 || nops > 4)) ||
       (intp->version < 4 && nops != 2) )
    {
      msgerror ( err, "(sread) Expected between 2 and 4 operands but %d found",
                 nops );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &text_buf, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[1]), &parse_buf, err ) ) return false;
  if ( nops > 2 )
    {
      if ( !op_to_u16 ( intp, &(ops[2]), &time, err ) ) return false;
      if ( nops == 4 )
        {
          if ( !op_to_u16 ( intp, &(ops[3]), &routine, err ) ) return false;
        }
    }
  if ( time != 0 && routine != 0 )
    {
      t0= g_get_monotonic_time ();
      accum_t= 0;
      call_routine= true;
      time_microsecs= ((gint64) ((uint64_t) time))*100000;
    }
  else
    {
      call_routine= false;
      t0= 0; // CALLA!!!
      time_microsecs= 0; // CALLA!!
    }

  // Status line
  if ( intp->version <= 3 )
    {
      if ( !show_status_line ( intp, err ) ) return false;
    }
  
  // Obté capacitat màxima caràcters.
  if ( !memory_map_READB ( intp->mem, text_buf, &max_letters, true, err ) )
    return false;
  if ( (max_letters < 3 && intp->version >= 5) ||
       (max_letters == 1 && intp->version <= 4) )
    {
      msgerror ( err, "(sread) Text buffer length (%d) less than 3",
                 max_letters);
      return false;
    }
  
  // Obté caràcters que hi han actualment en el buffer.
  if ( intp->version >= 5 )
    {
      if ( !memory_map_READB ( intp->mem, text_buf+1,
                               &current_letters, true, err ) )
        return false;
      if ( current_letters > max_letters )
        {
          msgerror ( err, "(sread) Text buffer already contains more text"
                     " (%u) than allowed (%u)",
                     current_letters, max_letters );
          return false;
        }
    }
  else current_letters= 0;

  // Llig.
  screen_set_undo_mark ( intp->screen );
  real_max= (int) (max_letters-current_letters);
  if ( real_max > intp->input_text.size )
    {
      intp->input_text.v= g_renew ( uint8_t, intp->input_text.v, real_max );
      intp->input_text.size= real_max;
    }
  intp->input_text.N= 0;
  stop= false;
  if ( !screen_print ( intp->screen, CURSOR, err ) )
    return false;
  do {
    
    // Mentre tinga exit llegint.
    changed= false;
    do {
      if ( !screen_read_char ( intp->screen, buf, &nread, err ) )
        return false;
      if ( nread > 0 ) changed= true;
      for ( n= 0; n < nread && !stop; n++ )
        {
          zc= buf[n];
          if ( zc == ZSCII_NEWLINE ) { stop= true; }
          else if ( zc == ZSCII_DELETE )
            {
              if ( intp->input_text.N > 0 ) --(intp->input_text.N);
            }
          // ASCII o EXTRA
          else if ( (zc >= 32 && zc <= 126) || (zc >= 155 && zc <= 251) )
            {
              if ( intp->input_text.N < real_max )
                {
                  if ( zc >= 'A' && zc <= 'Z' ) zc= (zc-'A')+'a';
                  intp->input_text.v[intp->input_text.N++]= zc;
                }
            }
        }
    } while ( nread > 0 && !stop );

    // Rutina
    if ( call_routine )
      {
        t1= g_get_monotonic_time ();
        accum_t+= t1-t0;
        t0= t1;
        while ( accum_t >= time_microsecs && !stop )
          {
            accum_t-= time_microsecs;
            screen_undo ( intp->screen );
            if ( !sread_call_routine ( intp, routine, &result_routine, err ) )
              return false;
            if ( result_routine != 0 )
              {
                intp->input_text.N= 0;
                stop= true;
                result= 0;
                changed= false;
              }
            else changed= true; // Que torne a pintar
            screen_set_undo_mark ( intp->screen );
          }
      }
    
    // Repinta
    if ( changed )
      {
        screen_undo ( intp->screen );
        if ( !print_input_text ( intp, stop==false, err ) ) return false;
        if ( !stop )
          {
            if ( !screen_print ( intp->screen, CURSOR, err ) )
              return false;
          }
      }
    
    // Espera
    g_usleep ( TIME_SLEEP );
    
  } while ( !stop );
  // --> Pinta retorn carro.
  if ( result != 0 )
    {
      if ( !print_output ( intp, "\n", true, err ) )
        return false;
    }
  
  // Escriu en el text buffer
  if ( intp->version >= 5 )
    {
      if ( !memory_map_WRITEB ( intp->mem, text_buf+1,
                                current_letters+intp->input_text.N,
                                true, err ) )
        return false;
      for ( n= 0; n < intp->input_text.N; ++n )
        {
          if ( !memory_map_WRITEB ( intp->mem,
                                    text_buf + 2 +
                                    ((uint16_t) current_letters) +
                                    ((uint16_t) n),
                                    intp->input_text.v[n],
                                    true, err ) )
            return false;
        }
    }
  else
    {
      for ( n= 0; n < intp->input_text.N; ++n )
        {
          if ( !memory_map_WRITEB ( intp->mem,
                                    text_buf + 1 + ((uint16_t) n),
                                    intp->input_text.v[n],
                                    true, err ) )
            return false;
        }
      if ( !memory_map_WRITEB ( intp->mem,
                                text_buf + 1 + ((uint16_t) n),
                                0, true, err ) )
        return false;
    }
  /* DEBUG!!!
  for ( int i= 0; i < intp->input_text.N; ++i )
    printf("%d ",intp->input_text.v[i]);
  printf("\n");
  */
  
  // Parseja.
  if ( !dictionary_parse ( intp->std_dict, text_buf, parse_buf, err ) )
    return false;

  // Desa valor retorn
  if ( intp->version >= 5 )
    {
      if ( !write_var ( intp, result_var, result, err ) )
        return false;
    }
  
  return true;
  
} // end sread


static bool
read_char (
           Interpreter      *intp,
           const operand_t  *ops,
           const int         nops,
           const uint8_t     result_var,
           char            **err
           )
{

  uint16_t op1,time,routine,result,result_routine;
  int nread;
  uint8_t buf[SCREEN_INPUT_TEXT_BUF];
  bool call_routine;
  gint64 time_microsecs,t0,t1,accum_t;
  
  
  // Parseja opcions.
  time= routine= 0;
  op1= 1;
  if ( nops > 3 )
    {
      msgerror ( err,
                 "Failed to execute read_char: expected between 1"
                 " or 3 operands but %d found", nops );
      return false;
    }
  if ( nops > 0 )
    {
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      if ( nops > 1 )
        {
          if ( !op_to_u16 ( intp, &(ops[1]), &time, err ) ) return false;
          if ( nops == 3 )
            {
              if ( !op_to_u16 ( intp, &(ops[2]), &routine, err ) ) return false;
            }
        }
    }
  if ( op1 != 1 )
    {
      msgerror ( err, "Failed to execute read_char: first operand"
                 " value must be 1, found %u instead", op1 );
      return false;
    }
  if ( time != 0 && routine != 0 )
    {
      t0= g_get_monotonic_time ();
      accum_t= 0;
      call_routine= true;
      time_microsecs= ((gint64) ((uint64_t) time))*100000;
    }
  else call_routine= false;
  
  // Llig caràcter.
  do {

    // Caràcter.
    if ( !screen_read_char ( intp->screen, buf, &nread, err ) )
      return false;

    // Crida rutina
    if ( call_routine )
      {
        t1= g_get_monotonic_time ();
        accum_t+= t1-t0;
        t0= t1;
        if ( accum_t >= time_microsecs )
          {
            accum_t-= time_microsecs;
            if ( !sread_call_routine ( intp, routine, &result_routine, err ) )
              return false;
            if ( result_routine != 0 )
              {
                buf[0]= 0; // '\0'
                nread= 1;
              }
          }
      }
    
    // Força una espera
    g_usleep ( TIME_SLEEP );
    
  } while ( nread == 0 );
  result= (uint16_t) buf[0];
  
  // Desa valor retorn
  if ( !write_var ( intp, result_var, result, err ) )
    return false;
  
  return true;
  
} // end read_char


static uint16_t
save_undo (
           Interpreter *intp
           )
{

  const gchar *undo_fn;
  char *err;

  
  err= NULL;
  undo_fn= saves_get_new_undo_file_name ( intp->saves, &err );
  if ( undo_fn == NULL ) goto error;
  if ( intp->verbose )
    ii ( "Writing undo save file: '%s'", undo_fn );
  if ( !state_save ( intp->state, undo_fn, &err ) ) goto error;
  
  return 1;
  
 error:
  ww ( "Failed to save undo: %s", err );
  g_free ( err );
  return 0;
  
} // end save_undo


static uint16_t
save (
      Interpreter      *intp,
      const operand_t  *ops,
      const int         nops
      )
{

  char *err;
  gchar *save_fn;
  

  if ( nops > 0 )
    {
      ee ( "save - CAL IMPLEMENTAR table bytes" );
    }
  
  err= NULL;
  save_fn= saves_get_save_file_name ( intp->saves, intp->screen,
                                      story_file_GETID ( intp->sf ),
                                      &err );
  if ( save_fn == NULL ) goto error;
  if ( intp->verbose )
    ii ( "Writing save file: '%s'", save_fn );
  if ( !state_save ( intp->state, save_fn, &err ) ) goto error;
  g_free ( save_fn );
  
  return 1;
  
 error:
  ww ( "Failed to save: %s", err );
  g_free ( err );
  g_free ( save_fn );
  return 0;
  
} // end save


static uint16_t
restore_undo (
              Interpreter *intp
              )
{

  const gchar *undo_fn;
  char *err;

  
  err= NULL;
  undo_fn= saves_get_undo_file_name ( intp->saves );
  if ( undo_fn == NULL )
    {
      ww ( "Failed to restore undo: no save file available" );
      return 0;
    }
  if ( intp->verbose )
    ii ( "Reading undo save file: '%s'", undo_fn );
  if ( !state_load ( intp->state, undo_fn, &err ) )
    {
      ww ( "Failed to restore undo: %s", err );
      g_free ( err );
      return 0;
    }
  saves_remove_last_undo_file_name ( intp->saves );
  
  return 2;
  
} // end restore_undo


static uint16_t
restore (
         Interpreter      *intp,
         const operand_t  *ops,
         const int         nops
         )
{

  char *err;
  gchar *save_fn;
  

  if ( nops > 0 )
    {
      ee ( "restore - CAL IMPLEMENTAR table bytes" );
    }
  
  err= NULL;
  save_fn= saves_get_save_file_name ( intp->saves, intp->screen,
                                      story_file_GETID ( intp->sf ),
                                      &err );
  if ( save_fn == NULL ) goto error;
  if ( intp->verbose )
    ii ( "Reading save file: '%s'", save_fn );
  if ( !state_load ( intp->state, save_fn, &err ) ) goto error;
  g_free ( save_fn );
  
  return 2;
  
 error:
  ww ( "Failed to restore: %s", err );
  g_free ( err );
  g_free ( save_fn );
  return 0;
  
} // end restore


static bool
output_stream (
               Interpreter      *intp,
               const operand_t  *ops,
               const int         nops,
               char            **err
               )
{

  uint16_t tmp;
  int16_t number;
  bool select;
  
  
  // Obté primer operand
  if ( nops == 0 )
    {
      msgerror ( err,
                 "Failed to execute output_stream: missing number argument" );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &tmp, err ) ) return false;
  number= (int16_t) tmp;
  if ( number == 0 ) return true; // No fa res

  // Comprovació operadors i select
  if ( (number != 3 && nops != 1) ||
       (number == 3 && (nops < 2 || nops > 3)) )
    goto wrong_args;
  if ( nops == 3 && intp->version != 6 ) goto wrong_args;
  if ( number > 0 ) { select= true; }
  else              { select= false; number= -number; }

  // Accions especial output stream 3
  if ( number == 3 )
    {
      if ( select )
        {
          if ( intp->ostreams.N3 == INTP_MAX_OSTREAM3 )
            {
              msgerror ( err,
                         "Failed to execute output_stream 3: reached"
                         " maximum number of active output stream 3" );
              return false;
            }
          if ( !op_to_u16 ( intp, &(ops[1]), &tmp, err ) ) return false;
          intp->ostreams.o3[intp->ostreams.N3].addr= (uint32_t) tmp;
          intp->ostreams.o3[intp->ostreams.N3++].N= 0;
        }
      // Ignora si no hi ha un stream seleccionat prèviament.
      else if ( intp->ostreams.N3 > 0 )
        {
          --intp->ostreams.N3;
          if ( !memory_map_WRITEW ( intp->mem,
                                    intp->ostreams.o3[intp->ostreams.N3].addr,
                                    intp->ostreams.o3[intp->ostreams.N3].N,
                                    true, err ) )
            return false;
        }
    }

  // Selecciona/Deselecciona
  if ( select ) intp->ostreams.active|= 0x1<<(number-1);
  else          intp->ostreams.active&= ~(0x1<<(number-1));
  
  return true;
  
 wrong_args:
  msgerror ( err,
             "Failed to execute output_stream: wrong number of arguments" );
  return false;
  
} // end output_stream


static bool
throw_inst (
            Interpreter      *intp,
            const uint16_t    value,
            const uint16_t    stack_frame,
            char            **err
            )
{

  // NOTA!! No queda clar com funciona açò, faré la implementació més
  // senzilla, si el stack_frame és superior a l'actual ignora.
  if ( stack_frame > intp->state->frame_ind )
    {
      msgerror ( err,
                 "Failed to execute throw: provided stack frame (%u)"
                 " greater than current one (%u)",
                 stack_frame, intp->state->frame_ind );
      return false;
    }
  while ( stack_frame < intp->state->frame_ind )
    {
      if ( !state_free_frame ( intp->state, err ) ) return false;
    }
  if ( !ret_val ( intp, value, err ) ) return false;

  return true;
  
} // end throw_inst


static bool
scan_table (
            Interpreter      *intp,
            const operand_t  *ops,
            const int         nops,
            const uint8_t     result_var,
            bool             *cond,
            char            **err
            )
{

  uint16_t x,table,len,form,field_size,addr,tmp16,res;
  uint32_t n;
  uint8_t tmp8;
  bool is_word;
  

  // Prepara.
  *cond= false;
  
  // Obté paràmetres
  if ( nops < 3 || nops > 4 )
    {
      msgerror ( err, "Failed to scan table: wrong number of arguments" );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &x, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[1]), &table, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[2]), &len, err ) ) return false;
  if ( nops == 4 )
    {
      if ( !op_to_u16 ( intp, &(ops[3]), &form, err ) ) return false;
    }
  else form= 0x82;
  is_word= (form&0x80)!=0;
  field_size= form&0x7f;
  
  // Cerca
  res= 0;
  addr= table;
  if ( field_size > 0 && len > 0 )
    {
      for ( n= 0; n < (uint32_t) len && !(*cond); ++n )
        {
          
          // Compara
          if ( is_word )
            {
              if ( !memory_map_READW ( intp->mem, addr, &tmp16, true, err ) )
                return false;
              if ( tmp16 == x ) { res= addr; *cond= true; }
            }
          else
            {
              if ( !memory_map_READB ( intp->mem, addr, &tmp8, true, err ) )
                return false;
              if ( (uint16_t) tmp8 == x ) { res= addr; *cond= true; }
            }
          
          // Incrementa
          addr+= field_size;
          
        }
    }

  // Escriu resultat
  if ( !write_var ( intp, result_var, res, err ) ) return false;
  
  return true;
  
} // end scan_table


static bool
copy_table (
            Interpreter     *intp,
            const uint16_t   first,
            const uint16_t   second,
            const uint16_t   size,
            char           **err
            )
{

  uint16_t beg,end,p,q;
  int16_t len;
  uint8_t val;
  bool forward;
  
  
  if ( size == 0 ) return true;

  // Cas especial. Si second és 0 es fica a 0 first.
  if ( second == 0 )
    {
      len= (int16_t) size;
      if ( len < 0 ) len= -len;
      beg= first; end= first + len;
      for ( p= beg; p != end; ++p )
        {
          if ( !memory_map_WRITEB ( intp->mem, p, 0x00, true, err ) )
            return false;
        }
    }

  // Cas normal. Copia first en second
  else
    {

      // Decideix direcció.
      len= (int16_t) size;
      if ( len > 0 ) forward= (first > second);
      else           { len= -len; forward= true; }

      // Còpia.
      if ( forward )
        {
          beg= first; end= first + len;
          for ( p= beg, q= second; p != end; ++p, ++q )
            {
              if ( !memory_map_READB ( intp->mem, p, &val, true, err ) )
                return false;
              if ( !memory_map_WRITEB ( intp->mem, q, val, true, err ) )
                return false;
            }
        }
      else
        {
          beg= first + (len-1); end= first-1;
          for ( p= beg, q= second + (len-1); p != end; --p, --q )
            {
              if ( !memory_map_READB ( intp->mem, p, &val, true, err ) )
                return false;
              if ( !memory_map_WRITEB ( intp->mem, q, val, true, err ) )
                return false;
            }
        }
      
    }
  
  return true;
  
} // end copy_table


static bool
tokenise (
          Interpreter      *intp,
          const operand_t  *ops,
          const int         nops,
          char            **err
          )
{

  uint16_t text,parse,dictionary,flag;
  bool use_dictionary;

  
  // Obté paràmetres
  flag= 0;
  use_dictionary= false;
  if ( nops < 2 || nops > 4 )
    {
      msgerror ( err, "Failed to execute tokenise: wrong number of arguments" );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &text, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[1]), &parse, err ) ) return false;
  if ( nops >= 3 )
    {
      use_dictionary= true;
      if ( !op_to_u16 ( intp, &(ops[2]), &dictionary, err ) ) return false;
      if ( nops == 4 )
        {
          if ( !op_to_u16 ( intp, &(ops[3]), &flag, err ) ) return false;
        }
    }
  if ( use_dictionary ) ee ( "tokenise - CAL_IMPLEMENTAR DICTIONARY" );
  if ( flag!=0 ) ee ( "tokenise - CAL_IMPLEMENTAR FLAG" );

  // Parseja
  if ( !dictionary_parse ( intp->std_dict, text, parse, err ) )
    return false;

  return true;
  
} // end tokenise


// Si el color no és suportat torna un no suportat
static uint16_t
color2true_color (
                  const uint16_t color
                  )
{

  uint16_t ret;


  switch ( color )
    {
    case 0x0000: ret= 0xFFFE; break; // Current
    case 0x0001: ret= 0xFFFF; break; // Default
    case 0x0002: ret= 0x0000; break; // Black
    case 0x0003: ret= 0x001D; break; // Red
    case 0x0004: ret= 0x0340; break; // Green
    case 0x0005: ret= 0x03BD; break; // Yellow
    case 0x0006: ret= 0x59A0; break; // Blue
    case 0x0007: ret= 0x7C1F; break; // Magenta
    case 0x0008: ret= 0x77A0; break; // Cyan
    case 0x0009: ret= 0x7FFF; break; // White
    case 0x000A: ret= 0x5AD6; break; // Light grey
    case 0x000B: ret= 0x4631; break; // Medium grey
    case 0x000C: ret= 0x2D6B; break; // Dark grey
    case 0xFFFF: ret= 0xFFFD; break; // Color baix cursor (V6)
    default:     ret= 0x8000; // <-- Valor no suportat que s'ignorarà.
    }

  return ret;
  
} // end color2true_color


static bool
quit (
      Interpreter  *intp,
      char        **err
      )
{

  int nread;
  uint8_t buf[SCREEN_INPUT_TEXT_BUF];
  
  
  if ( !screen_print ( intp->screen, "\n", err ) )
    return false;
  if ( !screen_print ( intp->screen, _("[Press any key to exit]"), err ) )
    return false;
  
  do {
    
    // Caràcter.
    if ( !screen_read_char ( intp->screen, buf, &nread, err ) )
      return false;
    
    // Força una espera
    g_usleep ( TIME_SLEEP );
    
  } while ( nread == 0 );
  
  return true;
  
} // end quit


static bool
inst_be (
         Interpreter  *intp,
         char        **err
         )
{

  State *state;
  uint8_t opcode,result_var;
  uint16_t op1,op2,res;
  int16_t places;
  int nops;
  operand_t ops[8];
  bool cond1,cond2;
  
  
  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &opcode, true, err ) )
    return false;
  switch ( opcode )
    {
    case 0x00: // save
      if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                 false, &result_var, err ) )
        return false;
      res= save ( intp, ops, nops );
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x01: // restore
      if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                 false, &result_var, err ) )
        return false;
      res= restore ( intp, ops, nops );
      if ( res == 2 ) // Exit
        {
          if ( !memory_map_READB ( intp->mem, intp->state->PC-1,
                                   &result_var, true, err ) )
            return false;
        }
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x02: // log_shift
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return false;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return false;
      places= (int16_t) op2;
      if ( places > 0 )       res= op1<<places;
      else if ( places == 0 ) res= op1;
      else                    res= op1>>(-places);
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x03: // art_shift
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return false;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return false;
      places= (int16_t) op2;
      if ( places > 0 )       res= op1<<places;
      else if ( places == 0 ) res= op1;
      else                    res= (uint16_t) (((int16_t) op1)>>(-places));
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x04: // set_font
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_font not implemented in version 6" );
          return false;
        }
      else
        {
          if ( !read_var_ops_store ( intp, ops, &nops, 1,
                                     false, &result_var, err ) )
            return false;
          if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
          res= screen_set_font ( intp->screen, op1 );
          if ( !write_var ( intp, result_var, res, err ) ) return false;
        }
      break;

    case 0x09: // save_undo
      if ( !read_var_ops_store ( intp, ops, &nops, 0,
                                 false, &result_var, err ) )
        return false;
      res= save_undo ( intp );
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x0a: // restore_undo
      if ( !read_var_ops_store ( intp, ops, &nops, 0,
                                 false, &result_var, err ) )
        return false;
      res= restore_undo ( intp );
      if ( res == 2 ) // Exit
        {
          if ( !memory_map_READB ( intp->mem, intp->state->PC-1,
                                   &result_var, true, err ) )
            return false;
        }
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x0b: // print_unicode
      if ( !read_var_ops ( intp, ops, &nops, 1, false,err ) ) return false;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      if ( !print_unicode ( intp, op1, err ) ) return false;
      break;
    case 0x0c: // check_unicode;
      if ( !read_var_ops_store ( intp, ops, &nops, 1,
                                 false, &result_var, err ) )
        return false;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      screen_check_unicode ( intp->screen, op1, &cond1, &cond2 );
      res=
        (cond1 ? 0x1 : 0x0) | // output
        (cond2 ? 0x2 : 0x0)   // input
        ;
      if ( !write_var ( intp, result_var, res, err ) ) return false;
      break;
    case 0x0d: // set_true_colour
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_true_colour not implemented in version 6" );
          return false;
        }
      else
        {
          if ( !read_var_ops ( intp, ops, &nops, 2, false,err ) ) return false;
          if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
          if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return false;
          screen_set_colour ( intp->screen, op1, op2 );
        }
      break;
      
    default: // Unknown
      msgerror ( err, "Unknown instruction opcode BE %02X (%d)",
                 opcode, opcode );
      return false;
    }
  
  return true;
  
} // end inst_be


static int
exec_next_inst (
                Interpreter  *intp,
                char        **err
                )
{

  int nops,n;
  uint8_t opcode,result_var,op1_u8,op2_u8,res_u8,ref;
  uint16_t op1,op2,op3,res,tmp16;
  uint32_t addr;
  State *state;
  operand_t ops[8];
  bool cond;
  

  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &opcode, true, err ) )
    return RET_ERROR;
  switch ( opcode )
    {

    case 0x01: // je
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, op1 == op2, err ) ) return RET_ERROR;
      break;
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
    case 0x04: // dec_chk
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err )) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, (int16_t) op1 < (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x05: // inc_chk
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err )) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, (int16_t) op1 > (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x06: // jin
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !jin ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x07: // test
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, (op1&op2) == op2, err ) ) return RET_ERROR;
      break;
    case 0x08: // or
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      res= op1 | op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x09: // and
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      res= op1 & op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x0a: // test_attr
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !test_attr ( intp, op1, op2, &cond, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x0b: // set_attr
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !set_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x0c: // clear_attr
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !clear_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x0d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, op1_u8, op2, err ) ) return RET_ERROR;
      break;
    case 0x0e: // insert_obj
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !insert_obj ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x0f: // loadw
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x10: // loadb
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x11: // get_prop
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !get_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x12: // get_prop_addr
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !get_prop_addr ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x13: // get_next_prop
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !get_next_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x14: // add
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      res= (uint16_t) (((int16_t) op1) + ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x15: // sub
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x16: // mul
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      res= S32_U16(U16_S32(op1) * U16_S32(op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x17: // div
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) / ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x18: // mod
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) % ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x19: // call_2s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].type= OP_SMALL;
      ops[1].type= OP_SMALL;
      if ( !read_small_small_store ( intp, &(ops[0].u8.val),
                                     &(ops[1].u8.val), &result_var, err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x1a: // call_2n
      if ( intp->version < 5 ) goto wrong_version;
      ops[0].type= OP_SMALL;
      ops[1].type= OP_SMALL;
      if ( !read_small_small ( intp, &(ops[0].u8.val), &(ops[1].u8.val), err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, 0, true, err ) ) return RET_ERROR;
      break;
    case 0x1b: // set_colour
      if ( intp->version < 5 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_colour not implemented in version 6" );
          return RET_ERROR;
        }
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      screen_set_colour ( intp->screen,
                          color2true_color ( op1 ),
                          color2true_color ( op2 ) );
      break;
    case 0x1c: // throw
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      if ( !throw_inst ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
      
    case 0x21: // je
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !branch ( intp, op1 == op2, err ) ) return RET_ERROR;
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
    case 0x24: // dec_chk
      if ( !read_small_var ( intp, &op1_u8, &op2, err )) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 < (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x25: // inc_chk
      if ( !read_small_var ( intp, &op1_u8, &op2, err )) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 > (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x26: // jin
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !jin ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x27: // test
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !branch ( intp, (op1&op2)==op2, err ) ) return RET_ERROR;
      break;
    case 0x28: // or
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      res= op1 | op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x29: // and
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      res= op1 & op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x2a: // test_attr
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !test_attr ( intp, op1, op2, &cond, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x2b: // set_attr
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !set_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x2c: // clear_attr
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !clear_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x2d: // store
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, op1_u8, op2, err ) ) return RET_ERROR;
      break;
    case 0x2e: // insert_obj
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !insert_obj ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x2f: // loadw
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x30: // loadb
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x31: // get_prop
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x32: // get_prop_addr
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_prop_addr ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x33: // get_next_prop
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_next_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x34: // add
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      res= (uint16_t) (((int16_t) op1) + ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x35: // sub
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x36: // mul
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      res= S32_U16(U16_S32(op1) * U16_S32(op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x37: // div
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) / ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x38: // mod
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) % ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x39: // call_2s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].type= OP_SMALL;
      ops[1].type= OP_VARIABLE;
      if ( !read_small_small_store ( intp, &(ops[0].u8.val),
                                     &(ops[1].u8.val), &result_var, err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x3a: // call_2n
      if ( intp->version < 5 ) goto wrong_version;
      ops[0].type= OP_SMALL;
      ops[1].type= OP_VARIABLE;
      if ( !read_small_small ( intp, &(ops[0].u8.val), &(ops[1].u8.val), err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, 0, true, err ) ) return RET_ERROR;
      break;
    case 0x3b: // set_colour
      if ( intp->version < 5 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_colour not implemented in version 6" );
          return RET_ERROR;
        }
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      screen_set_colour ( intp->screen,
                          color2true_color ( op1 ),
                          color2true_color ( op2 ) );
      break;
    case 0x3c: // throw
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !throw_inst ( intp, op1, op2, err ) ) return RET_ERROR;
      break;

    case 0x41: // je
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, op1 == op2, err ) ) return RET_ERROR;
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
    case 0x44: // dec_chk
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err )) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, ref, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, ref, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 < (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x45: // inc_chk
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err )) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, ref, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, ref, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 > (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0x46: // jin
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !jin ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x47: // test
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !branch ( intp, (op1&op2) == op2, err ) ) return RET_ERROR;
      break;
    case 0x48: // or
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      res= op1 | op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x49: // and
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      res= op1 & op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x4a: // test_attr
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !test_attr ( intp, op1, op2, &cond, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x4b: // set_attr
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !set_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x4c: // clear_attr
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !clear_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x4d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( ref == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, ref, op2, err ) ) return RET_ERROR;
      break;
    case 0x4e: // insert_obj
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !insert_obj ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x4f: // loadw
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x50: // loadb
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x51: // get_prop
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !get_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x52: // get_prop_addr
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !get_prop_addr ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x53: // get_next_prop
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !get_next_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x54: // add
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      res= (uint16_t) (((int16_t) op1) + ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x55: // sub
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x56: // mul
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      res= S32_U16(U16_S32(op1) * U16_S32(op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x57: // div
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) / ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x58: // mod
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) % ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x59: // call_2s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].type= OP_VARIABLE;
      ops[1].type= OP_SMALL;
      if ( !read_small_small_store ( intp, &(ops[0].u8.val),
                                     &(ops[1].u8.val), &result_var, err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x5a: // call_2n
      if ( intp->version < 5 ) goto wrong_version;
      ops[0].type= OP_VARIABLE;
      ops[1].type= OP_SMALL;
      if ( !read_small_small ( intp, &(ops[0].u8.val), &(ops[1].u8.val), err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, 0, true, err ) ) return RET_ERROR;
      break;
    case 0x5b: // set_colour
      if ( intp->version < 5 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_colour not implemented in version 6" );
          return RET_ERROR;
        }
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      screen_set_colour ( intp->screen,
                          color2true_color ( op1 ),
                          color2true_color ( op2 ) );
      break;
    case 0x5c: // throw
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_small ( intp, &op1, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !throw_inst ( intp, op1, op2, err ) ) return RET_ERROR;
      break;

    case 0x61: // je
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, op1 == op2, err ) ) return RET_ERROR;
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
    case 0x64: // dec_chk
      msgerror ( err, "Failed to execute @dec_chnk: Trying to reference"
                 " a variable with a variable" );
      return RET_ERROR;
      break;
    case 0x65: // inc_chk
      msgerror ( err, "Failed to execute @inc_chnk: Trying to reference"
                 " a variable with a variable" );
      return RET_ERROR;
      break;
    case 0x66: // jin
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !jin ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x67: // test
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, (op1&op2) == op2, err ) ) return RET_ERROR;
      break;
    case 0x68: // or
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      res= op1 | op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x69: // and
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      res= op1 & op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x6a: // test_attr
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !test_attr ( intp, op1, op2, &cond, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x6b: // set_attr
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !set_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x6c: // clear_attr
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !clear_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x6d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, op2_u8, &op2, err ) ) return RET_ERROR;
      if ( ref == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, ref, op2, err ) ) return RET_ERROR;
      break;
    case 0x6e: // insert_obj
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !insert_obj ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0x6f: // loadw
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x70: // loadb
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x71: // get_prop
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      if ( !get_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x72: // get_prop_addr
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      if ( !get_prop_addr ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x73: // get_next_prop
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      if ( !get_next_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x74: // add
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      res= (uint16_t) (((int16_t) op1) + ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x75: // sub
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x76: // mul
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      res= S32_U16(U16_S32(op1) * U16_S32(op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x77: // div
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) / ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x78: // mod
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) % ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x79: // call_2s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].type= OP_VARIABLE;
      ops[1].type= OP_VARIABLE;
      if ( !read_small_small_store ( intp, &(ops[0].u8.val),
                                     &(ops[1].u8.val), &result_var, err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x7a: // call_2n
      if ( intp->version < 5 ) goto wrong_version;
      ops[0].type= OP_VARIABLE;
      ops[1].type= OP_VARIABLE;
      if ( !read_small_small ( intp, &(ops[0].u8.val), &(ops[1].u8.val), err ))
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 2, 0, true, err ) ) return RET_ERROR;
      break;
    case 0x7b: // set_colour
      if ( intp->version < 5 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_colour not implemented in version 6" );
          return RET_ERROR;
        }
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      screen_set_colour ( intp->screen,
                          color2true_color ( op1 ),
                          color2true_color ( op2 ) );
      break;
    case 0x7c: // throw
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_var ( intp, &op1, &op2, err ) ) return RET_ERROR;
      if ( !throw_inst ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
      
    case 0x80: // jz
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !branch ( intp, op1 == 0, err ) ) return RET_ERROR;
      break;
    case 0x81: // get_sibling
      if ( !read_op1_large_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_sibling ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x82: // get_child
      if ( !read_op1_large_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_child ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x83: // get_parent
      if ( !read_op1_large_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_parent ( intp, op1, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x84: // get_prop_len
      if ( !read_op1_large_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_prop_len ( intp, op1, &res_u8, err ) ) return RET_ERROR;
      res= (uint8_t) res_u8;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x85: // inc
      msgerror ( err, "Failed to execute @inc: Trying to reference"
                 " a variable with a large constant" );
      return RET_ERROR;
      break;
    case 0x86: // dec
      msgerror ( err, "Failed to execute @dec: Trying to reference"
                 " a variable with a large constant" );
      return RET_ERROR;
      break;
    case 0x87: // print_addr
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !print_addr ( intp, op1, NULL, false, err ) ) return RET_ERROR;
      break;
    case 0x88: // call_1s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].u16.type= OP_LARGE;
      if ( !read_op1_large_store ( intp, &(ops[0].u16.val), &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 1, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x89: // remove_obj
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !remove_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x8a: // print_obj
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !print_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x8b: // ret
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !ret_val ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x8c: // jump
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= (uint32_t) U16_S32(op1);
      break;
    case 0x8d: // print_paddr
      if ( !memory_map_READW ( intp->mem, state->PC, &op1, true, err ) )
        return RET_ERROR;
      state->PC+= 2;
      if ( !print_paddr ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x8e: // load
      msgerror ( err, "Failed to execute @load: Trying to reference"
                 " a variable with a large constant" );
      return RET_ERROR;
      break;
    case 0x8f: // call_1n / not
      if ( intp->version >= 5 )
        {
          ops[0].u16.type= OP_LARGE;
          if ( !memory_map_READW ( intp->mem, intp->state->PC,
                                   &(ops[0].u16.val), true, err ) )
            return RET_ERROR;
          intp->state->PC+= 2;
          if ( !call_routine ( intp, ops, 1, 0, true, err ) )
            return RET_ERROR;
        }
      else
        {
          msgerror ( err, "@not not implemented in version %d", intp->version );
          return RET_ERROR;
        }
      break;
    case 0x90: // jz
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      if ( !branch ( intp, op1_u8 == 0, err ) ) return RET_ERROR;
      break;
    case 0x91: // get_sibling
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_sibling ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x92: // get_child
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_child ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0x93: // get_parent
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_parent ( intp, op1, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x94: // get_prop_len
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !get_prop_len ( intp, op1, &res_u8, err ) ) return RET_ERROR;
      res= (uint8_t) res_u8;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x95: // inc
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      break;
    case 0x96: // dec
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      break;
    case 0x97: // print_addr
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !print_addr ( intp, op1, NULL, false, err ) ) return RET_ERROR;
      break;
    case 0x98: // call_1s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].u8.type= OP_SMALL;
      if ( !read_op1_small_store ( intp, &(ops[0].u8.val), &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 1, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0x99: // remove_obj
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !remove_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x9a: // print_obj
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !print_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x9b: // ret
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !ret_val ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x9c: // jump
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      state->PC+= ((uint32_t) U16_S32(op1))-2;
      break;
    case 0x9d: // print_paddr
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      if ( !print_paddr ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0x9e: // load
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      if ( !read_var_nopop ( intp, op1_u8, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x9f: // call_1n / not
      if ( intp->version >= 5 )
        {
          ops[0].u8.type= OP_SMALL;
          if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                                   &(ops[0].u8.val), true, err ) )
            return RET_ERROR;
          if ( !call_routine ( intp, ops, 1, 0, true, err ) )
            return RET_ERROR;
        }
      else
        {
          msgerror ( err, "@not not implemented in version %d", intp->version );
          return RET_ERROR;
        }
      break;
    case 0xa0: // jz
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, op1 == 0, err ) ) return RET_ERROR;
      break;
    case 0xa1: // get_sibling
      if ( !read_op1_var_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_sibling ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0xa2: // get_child
      if ( !read_op1_var_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_child ( intp, op1, &res, &cond, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0xa3: // get_parent
      if ( !read_op1_var_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_parent ( intp, op1, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xa4: // get_prop_len
      if ( !read_op1_var_store ( intp, &op1, &result_var, err ) )
        return RET_ERROR;
      if ( !get_prop_len ( intp, op1, &res_u8, err ) ) return RET_ERROR;
      res= (uint8_t) res_u8;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xa5: // inc. NOTA!! Açò és possible?
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, ref, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, ref, op1, err ) ) return RET_ERROR;
      break;
    case 0xa6: // dec. NOTA!! Açò és possible?
      if ( !memory_map_READB ( intp->mem, state->PC++, &op1_u8, true, err ) )
        return RET_ERROR;
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, ref, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, ref, op1, err ) ) return RET_ERROR;
      break;
    case 0xa7: // print_addr
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !print_addr ( intp, op1, NULL, false, err ) ) return RET_ERROR;
      break;
    case 0xa8: // call_1s
      if ( intp->version < 4 ) goto wrong_version;
      ops[0].u8.type= OP_VARIABLE;
      if ( !read_op1_small_store ( intp, &(ops[0].u8.val), &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, 1, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0xa9: // remove_obj
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !remove_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xaa: // print_obj
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !print_obj ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xab: // ret
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !ret_val ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xac: // jump
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      state->PC+= ((uint32_t) U16_S32(op1))-2;
      break;
    case 0xad: // print_paddr
      if ( !read_op1_var ( intp, &op1, err ) ) return RET_ERROR;
      if ( !print_paddr ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xae: // load. NOTA!! Açò és possible?
      if ( !read_op1_small_store ( intp, &op1_u8, &result_var, err ) )
        return RET_ERROR;
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var_nopop ( intp, ref, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xaf: // call_1n / not
      if ( intp->version >= 5 )
        {
          ops[0].u8.type= OP_VARIABLE;
          if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                                   &(ops[0].u8.val), true, err ) )
            return RET_ERROR;
          if ( !call_routine ( intp, ops, 1, 0, true, err ) )
            return RET_ERROR;
        }
      else
        {
          msgerror ( err, "@not not implemented in version %d", intp->version );
          return RET_ERROR;
        }
      break;
    case 0xb0: // rtrue
      if ( !ret_val ( intp, 1, err ) ) return RET_ERROR;
      break;
    case 0xb1: // rfalse
      if ( !ret_val ( intp, 0, err ) ) return RET_ERROR;
      break;
    case 0xb2: // print
      if ( !print ( intp, err ) ) return RET_ERROR;
      break;
    case 0xb3: // print_ret
      if ( !print ( intp, err ) ) return RET_ERROR;
      if ( !print_output ( intp, "\n", false, err ) ) return RET_ERROR;
      if ( !ret_val ( intp, 1, err ) ) return RET_ERROR;
      break;
    case 0xb4: // nop
      break;
    case 0xb5: // save
      if ( intp->version < 4 )
        {
          res= save ( intp, ops, 0 );
          if ( !branch ( intp, res==1, err ) ) return RET_ERROR;
        }
      else if ( intp->version == 4 )
        {
          if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                                   &result_var, true, err ) )
            return RET_ERROR;
          res= save ( intp, ops, 0 );
          if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
        }
      else goto wrong_version;
      break;
    case 0xb6: // restore
      if ( intp->version < 4 )
        {
          res= restore ( intp, ops, 0 );
          if ( !branch ( intp, res==2, err ) ) return RET_ERROR;
        }
      else if ( intp->version == 4 )
        {
          if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                                   &result_var, true, err ) )
            return RET_ERROR;
          res= restore ( intp, ops, 0 );
          if ( res == 2 )
            {
              if ( !memory_map_READB ( intp->mem, intp->state->PC-1,
                                       &result_var, true, err ) )
                return RET_ERROR;
            }
          if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
        }
      else goto wrong_version;
      break;
    case 0xb7: // restart
      if ( !state_restart ( intp->state, err ) ) return RET_ERROR;
      if ( intp->version <= 3 )
        {
          if ( !show_status_line ( intp, err ) ) return RET_ERROR;
        }
      break;
    case 0xb8: // ret_popped
      if ( !state_readvar ( intp->state, 0, &op1, true, err ) )
        return RET_ERROR;
      if ( !ret_val ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xb9: // catch
      if ( intp->version >= 5 )
        {
          if ( !memory_map_READB ( intp->mem, intp->state->PC++,
                                   &result_var, true, err ) )
            return RET_ERROR;
          res= intp->state->frame_ind;
          if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
        }
      else
        {
          msgerror ( err, "pop not implemented in version %d", intp->version );
          return RET_ERROR;
        }
      break;
    case 0xba: // quit
      if ( !quit ( intp, err ) ) return RET_ERROR;
      return RET_STOP;
      break;
    case 0xbb: // new_line
      if ( !print_output ( intp, "\n", false, err ) ) return RET_ERROR;
      break;
    case 0xbc: // show_status
      if ( intp->version < 3 ) goto wrong_version;
      if ( intp->version == 3 ) // >3 com si fora NOP
        {
          if ( !show_status_line ( intp, err ) ) return RET_ERROR;
        }
      break;
      
    case 0xbe: // extended
      if ( intp->version < 5 ) goto wrong_version;
      if ( !inst_be ( intp, err ) ) return RET_ERROR;
      break;
      
    case 0xc1: // je
      if ( !read_var_ops ( intp, ops, &nops, -1,
                           false, err ) ) return RET_ERROR;
      if ( nops == 0 )
        {
          msgerror ( err, "(je) Expected at least 1 operand but 0 found" );
          return RET_ERROR;
        }
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      cond= false;
      for ( n= 1; n < nops && !cond; ++n )
        {
          if ( !op_to_u16 ( intp, &(ops[n]), &op2, err ) ) return RET_ERROR;
          if ( op1 == op2 ) cond= true;
        }
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0xc2: // jl
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, ((int16_t) op1) < ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0xc3: // jg
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, ((int16_t) op1) > ((int16_t) op2), err ) )
        return RET_ERROR;
      break;
    case 0xc4: // dec_chk
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_refvar ( intp, &(ops[0]), &op1_u8, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      --op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 < (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0xc5: // inc_chk
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_refvar ( intp, &(ops[0]), &op1_u8, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !read_var ( intp, op1_u8, &op1, err ) ) return RET_ERROR;
      ++op1;
      if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (int16_t) op1 > (int16_t) op2, err ) )
        return RET_ERROR;
      break;
    case 0xc6: // jin
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !jin ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0xc7: // test
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !branch ( intp, (op1&op2) == op2, err ) ) return RET_ERROR;
      break;
    case 0xc8: // or
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      res= op1|op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xc9: // and
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      res= op1&op2;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xca: // test_attr
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !test_attr ( intp, op1, op2, &cond, err ) ) return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0xcb: // set_attr
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !set_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0xcc: // clear_attr
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !clear_attr ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0xcd: // store
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( ops[0].type != OP_SMALL )
        {
          msgerror ( err, "Failed to execute @store: Trying to"
                     " reference a variable with a non small constant" );
          return RET_ERROR;
        }
      op1_u8= ops[0].u8.val;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, op1_u8, op2, err ) ) return RET_ERROR;
      break;
    case 0xce: // insert_obj
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !insert_obj ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
    case 0xcf: // loadw
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd0: // loadb
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd1: // get_prop
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !get_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd2: // get_prop_addr
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !get_prop_addr ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd3: // get_next_prop
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !get_next_prop ( intp, op1, op2, &res, err ) ) return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd4: // add
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      res= (uint16_t) (((int16_t) op1) + ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd5: // sub
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      res= (uint16_t) (((int16_t) op1) - ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd6: // mul
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      res= S32_U16(U16_S32(op1) * U16_S32(op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd7: // div
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) / ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd8: // mod
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( op2 == 0 ) goto division0;
      res= (uint16_t) (((int16_t) op1) % ((int16_t) op2));
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xd9: // call_2s
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0xda: // call_2n
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, 0, true, err ) ) return RET_ERROR;
      break;
    case 0xdb: // set_colour
      if ( intp->version < 5 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "@set_colour not implemented in version 6" );
          return RET_ERROR;
        }
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      screen_set_colour ( intp->screen,
                          color2true_color ( op1 ),
                          color2true_color ( op2 ) );
      break;
    case 0xdc: // throw
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 2, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !throw_inst ( intp, op1, op2, err ) ) return RET_ERROR;
      break;
      
    case 0xe0: // call_vs
      if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0xe1: // storew
      if ( !read_var_ops ( intp, ops, &nops, 3, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[2]), &op3, err ) ) return RET_ERROR;
      addr= (uint16_t) (op1 + (uint16_t) (2*((int16_t) op2)));
      if ( !memory_map_WRITEW ( intp->mem, addr, op3, false, err ) )
        return RET_ERROR;
      break;
    case 0xe2: // storeb
      if ( !read_var_ops ( intp, ops, &nops, 3, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[2]), &op3, err ) ) return RET_ERROR;
      addr= (uint16_t) (op1 + op2);
      if ( !memory_map_WRITEB ( intp->mem, addr,
                                (uint8_t) op3, false, err ) )
        return RET_ERROR;
      break;
    case 0xe3: // put_prop
      if ( !read_var_ops ( intp, ops, &nops, 3, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[2]), &op3, err ) ) return RET_ERROR;
      if ( !put_prop ( intp, op1, op2, op3, err ) ) return RET_ERROR;
      break;
    case 0xe4: // read
      if ( intp->version >= 5 )
        {
          if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                     false, &result_var, err ) )
            return RET_ERROR;
          if ( !sread ( intp, ops, nops, result_var, err ) ) return RET_ERROR;
        }
      else
        {
          if ( !read_var_ops ( intp, ops, &nops, -1, false, err ) )
            return RET_ERROR;
          // S'ignora el result_var
          if ( !sread ( intp, ops, nops, 0, err ) ) return RET_ERROR;
        }
      break;
    case 0xe5: // print_char
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !print_char ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xe6: // print_num
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !print_num ( intp, op1, err ) ) return RET_ERROR;
      break;
    case 0xe7: // random
      if ( !read_var_ops_store ( intp, ops, &nops, 1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( ((int16_t) op1) > 0  )
        res= ((random_next ( intp ) - 1)%op1) + 1;
      else // seed
        {
          res= 0;
          random_set_seed ( intp, -((int16_t) op1) );
        }
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xe8: // push
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !write_var ( intp, 0, op1, err ) ) return RET_ERROR;
      break;
    case 0xe9: // pull
      if ( intp->version == 6 )
        {
          msgerror ( err, "pull not implemented in version 6" );
          return RET_ERROR;
        }
      else
        {
          if ( !read_var_ops ( intp, ops, &nops, 1,
                               false, err ) ) return RET_ERROR;
          if ( !op_to_refvar ( intp, &(ops[0]), &op1_u8, err ) )
            return RET_ERROR;
          if ( !read_var ( intp, 0, &op1, err ) ) return RET_ERROR;
          if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
            {
              ww ( "pull - Using stack as variable" );
              if ( !read_var ( intp, 0, &op2, err ) ) return RET_ERROR;
            }
          if ( !write_var ( intp, op1_u8, op1, err ) ) return RET_ERROR;
        }
      break;
    case 0xea: // split_window
      if ( intp->version < 3 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !screen_split_window ( intp->screen, (int16_t) op1, err ) )
        return RET_ERROR;
      break;
    case 0xeb: // set_window
      if ( intp->version < 3 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !screen_set_window ( intp->screen, (int16_t) op1, err ) )
        return RET_ERROR;
      break;
    case 0xec: // call_vs2
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, -1, true,
                                 &result_var, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, result_var, false, err ) )
        return RET_ERROR;
      break;
    case 0xed: // erase_window
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !screen_erase_window ( intp->screen, (int16_t) op1, err ) )
        return RET_ERROR;
      break;

    case 0xef: // set_cursor
      if ( intp->version < 4 ) goto wrong_version;
      if ( intp->version == 6 )
        {
          msgerror ( err, "set_cursor not implemented in version 6" );
          return RET_ERROR;
        }
      else
        {
          if ( !read_var_ops ( intp, ops, &nops, -1, false, err ) )
            return RET_ERROR;
          if ( nops < 1 || nops > 2 )
            {
              msgerror ( err, "set_cursor wrong number of parameters" );
              return RET_ERROR;
            }
          if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
          if ( nops == 2 )
            {
              if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
            }
          else op2= 0;
          // line column --> x,y (CAL INTERCANVIAR)
          if ( !screen_set_cursor ( intp->screen, (int16_t) op2,
                                    (int16_t) op1, err ) )
            return RET_ERROR;
        }
      break;
      
    case 0xf1: // set_text_style
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      screen_set_style ( intp->screen, op1 );
      break;
    case 0xf2: // buffered_mode
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      screen_set_buffered ( intp->screen, op1!=0 );
      break;
    case 0xf3: // output_stream
      if ( intp->version < 3 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1,
                           false, err ) ) return RET_ERROR;
      if ( !output_stream ( intp, ops, nops, err ) ) return RET_ERROR;
      break;
      
    case 0xf6: // read_char
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !read_char ( intp, ops, nops, result_var, err ) ) return RET_ERROR;
      break;
    case 0xf7: // scan_table
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, -1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !scan_table ( intp, ops, nops, result_var, &cond, err ) )
        return RET_ERROR;
      if ( !branch ( intp, cond, err ) ) return RET_ERROR;
      break;
    case 0xf8: // not
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, 1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      res= ~op1;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0xf9: // call_vn
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1, false, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, 0x00, true, err ) )
        return RET_ERROR;
      break;
    case 0xfa: // call_vn2
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1, true, err ) )
        return RET_ERROR;
      if ( !call_routine ( intp, ops, nops, 0x00, true, err ) )
        return RET_ERROR;
      break;
    case 0xfb: // tokenise
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1, false, err ) )
        return RET_ERROR;
      if ( !tokenise ( intp, ops, nops, err ) ) return RET_ERROR;
      break;

    case 0xfd: // copy_table
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 3, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[2]), &op3, err ) ) return RET_ERROR;
      if ( !copy_table ( intp, op1, op2, op3, err ) ) return RET_ERROR;
      break;
    case 0xfe: // print_table
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1, false, err ) )
        return RET_ERROR;
      if ( !print_table ( intp, ops, nops, err ) ) return RET_ERROR;
      break;
    case 0xff: // check_arg_count
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !branch ( intp, (FRAME_ARGS(state)&(1<<(op1-1)))!=0, err ) )
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
 division0:
  msgerror ( err, "Division by 0" );
  return RET_ERROR;
  
} // end exec_next_inst


static bool
load_unicode_translation_table (
                                Interpreter     *intp,
                                const uint32_t   addr,
                                char           **err
                                )
{

  int n;
  
  
  // Inicialitza taula a desconegut.
  intp->echars.enabled= true;
  for ( n= 0; n < 256; ++n )
    intp->echars.v[n]= 0xFFFD;

  // Llig nombre entrades.
  if ( !memory_map_READB ( intp->mem, addr, &intp->echars.N, true, err ) )
    return false;

  // Llig entrades.
  for ( n= 0; n < (int) ((uint16_t) intp->echars.N); ++n )
    {
      if ( !memory_map_READW ( intp->mem, addr + 1 + n*2,
                               &(intp->echars.v[n]), true, err ) )
        return false;
    }

  return true;
  
} // end load_unicode_translation_table


static bool
load_header_extension_table (
                             Interpreter  *intp,
                             char        **err
                             )
{

  uint32_t ext_addr;
  uint16_t N,tmp;
  
  
  // Inicialització de valors depenent.
  intp->echars.enabled= false;

  // Intenta llegir.
  if ( intp->version < 5 ) return true;
  ext_addr=
    (((uint32_t) intp->mem->sf_mem[0x36])<<8) |
    ((uint32_t) intp->mem->sf_mem[0x37])
    ;
  if ( ext_addr == 0 ) return true;

  // Llig taula
  // --> Nombre d'entrades
  if ( !memory_map_READW ( intp->mem, ext_addr, &N, true, err ) )
    return false;
  // --> Unicode translation table address
  if ( N >= 3 )
    {
      if ( !memory_map_READW ( intp->mem, ext_addr+3*2, &tmp, true, err ) )
        return false;
      if ( tmp != 0 )
        {
          if ( !load_unicode_translation_table ( intp, (uint32_t) tmp, err ) )
            return false;
        }
    }
  
  return true;
  
} // end load_header_extension_table


static bool
load_alphabet_table (
                     Interpreter  *intp,
                     uint32_t      addr,
                     char        **err
                     )
{

  int i,j;

  
  assert ( addr != 0 );
  intp->alph_table.enabled= true;
  for ( i= 0; i < 3; ++i )
    for ( j= 0; j < 26; ++j, ++addr )
      if ( !memory_map_READB ( intp->mem, addr,
                               &(intp->alph_table.v[i][j]),
                               true, err ) )
        return false;
  intp->alph_table.v[2][1]= ZSCII_NEWLINE;
  
  return true;
  
} // end load_alphabet_table


static bool
register_extra_chars (
                      Interpreter  *intp,
                      char        **err
                      )
{

  int i;

  
  // Taula
  if ( intp->echars.enabled )
    {
      for ( i= 0; i < intp->echars.N; ++i )
        if ( !screen_ADD_EXTRA_CHAR ( intp->screen, intp->echars.v[i],
                                      (uint8_t) (i+155), err  ) )
          return false;
    }
  // Taula per defecte
  else
    {
      for ( i= 0; i < ZSCII_TO_UNICODE_SIZE; ++i )
        if ( !screen_ADD_EXTRA_CHAR ( intp->screen, ZSCII_TO_UNICODE[i],
                                      (uint8_t) (i+155), err  ) )
          return false;
    }
  
  return true;
  
} // end register_extra_chars




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
interpreter_free (
                  Interpreter *intp
                  )
{
  
  if ( intp->transcript_fd != NULL ) fclose ( intp->transcript_fd );
  if ( intp->saves != NULL ) saves_free ( intp->saves );
  if ( intp->std_dict != NULL ) dictionary_free ( intp->std_dict );
  if ( intp->usr_dict != NULL ) dictionary_free ( intp->usr_dict );
  g_free ( intp->input_text.v );
  g_free ( intp->text.v );
  if ( intp->screen != NULL ) screen_free ( intp->screen );
  if ( intp->ins != NULL ) instruction_free ( intp->ins );
  if ( intp->mem != NULL ) memory_map_free ( intp->mem );
  if ( intp->sf != NULL ) story_file_free ( intp->sf );
  if ( intp->state != NULL ) state_free ( intp->state );
  g_free ( intp );
  
} // end interpreter_free


Interpreter *
interpreter_new_from_file_name (
                                const char      *file_name,
                                Conf            *conf,
                                const char      *transcript_fn,
                                const gboolean   verbose,
                                Tracer          *tracer,
                                char           **err
                                )
{

  Interpreter *ret;
  uint32_t std_dict_addr,alphabet_table_addr;
  uint8_t *icon;
  size_t icon_size;
  
  
  // Prepara.
  icon= NULL;
  ret= g_new ( Interpreter, 1 );
  ret->sf= NULL;
  ret->state= NULL;
  ret->mem= NULL;
  ret->ins= NULL;
  ret->tracer= tracer;
  ret->screen= NULL;
  ret->text.v= NULL;
  ret->input_text.v= NULL;
  ret->std_dict= NULL;
  ret->usr_dict= NULL;
  ret->saves= NULL;
  ret->verbose= verbose;
  ret->alph_table.enabled= false;
  ret->transcript_fd= NULL;
  
  // Obri story file
  ret->sf= story_file_new_from_file_name ( file_name, err );
  if ( ret->sf == NULL ) goto error;

  // Inicialitza pantalla
  if ( !story_file_get_frontispiece ( ret->sf, &icon, &icon_size, err ) )
    goto error;
  if ( ret->sf->data[0] == 6 )
    {
      msgerror ( err, "Screen model V6 not supported" );
      goto error;
    }
  else
    {
      ret->screen= screen_new ( conf, ret->sf->data[0],
                                story_file_get_title ( ret->sf ),
                                icon, icon_size, verbose, err );
      if ( ret->screen == NULL ) goto error;
    }
  g_free ( icon ); icon= NULL;
  
  // Crea estat.
  ret->state= state_new ( ret->sf, ret->screen, tracer, err );
  if ( ret->state == NULL ) goto error;
  
  // Inicialitza mapa de memòria.
  ret->mem= memory_map_new ( ret->sf, ret->state, tracer, err );
  if ( ret->mem == NULL ) goto error;

  // Altres
  random_reset ( ret );
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
  ret->object_table_offset=
    (((uint32_t) ret->mem->sf_mem[0xa])<<8) |
    ((uint32_t) ret->mem->sf_mem[0xb])
    ;
  if ( ret->version >= 5 )
    {
      alphabet_table_addr=
        (((uint32_t) ret->mem->sf_mem[0x34])<<8) |
        ((uint32_t) ret->mem->sf_mem[0x35])
        ;
    }
  else alphabet_table_addr= 0;
  if ( ret->version >= 2 )
    {
      ret->abbr_table_addr=
        (((uint32_t) ret->mem->sf_mem[0x18])<<8) |
        ((uint32_t) ret->mem->sf_mem[0x19])
        ;
    }
  else ret->abbr_table_addr= 0;
  ret->text.size= 8; // Té prou espai per a imprimir un número 16bit
                     // amb signe
  ret->text.v= g_new ( char, ret->text.size );
  ret->input_text.size= 1;
  ret->input_text.v= g_new ( uint8_t, ret->input_text.size );

  // Diccionaris.
  ret->std_dict= dictionary_new ( ret->mem, err );
  if ( ret->std_dict == NULL ) goto error;
  std_dict_addr=
    (((uint32_t) ret->mem->sf_mem[0x8])<<8) |
    ((uint32_t) ret->mem->sf_mem[0x9])
    ;
  if ( !dictionary_load ( ret->std_dict, std_dict_addr, err ) ) goto error;
  ret->usr_dict= dictionary_new ( ret->mem, err );
  if ( ret->usr_dict == NULL ) goto error;

  // Saves
  ret->saves= saves_new ( verbose );
  if ( ret->saves == NULL ) goto error; // ARA NO PASSA MAI.

  // Output streams
  ret->ostreams.active= INTP_OSTREAM_SCREEN;
  ret->ostreams.N3= 0;

  // Header extension table
  if ( !load_header_extension_table ( ret, err ) ) goto error;

  // Alphabet table addr
  if ( alphabet_table_addr != 0 )
    {
      if ( !load_alphabet_table ( ret, alphabet_table_addr, err ) ) goto error;
    }

  // Register extra chars in screen.
  if ( !register_extra_chars ( ret, err ) ) goto error;

  // Fitxer transcript
  if ( transcript_fn != NULL )
    {
      if ( verbose )
        ii ( "Creating transcript file: %s", transcript_fn );
      ret->transcript_fd= fopen ( transcript_fn, "w" );
      if ( ret->transcript_fd == NULL )
        {
          msgerror ( err, "Failed to open transcript file '%s'",
                     transcript_fn );
          return false;
        }
    }
  
  return ret;
  
 error:
  g_free ( icon );
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
