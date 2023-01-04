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
 *  state.c - Implementa 'state.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "state.h"
#include "utils/error.h"
#include "utils/log.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
reset_header_values (
                     State      *state,
                     const bool  init
                     )
{

  uint8_t version;


  version= state->sf->data[0];

  // 0x01: Flags 1
  if ( version <= 3 )
    {
      // 4: Status line not available?
      state->mem[0x01]&= ~0x10;
      // 5: Screen-splitting available?
      state->mem[0x01]|= 0x20;
      // 6: Is a variable-pitch font the default?
      ww ( "[version 3] Flags1.6: Is a variable-pitch font the default?"
           " - Not implemented" );
    }
  else
    {
      // 0: Colours available?
      if ( version >= 5 )
        state->mem[0x01]|= 0x01;
      // 1: Picture displaying available?
      if ( version == 6 )
        state->mem[0x01]|= 0x02;
      // 2: Boldface available?
      state->mem[0x01]|= 0x04;
      // 3: Italic available?
      state->mem[0x01]|= 0x08;
      // 4: Fixed-space style available?
      state->mem[0x01]|= 0x10;
      // 5: Sound effects available?
      if ( version == 6 )
        state->mem[0x01]|= 0x20;
      // 7: Timed keyboard input available?
      state->mem[0x01]|= 0x80;
    }

  // 0x10-0x11: Flags 2 (Dubtes sobre si el bit 8 està en el 0x11 o en
  // el 0x10)
  // 0: Set when transcripting is on
  if ( init ) state->mem[0x10]&= ~0x01;
  else ww ( "Flags2.0: Set when transcripting is on on reset");
  // En la resta de flags que es poden canviar en Int o Reset si es
  // permet no cal fer res.

  // 0x1E: Interpreter number
  if ( version >= 4)
    {
      state->mem[0x1e]= 4; // Aposte pel model Amiga
      if ( version == 6 )
        ww ( "Interpreter Number set to Amiga" );
    }

  // 0x1F: Interpreter version
  if ( version >= 4 )
    {
      // NOTA:: ¿¿?? No tinc clar què ficar, fique 1
      state->mem[0x1f]= version==6 ? 1 : '1';
    }

  // 0x20: Screen height (lines): 255 means "infinite"
  // 0x21: Screen width (characters)
  if ( version >= 4 )
    {
      ww ( "[Header] Screen height - Not implemented" );
      ww ( "[Header] Screen width - Not implemented" );
    }

  // 0x22: Screen width in units
  // 0x24: Screen height in units
  // 0x26: Font width / height in units (defined as width of a '0')
  // 0x27: Font height / width in units
  if ( version >= 5 )
    {
      ww ( "[Header] Screen width in units - Not implemented" );
      ww ( "[Header] Screen height in units - Not implemented" );
      if ( version == 6 )
        {
          ww ( "[Header] Font height in units - Not implemented" );
          ww ( "[Header] Font width in units (defined as width of a '0')"
               " - Not implemented" );
        }
      else
        {
          ww ( "[Header] Font width in units (defined as width of a '0')"
               " - Not implemented" );
          ww ( "[Header] Font height in units - Not implemented" );
        }
    }

  // 0x2C: Default background colour
  // 0x2D: Default foreground colour
  if ( version >= 5 )
    {
      ww ( "[Header] Default background colour - Not implemented" );
      ww ( "[Header] Default foreground colour - Not implemented" );
    }
  
} // end reset_header_values


static void
create_dummy_frame (
                    State *state
                    )
{

  state->frame= 0x0000;
  state->stack[0]= 0x0000;
  state->stack[1]= 0x0000;
  state->stack[2]= 0x0000;
  state->stack[3]= 0x0000;
  state->SP= 0x0004;
  
} // end create_dummy_frame


// S'assumeix que PC és el valor de la capçalera.
static bool
call_main (
           State  *state,
           char  **err
           )
{

  uint32_t roffset,main_addr;
  uint8_t num_local_vars,n;
  uint16_t local_vars[16];
  

  // Calcula adreça rutina
  roffset= (((uint32_t) state->mem[0x28])<<8) | ((uint32_t) state->mem[0x29]);
  roffset<<= 3;
  main_addr= state->PC + roffset;
  if ( ((size_t) (main_addr+1)) >= state->sf->size )
    {
      msgerror ( err, "Invalid 'main' routine address %X", main_addr );
      return false;
    }
  
  // Llig nombre variables locals.
  num_local_vars= state->sf->data[main_addr];
  if ( num_local_vars > 0x0f )
    {
      msgerror ( err, "Wrong number of arguments in 'main' routine: %X",
                 num_local_vars );
      return false;
    }
  // NOTA!! Aquesta funció sols es crida quan la versió és 6. Per tant
  // tots els arguments s'inicialitzen a 0.
  for ( n= 0; n < num_local_vars; ++n )
    local_vars[n]= 0x0000;

  // Prepara entorn i crea un nou frame.
  state->frame= 0x0000;
  state->SP= 0x0000;
  if ( !state_new_frame ( state, main_addr+1, local_vars, num_local_vars,
                          true, 0, 0x00, err ) )
    return false;
  
  return true;
  
} // end call_main




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
state_free (
            State *state
            )
{

  g_free ( state->mem );
  g_free ( state );
  
} // end state_free


