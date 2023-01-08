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

#include "iff.h"
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
  else ww ( "Flags2.0: Set when transcripting is on reset");
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
  if ( !state_new_frame ( state, main_addr+1, num_local_vars,
                          true, 0, 0x00, err ) )
    return false;
  for ( n= 0; n < num_local_vars; ++n )
    if ( !state_writevar ( state, n+1, local_vars[n], err ) )
      return false;
  
  return true;
  
} // end call_main


static uint32_t
calc_quetzal_cmem_size (
                        const State *state
                        )
{
  
  uint32_t ret,n;
  uint8_t val;
  bool is_zero;
  int zeros;
  

  ret= 0;
  is_zero= false;
  zeros= 0;
  for ( n= 0; n < state->mem_size; ++n )
    {
      val= state->mem[n]^state->sf->data[n];
      if ( val != 0x00 )
        {
          is_zero= false;
          ++ret;
        }
      else if ( !is_zero )
        {
          ret+= 2;
          zeros= 0;
          is_zero= true;
        }
      else if ( ++zeros == 256 )
        {
          ret+= 2;
          zeros= 0;
        }
    }
  
  return ret;
  
} // end calc_quetzal_cmem_size


// Return compressed data
static uint8_t *
get_quetzal_cmem (
                  const State    *state,
                  const uint32_t  cmem_size
                  )
{

  uint32_t n,p;
  uint8_t val;
  bool is_zero;
  int zeros;
  uint8_t *ret;


  ret= g_new ( uint8_t, cmem_size );
  is_zero= false;
  zeros= 0;
  p= 0;
  for ( n= 0; n < state->mem_size; ++n )
    {
      val= state->mem[n]^state->sf->data[n];
      if ( val != 0x00 )
        {
          if ( is_zero ) ret[p++]= (uint8_t) zeros;
          ret[p++]= val;
          is_zero= false;
        }
      else if ( !is_zero )
        {
          ret[p++]= 0x00;
          zeros= 0;
          is_zero= true;
        }
      else if ( ++zeros == 256 )
        {
          ret[p++]= 0xff;
          ret[p++]= 0x00;
          zeros= 0;
        }
    }
  if ( is_zero ) ret[p++]= (uint8_t) zeros;
  
  assert ( p == cmem_size );
  
  return ret;
  
} // end get_quetzal_cmem


static uint8_t *
get_quetzal_stks (
                  const State *state
                  )
{

  uint8_t *ret;
  uint16_t current,frame,val,num_local_vars,i;
  uint32_t pos;
  

  // Com a mínim estarà el dumb frame.
  assert ( state->SP > 0 );
  assert ( state->SP > state->frame );

  // Crea
  ret= g_new ( uint8_t, ((uint32_t) state->SP)*2 );
  current= state->SP-1;
  frame= state->frame;
  while ( current != 0xFFFF )
    {
      
      pos= ((uint32_t) frame)*2;
      
      // return PC High
      val= state->stack[frame+1];
      ret[pos++]= (uint8_t) (val>>8);
      ret[pos++]= (uint8_t) val;
      // return PC low | 000pvvvv flags
      val= state->stack[frame+2];
      ret[pos++]= (uint8_t) (val>>8);
      ret[pos++]= (uint8_t) val;
      // Result | arguments
      val= state->stack[frame+3];
      ret[pos++]= (uint8_t) (val>>8);
      ret[pos++]= (uint8_t) val;
      // Number of words
      num_local_vars= state->stack[frame+2]&0xF;
      val= current-(frame+4+num_local_vars)+1;
      ret[pos++]= (uint8_t) (val>>8);
      ret[pos++]= (uint8_t) val;
      // Local variables and values
      for ( i= frame+4; i <= current; ++i )
        {
          val= state->stack[i];
          ret[pos++]= (uint8_t) (val>>8);
          ret[pos++]= (uint8_t) val;
        }
      
      current= frame-1;
      frame= state->stack[frame];

    }
  
  return ret;
  
} // end get_quetzal_stks


static bool
fwrite_u32_be (
               FILE           *f,
               const uint32_t  val
               )
{

  uint8_t buf[4];


  buf[0]= (uint8_t) (val>>24);
  buf[1]= (uint8_t) (val>>16);
  buf[2]= (uint8_t) (val>>8);
  buf[3]= (uint8_t) val;
  
  if ( fwrite ( buf, sizeof(buf), 1, f ) != 1 )
    return false;

  return true;
  
} // end fwrite_u32_be


