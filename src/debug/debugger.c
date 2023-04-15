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
 *  debugger.c - Implementació de 'debugger.h'.
 *
 */


#include <errno.h>
#include <glib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/interpreter.h"
#include "debugger.h"
#include "tokenizer.h"
#include "tracer.h"
#include "utils/log.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
help (void)
{
  printf (
          "\n"
          "COMMANDS:\n\n"
          "  * help: \n"
          "      Show this message\n\n"
          "  * enable_trace [CPU|MEM|STACK|PRINT_CC]+:\n"
          "      Enable trace for specified components\n\n"
          "  * disable_trace [CPU|MEM|STACK|PRINT_CC]+:\n"
          "      Disable trace for specified components\n\n"
          "  * trace <num_cc>:\n"
          "      Execute <num_cc> CPU cycles in trace mode\n\n"
          "  * quit:\n"
          "      Stop debugger\n\n"
          "\n"
          );
} // end help


static void
enable_trace (
              DebugTracer  *tracer,
              const char  **tokens
              )
{

  for ( ++tokens; *tokens != NULL; ++tokens )
    {
      if ( !strcmp ( *tokens, "CPU" ) )
        debug_tracer_enable_flags ( tracer, DEBUG_TRACER_FLAGS_CPU );
      else if ( !strcmp ( *tokens, "MEM" ) )
        debug_tracer_enable_flags ( tracer, DEBUG_TRACER_FLAGS_MEM );
      else if ( !strcmp ( *tokens, "STACK" ) )
        debug_tracer_enable_flags ( tracer, DEBUG_TRACER_FLAGS_STACK );
      else if ( !strcmp ( *tokens, "PRINT_CC" ) )
        debug_tracer_enable_flags ( tracer, DEBUG_TRACER_FLAGS_PRINT_CC );
      else
        ww ( "[enable_trace] Unknown flag '%s'", *tokens );
    }
  
} // enable_trace


static void
disable_trace (
               DebugTracer  *tracer,
               const char  **tokens
               )
{

  for ( ++tokens; *tokens != NULL; ++tokens )
    {
      if ( !strcmp ( *tokens, "CPU" ) )
        debug_tracer_disable_flags ( tracer, DEBUG_TRACER_FLAGS_CPU );
      else if ( !strcmp ( *tokens, "MEM" ) )
        debug_tracer_disable_flags ( tracer, DEBUG_TRACER_FLAGS_MEM );
      else if ( !strcmp ( *tokens, "STACK" ) )
        debug_tracer_disable_flags ( tracer, DEBUG_TRACER_FLAGS_STACK );
      else if ( !strcmp ( *tokens, "PRINT_CC" ) )
        debug_tracer_disable_flags ( tracer, DEBUG_TRACER_FLAGS_PRINT_CC );
      else
        ww ( "[disable_trace] Unknown flag '%s'", *tokens );
    }
  
} // disable_trace


static bool
trace (
       Interpreter  *intp,
       const char  **tokens,
       char        **err
       )
{

  unsigned long iters;

  
  // Obté el nombre d'iteracions.
  ++tokens;
  if ( *tokens == NULL )
    {
      ww ( "[trace] number of cycles not specified" );
      return true;
    }
  errno= 0;
  iters= strtoul ( *tokens, NULL, 10 );
  if ( errno != 0 )
    {
      ww ( "[trace] invalid number of cycles: '%s'", *tokens );
      return true;
    }

  // Trace
  if ( !interpreter_trace ( intp, iters, err ) )
    return false;
  
  return true;
  
} // end trace


static bool
run_command (
             Interpreter  *intp,
             DebugTracer  *tracer,
             const char  **tokens,
             bool         *stop,
             char        **err
             )
{

  // Línia buida. Crec que no és possible.
  if ( *tokens == NULL ) return true;

  // Processa
  if ( !strcmp ( *tokens, "help" ))
    help ();
  else if ( !strcmp ( *tokens, "enable_trace" ) )
    enable_trace ( tracer, tokens );
  else if ( !strcmp ( *tokens, "disable_trace" ) )
    disable_trace ( tracer, tokens );
  else if ( !strcmp ( *tokens, "trace" ) )
    {
      if ( !trace ( intp, tokens, err ) )
        return false;
    }
  else if ( !strcmp ( *tokens, "quit" ) )
    *stop= true;
  else
    ww ( "Unknown command '%s'", *tokens );
  
  return true;
  
} // end run_command




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

bool
debugger_run (
              const char      *zcode_fn,
              Conf            *conf,
              const gboolean   verbose,
              char           **err
              )
{

  Tokenizer *t;
  Interpreter *intp;
  const char **tokens;
  DebugTracer *tracer;
  bool stop;
  
  
  // Preparació.
  t= NULL;
  intp= NULL;
  tracer= NULL;

  // Inicialitza.
  t= tokenizer_new ( stdin, err );
  if ( t == NULL ) goto error;
  tracer= debug_tracer_new ( 0x00 ); // Inicialment tot desactivat
  if ( tracer == NULL ) goto error;
  if ( verbose )
    ii ( "Loading Z-Code file '%s' ...", zcode_fn );
  intp= interpreter_new_from_file_name ( zcode_fn, conf, NULL, verbose,
                                         TRACER(tracer), err );
  if ( intp == NULL ) goto error;

  // Processa línia
  stop= false;
  while ( !stop && (tokens= tokenizer_get_line ( t, err ) ) != NULL )
    if ( !run_command ( intp, tracer, tokens, &stop, err ) )
      goto error;
  if ( tokenizer_check_error ( t ) ) goto error;
  
  // Allibera memòria.
  interpreter_free ( intp );
  debug_tracer_free ( tracer );
  tokenizer_free ( t );
  
  return true;

 error:
  if ( intp != NULL ) interpreter_free ( intp );
  if ( tracer != NULL ) debug_tracer_free ( tracer );
  if ( t != NULL ) tokenizer_free ( t );
  return false;
  
} // end debugger_run
