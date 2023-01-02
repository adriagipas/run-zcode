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

  gboolean verbose;
  gboolean debug;
  
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
      FALSE, // verbose
      FALSE  // debug
    };

  static GOptionEntry entries[]=
    {
      { "verbose", 'v', 0, G_OPTION_ARG_NONE, &vals.verbose,
        "Verbose",
        NULL },
      { "debug", 'D', 0, G_OPTION_ARG_NONE, &vals.debug,
        "Enable debug mode (interactive console)" },
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
} // end free_opts




/**********************/
/* PROGRAMA PRINCIPAL */
/**********************/

#include "core/story_file.h"

int main ( int argc, char *argv[] )
{

  struct args args;
  struct opts opts;
  
  
  setlocale ( LC_ALL, "" );

  usage ( &argc, &argv, &args, &opts );

  printf ( "ZCODE: %s\n", args.zcode_fn );
  printf ( "DEBUG: %d\n", opts.debug );
  printf ( "VERBOSE: %d\n", opts.verbose );

  char *err= NULL;
  StoryFile *sf;
  sf= story_file_new_from_file_name ( args.zcode_fn, &err );
  if ( sf == NULL )
    {
      fprintf ( stderr, "Error: %s\n", err );
      free ( err );
      exit ( EXIT_FAILURE );
    }
  printf ( "SF size: %lu\n", sf->size );
  story_file_free ( sf );
  
  free_opts ( &opts );
  
  return EXIT_SUCCESS;
  
}