static bool
find_quetzal_chunks (
                     const IFF   *iff,
                     IFFChunk    *ifhd,
                     IFFChunk    *cmem,
                     IFFChunk    *stks,
                     const char  *file_name,
                     char       **err
                     )
{

  size_t n;


  // Cerca
  ifhd->type[0]= '\0';
  cmem->type[0]= '\0';
  stks->type[0]= '\0';
  for ( n= 0; n < iff->N; ++n )
    if ( strcmp ( iff->chunks[n].type, "IFhd" ) == 0 )
      *ifhd= iff->chunks[n];
    else if ( strcmp ( iff->chunks[n].type, "CMem" ) == 0 )
      *cmem= iff->chunks[n];
    else if ( strcmp ( iff->chunks[n].type, "Stks" ) == 0 )
      *stks= iff->chunks[n];

  // Comprova
  if ( ifhd->type[0] == '\0' )
    {
      msgerror ( err, "Chunk IFhd not found" );
      return false;
    }
  if ( cmem->type[0] == '\0' )
    {
      msgerror ( err, "Chunk CMem not found" );
      return false;
    }
  if ( stks->type[0] == '\0' )
    {
      msgerror ( err, "Chunk Stks not found" );
      return false;
    }

  return true;
  
} // end find_quetzal_chunks


static bool
load_quetzal_ifhd (
                   FILE            *f,
                   State           *state,
                   const IFFChunk  *chunk,
                   const char      *file_name,
                   char           **err
                   )
{

  uint8_t *data;


  // Obté dades
  if ( fseek ( f, chunk->offset+8, SEEK_SET ) != 0 )
    { error_read_file ( err, file_name ); goto error; }
  data= g_new ( uint8_t, chunk->length );
  if ( fread ( data, chunk->length, 1, f ) != 1 )
    { error_read_file ( err, file_name ); goto error; }
  
  // Comprova camps.
  if ( state->mem[0x2] != data[0] || state->mem[0x3] != data[1] )
    {
      msgerror ( err,
                 "Failed to load state, release number mismatch "
                 "%02X%02X != %02x%02X: %s",
                 state->mem[0x2], state->mem[0x3],
                 data[0], data[1], file_name );
      goto error;
    }
  if ( state->mem[0x12] != data[2] ||
       state->mem[0x13] != data[3] ||
       state->mem[0x14] != data[4] ||
       state->mem[0x15] != data[5] ||
       state->mem[0x16] != data[6] ||
       state->mem[0x17] != data[7] )
    {
      msgerror ( err,
                 "Failed to load state, serial number mismatch "
                 "%02X%02X%02X%02X%02X%02X != %02X%02X%02X%02X%02X%02X: %s",
                 state->mem[0x12], state->mem[0x13], state->mem[0x14],
                 state->mem[0x15], state->mem[0x16], state->mem[0x17],
                 data[2], data[3], data[4], data[5], data[6], data[7],
                 file_name );
      goto error;
    }
  if ( state->mem[0] >= 4 ) // Sols comprove checksum en versions superiors.
    {
      if ( state->mem[0x1c] != data[8] || state->mem[0x1d] != data[9] )
        {
          msgerror ( err,
                     "Failed to load state, checksum mismatch "
                     "%02X%02X != %02x%02X: %s",
                     state->mem[0x1c], state->mem[0x1d],
                     data[8], data[9], file_name );
          goto error;
        }
    }

  // Carrega PC.
  state->PC=
    (((uint32_t) data[10])<<16) |
    (((uint32_t) data[11])<<8) |
    ((uint32_t) data[12])
    ;
  
  // Allibera
  g_free ( data );
  
  return true;
  
 error:
  g_free ( data );
  return false;
  
} // end load_quetzal_ifhd


