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
 *  state.h - Implementa l'estat del joc.
 *
 */

#ifndef __CORE__STATE_H__
#define __CORE__STATE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "story_file.h"
#include "tracer.h"
#include "frontend/screen.h"

#define STACK_SIZE 0xFFFF

// IMPORTANT!! Aquests macros no fan comprovacions.
#define FRAME_NLOCAL(ST) ((uint8_t) ((ST)->stack[(ST)->frame+2]&0xF))
#define FRAME_DISCARD_RES(ST) (((ST)->stack[(ST)->frame+2]&0x10)!=0)
#define FRAME_NUM_RES(ST) ((ST)->stack[(ST)->frame+3]>>8)
#define FRAME_ARGS(ST) (((ST)->stack[(ST)->frame+3]&0x7f))
#define FRAME_LOCAL(ST,IND) ((ST)->stack[(ST)->frame+4+((uint16_t) (IND))])

/*
 * Cada vegada que es crida a una nova rutina en la pila es crea un
 * frame amb la següent estructura:
 *
 *   NEW_FRAME --> | OLD_FRAME
 *                 | RET_PC_HIGH (2 bytes)
 *                 | RET_PC_LOW (1 byte)  | 000pvvvv (com quetzal)
 *                 | VARIABLE_NUM_RESULT  | 0gfedcba (com quetzal)
 *                 | LOCAL_VAR 0
 *                     ...
 *                 | LOCAL_VAR v-1
 *                 ----------------
 *          SP --> |
 */
typedef struct _State State;

struct _State
{

  // Camps públics!!
  uint8_t  *mem;                // Memòria dinàmica.
  uint32_t  mem_size;           // Grandària de la memòria.
  uint32_t  PC;                 // Comptador de programa. (Com a màxim
                                // pot ser 7fff8)
  uint16_t  stack[STACK_SIZE];  // Pila
  uint16_t  frame;              // Apunta al frame actual en la pila.
  uint16_t  SP;                 // Apunta al següent element lliure.
  uint16_t  frame_ind;          // Seguint les recomanacions de
                                // Quetzal, indica el nombre de frames
                                // actius en la pila. És el que
                                // utilitzem en catch/throw.

  // Camps privats
  const StoryFile *sf;
  Tracer          *tracer; // Pot ser NULL
  const Screen    *screen;
  
  // Callbacks.
  bool (*writevar) (State *,const uint8_t,const uint16_t,char **);
  bool (*readvar) (State *,const uint8_t,uint16_t *,const bool,char **);
  
};

void
state_free (
            State *state
            );

State *
state_new (
           StoryFile     *sf,
           const Screen  *screen,
           Tracer        *tracer, // Pot ser NULL
           char         **err
           );

// Torna cert si tot ha anat bé. El nombre de variables locals no pot
// superar mai 15 i no es comprova.
//
// IMPORTANT!! Les variables locals no s'inicialitzen.
//
// 'result_num_var' s'ignora si 'discard_result' és true.
//
// IMPORTANT!!! El comptador de programa es modifica també.
//
// args_mask: Ha de ser vàlid, no es comprova.
//  Bit 0: 1 -> local_vars[0] s'ha inicialitzat amb un argument.
//  Bit 1: 1 -> local_vars[1] s'ha inicialitzat amb un argument.
//    ...
//  Bit 6: 1 -> local_vars[6] s'ha inicialitzat amb un argument.
bool
state_new_frame (
                 State           *state,
                 const uint32_t   new_PC,
                 const uint8_t    num_local_vars, // [0,15]
                 const bool       discard_result,
                 const uint8_t    result_num_var,
                 const uint8_t    args_mask,
                 char           **err
                 );

// IMPORTANT!!!! Alliberar el frame actualitza el PC
bool
state_free_frame (
                  State  *state,
                  char  **err
                  );

// VAR: 0 pila, resta variables locals.
#define state_writevar(STATE,VAR,VAL,ERR)               \
  ((STATE)->writevar ( (STATE), (VAR), (VAL), (ERR) ))

// VAR: 0 pila, resta variables locals.
#define state_readvar(STATE,VAR,P_DST,POP,ERR)          \
  ((STATE)->readvar ( (STATE), (VAR), (P_DST), (POP), (ERR) ))

void
state_print_stack (
                   State *state,
                   FILE  *f
                   );

// Desa l'estat en format Quetzal.
bool
state_save (
            State       *state,
            const char  *file_name,
            char       **err
            );

// Llegit l'estat en format Quetzal.
bool
state_load (
            State       *state,
            const char  *file_name,
            char       **err
            );

void
state_enable_trace (
                    State      *state,
                    const bool  enable
                    );

#endif // __CORE__STATE_H__