State *
state_new (
           StoryFile  *sf,
           char      **err
           )
{

  State *ret;
  uint8_t version;
  
  
  // Prepara.
  ret= g_new ( State, 1 );
  ret->mem= NULL;
  ret->sf= sf;

  // Crea memòria dinàmica.
  version= sf->data[0];
  ret->mem_size= (((uint32_t) sf->data[0xe])<<8) | ((uint32_t) sf->data[0xf]);
  if ( ret->mem_size < 64 )
    {
      msgerror ( err, "Dynamic memory size is too small: %u", ret->mem_size );
      goto error;
    }
  if ( ((size_t) ret->mem_size) > sf->size )
    ret->mem_size= (uint16_t) sf->size;
  ret->mem= g_new ( uint8_t, ret->mem_size );
  memcpy ( ret->mem, sf->data, ret->mem_size );
  reset_header_values ( ret, true );

  // PC
  ret->PC= (((uint32_t) sf->data[0x6])<<8) | ((uint32_t) sf->data[0x7]);
  if ( version == 6 )
    ret->PC<<= 2;

  // Stack.
  if ( version == 6 )
    {
      if ( !call_main ( ret, err ))
        goto error;
    }
  else create_dummy_frame ( ret );
  
  return ret;

 error:
  state_free ( ret );
  return NULL;
  
} // end state_new


bool
state_new_frame (
                 State           *state,
                 const uint32_t   new_PC,
                 const uint16_t  *local_vars,
                 const uint8_t    num_local_vars, // [0,15]
                 const bool       discard_result,
                 const uint8_t    result_num_var,
                 const uint8_t    args_mask,
                 char           **err
                 )
{

  uint16_t size,new_SP;
  uint8_t n;
  

  assert ( num_local_vars <= 15 );
  
  // Calcula espai necessari.
  size= 4 + (uint16_t)num_local_vars;
  new_SP= state->SP + size;
  if ( new_SP < state->SP )
    {
      msgerror ( err, "Stack overflow" );
      return false;
    }

  // Desa valors
  // --> OLD_FRAME i nou frame
  state->stack[state->SP]= state->frame;
  state->frame= state->SP++;
  // --> OLD_PC i pvvvv
  // NOTA!! La idea quan vaja a escriure en quetzal és que escriure de
  // cada word primer el byte high i després el low fent ús
  // d'operadors. Per tant sense entrar en endianismes dese en la pila
  // els valors de tal manera que al guardar en format quetzal
  // encaixe.
  state->stack[state->SP++]= (uint16_t) (state->PC>>8);
  state->stack[state->SP++]=
    (((uint16_t) (state->PC&0xFF))<<8) |
    (discard_result ? 0x0010 : 0x0000) |
    ((uint16_t) num_local_vars)
    ;
  // --> VARIABLE_NUM_RESULT i 0gfedcba (com quetzal)
  state->stack[state->SP++]=
    (discard_result ? 0x0000 : (((uint16_t) result_num_var)<<8)) |
    ((uint16_t) args_mask)
    ;
  // --> LOCAL_VARS
  for ( n= 0; n < num_local_vars; ++n )
    state->stack[state->SP++]= local_vars[n];
  assert ( new_SP == state->SP );

  // Nou PC
  state->PC= new_PC;
  
  return true;
  
} // end state_new_frame


bool
state_free_frame (
                  State  *state,
                  char  **err
                  )
{

  if ( state->frame == 0x0000 )
    {
      msgerror ( err, "Stack underflow" );
      return false;
    }
  state->PC=
    (((uint32_t) state->stack[state->frame+1])<<8) |
    (((uint32_t) state->stack[state->frame+2])>>8)
    ;
  state->SP= state->frame;
  state->frame= state->stack[state->frame];
  
  return true;
  
} // end state_free_frame


bool
state_stack_push (
                  State           *state,
                  const uint16_t   val,
                  char           **err
                  )
{

  if ( state->SP == 0xFFFF )
    {
      msgerror ( err, "Stack overflow" );
      return false;
    }
  state->stack[state->SP++]= val;

  return true;
  
} // end state_stack_push


bool
state_stack_pop (
                  State     *state,
                  uint16_t  *val,
                  char     **err
                  )
{

  if ( state->SP == 0x0000 )
    {
      msgerror ( err, "Stack underflow" );
      return false;
    }
  *val= state->stack[--state->SP];
  
  return true;
  
} // end state_stack_pop


void
state_print_stack (
                   State *state,
                   FILE  *f
                   )
{

  uint16_t current,frame;


  // Prepara
  fprintf ( f, "%04X  |      | <- SP\n", state->SP );
  fprintf ( f, "      --------\n");
  current= state->SP-1;
  frame= state->frame;

  // Imprimeix
  while ( current != 0xFFFF )
    {
      for ( ; current > frame; --current )
        fprintf ( f, "%04X  | %04X |\n",
                  current, state->stack[current] );
      fprintf ( f, "%04X  | %04X |\n",
                current, state->stack[current] );
      --current;
      fprintf ( f, "      --------\n");
      frame= state->stack[frame];
    }
  
} // end state_print_stack