static bool
load_quetzal_cmem (
                   FILE            *f,
                   State           *state,
                   const IFFChunk  *chunk,
                   const char      *file_name,
                   char           **err
                   )
{

  uint8_t *data;
  uint8_t flags2_10,flags2_11,val;
  uint32_t i,pos,num_zeros,j;
  
  
  // Obté dades
  if ( fseek ( f, chunk->offset+8, SEEK_SET ) != 0 )
    { error_read_file ( err, file_name ); goto error; }
  data= g_new ( uint8_t, chunk->length );
  if ( fread ( data, chunk->length, 1, f ) != 1 )
    { error_read_file ( err, file_name ); goto error; }
  
  // Carrega (S'ha de descomprimir)
  flags2_10= state->mem[0x10];
  flags2_11= state->mem[0x11];
  pos= 0;
  for ( i= 0; i < chunk->length && pos < state->mem_size; ++i )
    {
      val= data[i];
      if ( val != 0x00 )
        {
          state->mem[pos]= state->sf->data[pos]^val;
          ++pos;
        }
      else
        {
          state->mem[pos]= state->sf->data[pos];
          ++pos; ++i;
          if ( i >= chunk->length ) goto error_invalid_cmem;
          num_zeros= (uint32_t) data[i];
          for ( j= 0; j < num_zeros && pos < state->mem_size; ++j )
            {
              state->mem[pos]= state->sf->data[pos];
              ++pos;
            }
          if ( j != num_zeros ) goto error_invalid_cmem;
        }
    }
  if ( pos > state->mem_size ) goto error_invalid_cmem;
  for ( ; pos < state->mem_size; ++pos )
    state->mem[pos]= state->sf->data[pos];
  reset_header_values ( state, false );
  state->mem[0x10]= flags2_10;
  state->mem[0x11]= flags2_11;
  
  // Allibera
  g_free ( data );
  
  return true;
  
 error_invalid_cmem:
  msgerror ( err,
             "Failed to load state, invalid CMem"
             " compressed data: %s",
             file_name );
 error:
  g_free ( data );
  return false;
  
} // end load_quetzal_cmem


static bool
load_quetzal_stks (
                   FILE            *f,
                   State           *state,
                   const IFFChunk  *chunk,
                   const char      *file_name,
                   char           **err
                   )
{

  uint8_t *data;
  uint8_t tmp;
  uint32_t i,num_local_vars,num_words_eval,total_extra,j;
  uint16_t pos;
  
  
  // Obté dades
  if ( fseek ( f, chunk->offset+8, SEEK_SET ) != 0 )
    { error_read_file ( err, file_name ); goto error; }
  data= g_new ( uint8_t, chunk->length );
  if ( fread ( data, chunk->length, 1, f ) != 1 )
    { error_read_file ( err, file_name ); goto error; }

  // Processa
  pos= state->SP= state->frame= 0x0000;
  i= 0;
  while ( (pos+3) < 0xFFFF && (i+7) < chunk->length )
    {
      // Old frame
      state->stack[pos++]= state->frame;
      state->frame= state->SP;
      // PC_HIGH
      state->stack[pos++]= (((uint16_t) data[i])<<8) | ((uint16_t) data[i+1]);
      i+= 2;
      // PC_LOW | 000pvvvv flags
      tmp= data[i+1];
      if ( (tmp&0x1F) != tmp ) goto error_invalid_stks;
      num_local_vars= (uint32_t) (tmp&0xF);
      state->stack[pos++]= (((uint16_t) data[i])<<8) | ((uint16_t) tmp);
      i+= 2;
      // Variable result | 0gfedcba
      state->stack[pos++]= (((uint16_t) data[i])<<8) | ((uint16_t) data[i+1]);
      i+= 2;
      //  number of words of evaluation
      num_words_eval= (((uint16_t) data[i])<<8) | ((uint16_t) data[i+1]);
      i+= 2;
      total_extra= num_local_vars + num_words_eval;
      if ( (((uint32_t) pos)+total_extra > 0xFFFF) ||
           i+total_extra > chunk->length )
        goto error_invalid_stks;
      // Local variables i Other
      for ( j= 0; j < total_extra; ++j )
        {
          state->stack[pos++]=
            (((uint16_t) data[i])<<8) | ((uint16_t) data[i+1]);
          i+= 2;
        }
      // Nou SP
      state->SP= pos;
    }
  if ( i != chunk->length ) goto error_invalid_stks;
  
  // Allibera
  g_free ( data );
  
  return true;
  
 error_invalid_stks:
  msgerror ( err,
             "Failed to load state, invalid Stks data or stack too large: %s",
             file_name );
 error:
  g_free ( data );
  return false;
  
} // end load_quetzal_stks


