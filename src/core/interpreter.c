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
#include <string.h>

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
  }

#define U16_S32(U16) ((int32_t) ((int16_t) (U16)))
#define S32_U16(S32) ((uint16_t) ((uint32_t) (S32)))

// ESPECIAL ZSCII CHARS
#define ZSCII_TAB     11
#define ZSCII_NEWLINE 13
#define ZSCII_DELETE  8

// 10 milisegons, de moment
#define TIME_SLEEP 10000




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
    0x00bf  // ¿
  };




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
        offset= ((uint32_t) -((int32_t) offset));
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
                   uint32_t        *offset,
                   char           **err
                   )
{

  // Comprovacions i obté property pointer
  if ( intp->version <= 3 )
    {
      if ( object < 1 || object > 255 )
        {
          msgerror ( err, "Failed to get property address: invalid"
                     " object index %u", object );
          return false;
        }
      *offset=
        intp->object_table_offset +
        31*2 + // Default properties
        (object-1)*9
        ;
    }
  else
    {
      if ( object < 1 )
        {
          msgerror ( err, "Failed to get property address: invalid"
                     " object index %u", object );
          return false;
        }
      *offset=
        intp->object_table_offset +
        63*2 + // Default properties
        (object-1)*14
        ;
    }

  return true;
  
} // end get_object_offset


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
  if ( !get_object_offset ( intp, object, &property_pointer_offset, err ) )
    return false;
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
get_prop_addr (
               Interpreter     *intp,
               const uint16_t   object,
               const uint16_t   property,
               uint16_t        *addr,
               char           **err
               )
{
  
  uint8_t tmp;
  
  
  return get_prop_addr_len ( intp, object, property, addr, &tmp, err );
  
} // end get_prop_addr


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
  if ( len == 1 )
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
  if ( !get_object_offset ( intp, object, &object_offset, err ) )
    return false;

  // Comprova atribut.
  offset= object_offset + attr/8;
  mask= 1<<(7-(attr%8));
  if ( !memory_map_READB ( intp->mem, offset, &val, false, err ) )
    return false;
  *ret= (val&mask)!=0;
  
  return true;
  
} // end test_attr


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
  
  
  // Obté property pointer
  if ( !get_object_offset ( intp, a, &parent_offset, err ) )
    return false;

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

  // Bota.
  if ( !branch ( intp, is_parent, err ) ) return false;
  
  return true;
  
} // end jin


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


