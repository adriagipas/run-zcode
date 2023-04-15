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
 *  main.c - Programa principal.
 *
 */


#include <glib.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "core/interpreter.h"
#include "debug/debugger.h"
#include "frontend/conf.h"
#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define NUM_ARGS 1




/*********/
/* TIPUS */
/*********/

struct args
{

  const gchar *zcode_fn;
  
};


struct opts
{

  gboolean  verbose;
  gboolean  debug;
  gchar    *conf_fn;
  gchar    *transcript_fn;
  
};




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
usage (
       int          *argc,
       char        **argv[],
       struct args  *args,
       struct opts  *opts
       )
{
  
  static struct opts vals=
    {
      FALSE,  // verbose
      FALSE,  // debug
      NULL,   // conf_fn
      NULL
    };

  static GOptionEntry entries[]=
    {
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &vals.verbose,
        "Verbose",
        NULL },
      { "debug", 'D', 0, G_OPTION_ARG_NONE, &vals.debug,
        "Enable debug mode (interactive console)" },
      { "conf", 'c', 0, G_OPTION_ARG_STRING, &vals.conf_fn,
        "Specify the configuration file. By default use the"
        " standard configuration file" },
      { "transcript", 'T', 0, G_OPTION_ARG_STRING, &vals.transcript_fn,
        "Specify the file used to write the transcription" },
      { NULL }
    };
  
  GError *err;
  GOptionContext *context;
  
  
  // Parseja opcions.
  err= NULL;
  context= g_option_context_new ( "<story-file> - run Z-Machine story"
                                  " files on Unix" );
  g_option_context_add_main_entries ( context, entries, NULL );
  if ( !g_option_context_parse ( context, argc, argv, &err ) )
    {
      fprintf ( stderr, "%s\n", err->message );
      exit ( EXIT_FAILURE );
    }
  *opts= vals;
  
  // Comprova arguments.
  if ( *argc-1 != NUM_ARGS )
    {
      fprintf ( stderr, "%s\n",
                g_option_context_get_help ( context, TRUE, NULL ) );
      exit ( EXIT_FAILURE );
    }
  args->zcode_fn= (*argv)[1];
  
  // Allibera
  g_option_context_free ( context );
  
} // end usage


static void
free_opts (
           struct opts *opts
           )
{

  g_free ( opts->transcript_fn );
  g_free ( opts->conf_fn );
  
} // end free_opts




/**********************/
/* PROGRAMA PRINCIPAL */
/**********************/

int main ( int argc, char *argv[] )
{
  
  struct args args;
  struct opts opts;
  Interpreter *intp;
  Conf *conf;
  char *err;
  

  // Prepara.
  setlocale ( LC_ALL, "" );
  err= NULL;
  intp= NULL;
  conf= NULL;
  if ( SDL_Init ( SDL_INIT_VIDEO|SDL_INIT_EVENTS ) != 0 )
    {
      msgerror ( &err, "Failed to initialize SDL: %s", SDL_GetError () );
      goto error;
    }
  
  // Parseja opcions i configuració.
  usage ( &argc, &argv, &args, &opts );
  conf= conf_new ( opts.verbose, opts.conf_fn, &err );
  if ( conf == NULL ) goto error;
  
  // Executa.
  if ( opts.debug )
    {
      if ( !debugger_run ( args.zcode_fn, conf, opts.verbose, &err ) )
        goto error;
    }
  else
    {
      intp= interpreter_new_from_file_name ( args.zcode_fn, conf,
                                             opts.transcript_fn,
                                             opts.verbose, NULL, &err );
      if ( intp == NULL ) goto error;
      if ( !interpreter_run ( intp, &err ) ) goto error;
      interpreter_free ( intp ); intp= NULL;
    }
  
  // Allibera i acaba.
  if ( !conf_write ( conf, &err ) ) goto error;
  conf_free ( conf );
  free_opts ( &opts );
  SDL_Quit ();
  
  return EXIT_SUCCESS;
  
 error:
  fprintf ( stderr, "[EE] %s\n", err  );
  g_free ( err );
  if ( conf != NULL ) conf_free ( conf );
  if ( intp != NULL ) interpreter_free ( intp );
  free_opts ( &opts );
  SDL_Quit ();
  return EXIT_FAILURE;
  
}