// VAR: 0 pila, resta variables locals.
static bool
writevar (
          State           *state,
          const uint8_t    var,
          const uint16_t   val,
          char           **err
          )
{

  uint8_t ind;

  
  // Pila
  if ( var == 0x00 )
    {
      if ( state->SP == 0xFFFF )
        {
          msgerror ( err, "Stack overflow" );
          return false;
        }
      state->stack[state->SP++]= val;
    }

  // Variable local
  else
    {
      ind= var-1;
      if ( ind >= FRAME_NLOCAL(state) )
        {
          msgerror ( err, "Failed to write local variable: index %u"
                     " is out of bounds [0,%u[", ind, FRAME_NLOCAL(state) );
          return false;
        }
      FRAME_LOCAL(state,ind)= val;
    }
  
  return true;
  
} // end writevar


static bool
writevar_trace (
                State           *state,
                const uint8_t    var,
                const uint16_t   val,
                char           **err
                )
{

  if ( !writevar ( state, var, val, err ) )
    return false;
  if ( state->tracer != NULL && state->tracer->stack_access != NULL )
    state->tracer->stack_access ( state->tracer, var, val, STACK_ACCESS_WRITE );
  
  return true;
  
} // end writevar_trace


// VAR: 0 pila, resta variables locals.
static bool
readvar (
         State          *state,
         const uint8_t   var,
         uint16_t       *val,
         char          **err
         )
{
  
  uint8_t ind;
  
  
  // Pila
  if ( var == 0x00 )
    {
      if ( state->SP == state->frame+4 )
        {
          msgerror ( err, "Stack underflow" );
          return false;
        }
      *val= state->stack[--state->SP];
    }

  // Variable local
  else
    {
      ind= var-1;
      if ( ind >= FRAME_NLOCAL(state) )
        {
          msgerror ( err, "Failed to read local variable: index %u"
                     " is out of bounds [0,%u[", ind, FRAME_NLOCAL(state) );
          return false;
        }
      *val= FRAME_LOCAL(state,ind);
    }
  
  return true;
  
} // end readvar


static bool
readvar_trace (
               State          *state,
               const uint8_t   var,
               uint16_t       *val,
               char          **err
               )
{

  if ( !readvar ( state, var, val, err ) )
    return false;
  if ( state->tracer != NULL && state->tracer->stack_access != NULL )
    state->tracer->stack_access ( state->tracer, var, *val, STACK_ACCESS_READ );
  
  return true;
  
} // end readvar_trace




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
           Tracer     *tracer,
           char      **err
           )
{

  State *ret;
  uint8_t version;
  
  
  // Prepara.
  ret= g_new ( State, 1 );
  ret->mem= NULL;
  ret->sf= sf;
  ret->tracer= tracer;

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

  // Callbacks.
  ret->writevar= writevar;
  ret->readvar= readvar;
  
  return ret;

 error:
  state_free ( ret );
  return NULL;
  
} // end state_new