static bool
zscii2utf8 (
            Interpreter     *intp,
            const uint32_t   addr,
            uint32_t        *ret_addr, // Pot ser NULL
            const bool       hmem_allowed,
            const bool       is_abbr,
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
                               false, true, err ) )
              return false;
            abbr_ind= 0;
          }
        
        // Valors
        else
          {
            if ( intp->alphabet_table_addr != 0x00 )
              ee ( "CAL IMPLEMENTAR ALPHABET_TABLE " );
            else
              {
                if ( zc == 6 && alph == 2 ) mode= WAIT_ZSCII_TOP;
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
              }
            alph= prev_alph;
          }
        
        word<<= 5;
      }
    
  } while ( !end );
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
      ee ( "print_output - CAL IMPLEMENTAR output stream 2 (transcript)" );
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

  if ( !zscii2utf8 ( intp, addr, ret_addr, hmem_allowed, false, err ) )
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
print_input_text (
                  Interpreter  *intp,
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
        { ee ("print_input_text - CAL IMPLEMENTAR EXTRA CHARS"); }
    }
  if ( !text_add ( intp, '\0', err ) ) return false;
  
  // Imprimeix
  if ( !print_output ( intp, intp->text.v, true, err ) )
    return false;
  
  return true;
  
} // end print_input_text


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

  uint16_t text_buf,parse_buf,result;
  uint8_t max_letters,current_letters;
  int n,nread,real_max;
  uint8_t buf[SCREEN_INPUT_TEXT_BUF],zc;
  bool stop,changed;
  
  
  // Parseja opcions.
  result= 13; // Newline
  if ( nops < 2 || nops > 4 )
    {
      msgerror ( err, "(sread) Expected between 2 and 4 operands but %d found",
                 nops );
      return false;
    }
  if ( !op_to_u16 ( intp, &(ops[0]), &text_buf, err ) ) return false;
  if ( !op_to_u16 ( intp, &(ops[1]), &parse_buf, err ) ) return false;
  if ( nops > 2 )
    {
      ee ( "CAL IMPLEMENTAR sread time routine" );
    }
  
  // Obté capacitat màxima caràcters.
  if ( !memory_map_READB ( intp->mem, text_buf, &max_letters, true, err ) )
    return false;
  if ( max_letters < 3 )
    {
      msgerror ( err, "(sread) Text buffer length (%d) less than 3",
                 max_letters);
      return false;
    }
  
  // Obté caràcters que hi han actualment en el buffer.
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

    // Repinta
    if ( changed )
      {
        screen_undo ( intp->screen );
        if ( !print_input_text ( intp, err ) ) return false;
      }
    
    // Espera
    g_usleep ( TIME_SLEEP );
    
  } while ( !stop );
  // --> Pinta retorn carro.
  if ( !print_output ( intp, "\n", true, err ) )
    return false;
  
  // Escriu en el text buffer
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
  /* DEBUG!!!
  for ( int i= 0; i < intp->input_text.N; ++i )
    printf("%d ",intp->input_text.v[i]);
  printf("\n");
  */
  
  // Parseja.
  if ( !dictionary_parse ( intp->std_dict, text_buf, parse_buf, err ) )
    return false;

  // Desa valor retorn
  if ( !write_var ( intp, result_var, result, err ) )
    return false;
  
  return true;
  
} // end sread


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

  
  state= intp->state;
  if ( !memory_map_READB ( intp->mem, state->PC++, &opcode, true, err ) )
    return false;
  switch ( opcode )
    {

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
      
    case 0x0d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, op1_u8, op2, err ) ) return RET_ERROR;
      break;
      
    case 0x0f: // loadw
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x10: // loadb
      if ( !read_small_small_store ( intp, &op1_u8, &op2_u8, &result_var, err ))
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      SET_U8TOU16(op2_u8,op2);
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
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
      
    case 0x2d: // store
      if ( !read_small_var ( intp, &op1_u8, &op2, err ) ) return RET_ERROR;
      if ( op1_u8 == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, op1_u8, op2, err ) ) return RET_ERROR;
      break;

    case 0x2f: // loadw
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x30: // loadb
      if ( !read_small_var_store ( intp, &op1_u8, &op2, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op1_u8,op1);
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
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
      
    case 0x4d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( ref == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, ref, op2, err ) ) return RET_ERROR;
      break;

    case 0x4f: // loadw
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x50: // loadb
      if ( !read_var_small_store ( intp, &op1, &op2_u8, &result_var, err ) )
        return RET_ERROR;
      SET_U8TOU16(op2_u8,op2);
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
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
      
    case 0x6d: // store
      if ( !read_small_small ( intp, &op1_u8, &op2_u8, err ) ) return RET_ERROR;
      if ( !read_ind_var_ref ( intp, op1_u8, &ref, err ) ) return RET_ERROR;
      if ( !read_var ( intp, op2_u8, &op2, err ) ) return RET_ERROR;
      if ( ref == 0x00 ) // Si es desa en la pila descarte anterior
        { if ( !read_var ( intp, 0, &tmp16, err ) ) return RET_ERROR; }
      if ( !write_var ( intp, ref, op2, err ) ) return RET_ERROR;
      break;

    case 0x6f: // loadw
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
      if ( !memory_map_READW ( intp->mem, addr, &res, false, err ) )
        return RET_ERROR;
      if ( !write_var ( intp, result_var, res, err ) ) return RET_ERROR;
      break;
    case 0x70: // loadb
      if ( !read_var_var_store ( intp, &op1, &op2, &result_var, err ) )
        return RET_ERROR;
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
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
      return RET_STOP;
      break;
    case 0xbb: // new_line
      if ( !print_output ( intp, "\n", false, err ) ) return RET_ERROR;
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

    case 0xcf: // loadw
      if ( !read_var_ops_store ( intp, ops, &nops, 2,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
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
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
      if ( !memory_map_READB ( intp->mem, addr, &res_u8, false, err ) )
        return RET_ERROR;
      SET_U8TOU16(res_u8,res);
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
      addr= ((uint32_t) op1) + (uint32_t) (2*(int32_t) ((int16_t) op2));
      if ( !memory_map_WRITEW ( intp->mem, addr, op3, false, err ) )
        return RET_ERROR;
      break;
    case 0xe2: // storeb
      if ( !read_var_ops ( intp, ops, &nops, 3, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[1]), &op2, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[2]), &op3, err ) ) return RET_ERROR;
      addr= ((uint32_t) op1) + (uint32_t) ((int32_t) ((int16_t) op2));
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
          msgerror ( err, "read not implemented in version %d", intp->version );
          return RET_ERROR;
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

    case 0xf1: // set_text_style
      if ( intp->version < 4 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, 1, false, err ) ) return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return RET_ERROR;
      screen_set_style ( intp->screen, op1 );
      break;

    case 0xf3: // output_stream
      if ( intp->version < 3 ) goto wrong_version;
      if ( !read_var_ops ( intp, ops, &nops, -1,
                           false, err ) ) return RET_ERROR;
      if ( !output_stream ( intp, ops, nops, err ) ) return RET_ERROR;
      break;
      
    case 0xf8: // not
      if ( intp->version < 5 ) goto wrong_version;
      if ( !read_var_ops_store ( intp, ops, &nops, 1,
                                 false, &result_var, err ) )
        return RET_ERROR;
      if ( !op_to_u16 ( intp, &(ops[0]), &op1, err ) ) return false;
      res= ~op1;
      if ( !write_var ( intp, result_var, res, err ) ) return false;
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




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
interpreter_free (
                  Interpreter *intp
                  )
{

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
                                const gboolean   verbose,
                                Tracer          *tracer,
                                char           **err
                                )
{

  Interpreter *ret;
  uint32_t std_dict_addr;
  
  
  // Prepara.
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
  
  // Obri story file
  ret->sf= story_file_new_from_file_name ( file_name, err );
  if ( ret->sf == NULL ) goto error;

  // Inicialitza pantalla
  if ( ret->sf->data[0] == 6 )
    {
      msgerror ( err, "Screen model V6 not supported" );
      goto error;
    }
  else
    {
      ret->screen= screen_new ( conf, ret->sf->data[0], "Prova",
                                verbose, err );
      if ( ret->screen == NULL ) goto error;
    }
  
  // Crea estat.
  ret->state= state_new ( ret->sf, ret->screen, tracer, err );
  if ( ret->state == NULL ) goto error;
  
  // Inicialitza mapa de memòria.
  ret->mem= memory_map_new ( ret->sf, ret->state, tracer, err );
  if ( ret->mem == NULL ) goto error;

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
  ret->object_table_offset=
    (((uint32_t) ret->mem->sf_mem[0xa])<<8) |
    ((uint32_t) ret->mem->sf_mem[0xb])
    ;
  if ( ret->version >= 5 )
    {
      ret->alphabet_table_addr=
        (((uint32_t) ret->mem->sf_mem[0x34])<<8) |
        ((uint32_t) ret->mem->sf_mem[0x35])
        ;
    }
  else ret->alphabet_table_addr= 0;
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