bool
state_new_frame (
                 State           *state,
                 const uint32_t   new_PC,
                 const uint8_t    num_local_vars, // [0,15]
                 const bool       discard_result,
                 const uint8_t    result_num_var,
                 const uint8_t    args_mask,
                 char           **err
                 )
{

  uint16_t size,new_SP;
  

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
  state->SP+= (uint16_t) num_local_vars;
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


bool
state_save (
            State       *state,
            const char  *file_name,
            char       **err
            )
{

  const uint8_t ZERO_VAL= 0x00;
  
  FILE *f;
  uint8_t buf_PC[3];
  uint8_t *cmem,*stks;
  uint32_t cmem_size,ifhd_size,stks_size,total_size;
  size_t welems;
  
  
  // Obri fitxer.
  f= fopen ( file_name, "wb" );
  if ( f == NULL )
    {
      error_create_file ( err, file_name );
      goto error;
    }

  // Calcula grandàries seccions.
  ifhd_size= 13;
  cmem_size= calc_quetzal_cmem_size ( state );
  stks_size= ((uint32_t) (state->SP))*2;
  total_size=
    4 + // FORM TYPE
    8 + ifhd_size + 1 +
    8 + cmem_size + ((cmem_size&0x1) ? 1 : 0) +
    8 + stks_size + ((stks_size&0x1) ? 1 : 0)
    ;

  // Escriu capçalera
  if ( fprintf ( f, "FORM" ) != 4 ) goto error_write;
  if ( !fwrite_u32_be ( f, total_size ) ) goto error_write;
  if ( fprintf ( f, "IFZS" ) != 4 ) goto error_write;

  // IFhd (Associated story file recognition)
  if ( fprintf ( f, "IFhd" ) != 4 ) goto error_write;
  if ( !fwrite_u32_be ( f, 13 ) ) goto error_write;
  // --> release number 
  if ( fwrite ( &(state->sf->data[0x2]), 2, 1, f ) != 1 ) goto error_write;
  // --> serial number
  if ( fwrite ( &(state->sf->data[0x12]), 6, 1, f ) != 1 ) goto error_write;
  // --> checksum
  if ( fwrite ( &(state->sf->data[0x1c]), 2, 1, f ) != 1 ) goto error_write;
  // --> PC
  buf_PC[0]= (uint8_t) (state->PC>>16);
  buf_PC[1]= (uint8_t) (state->PC>>8);
  buf_PC[2]= (uint8_t) (state->PC);
  if ( fwrite ( buf_PC, sizeof(buf_PC), 1, f ) != 1 ) goto error_write;
  if ( fwrite ( &ZERO_VAL, 1, 1, f ) != 1 ) goto error_write;

  // Content of dynamic memory
  if ( fprintf ( f, "CMem" ) != 4 ) goto error_write;
  if ( !fwrite_u32_be ( f, cmem_size ) ) goto error_write;
  cmem= get_quetzal_cmem ( state, cmem_size );
  welems= fwrite ( cmem, cmem_size, 1, f );
  g_free ( cmem );
  if ( welems != 1 ) goto error_write;
  if ( cmem_size&0x1 )
    { if ( fwrite ( &ZERO_VAL, 1, 1, f ) != 1 ) goto error_write; }
  
  // Content of stacks
  if ( fprintf ( f, "Stks" ) != 4 ) goto error_write;
  if ( !fwrite_u32_be ( f, stks_size ) ) goto error_write;
  stks= get_quetzal_stks ( state );
  welems= fwrite ( stks, stks_size, 1, f );
  g_free ( stks );
  if ( welems != 1 ) goto error_write;
  if ( stks_size&0x1 )
    { if ( fwrite ( &ZERO_VAL, 1, 1, f ) != 1 ) goto error_write; }
  
  // Tanca
  fclose ( f );

  return true;

 error_write:
  error_write_file ( err, file_name );
 error:
  if ( f != NULL ) fclose ( f );
  return false;
  
} // end state_save


bool
state_load (
            State       *state,
            const char  *file_name,
            char       **err
            )
{

  FILE *f;
  IFF *iff;
  IFFChunk ifhd,cmem,stks;
  

  // Prepara.
  f= NULL;
  iff= NULL;

  // Obté el IFF
  iff= iff_new_from_file_name ( file_name, err );
  if ( iff == NULL ) goto error;
  if ( strcmp ( iff->type, "IFZS" ) != 0 )
    {
      msgerror ( err, "Unknown FORM type '%s': %s",
                 iff->type, file_name );
      goto error;
    }

  // Localitza chunks rellevants.
  if ( !find_quetzal_chunks ( iff, &ifhd, &cmem, &stks, file_name, err ) )
    goto error;

  // Obri fitxer
  f= fopen ( file_name, "rb" );
  if ( f == NULL ) { error_open_file ( err, file_name ); goto error; }

  // Informació IFhd
  if ( !load_quetzal_ifhd ( f, state, &ifhd, file_name, err ) )
    goto error;
  
  // Informació CMem
  if ( !load_quetzal_cmem ( f, state, &cmem, file_name, err ) )
    goto error;

  // Informació Stks
  if ( !load_quetzal_stks ( f, state, &stks, file_name, err ) )
    goto error;
  
  // Allibera.
  iff_free ( iff );
  fclose ( f );
  
  return true;

 error:
  if ( iff != NULL ) iff_free ( iff );
  if ( f != NULL ) fclose ( f );
  return false;
  
} // end state_load


void
state_enable_trace (
                    State      *state,
                    const bool  enable
                    )
{

  if ( enable )
    {
      state->writevar= writevar_trace;
      state->readvar= readvar_trace;
    }
  else
    {
      state->writevar= writevar;
      state->readvar= readvar;
    }
  
} // end state_enable_trace
