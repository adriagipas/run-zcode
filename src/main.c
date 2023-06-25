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
#include <libintl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

#include "core/interpreter.h"
#include "core/story_file.h"
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
  gchar    *cover_fn;
  
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
      NULL,   // transcript_fn
      NULL    // cover_fn
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
      { "cover", 'C', 0, G_OPTION_ARG_STRING, &vals.cover_fn,
        "Extract the frontispiece image (cover) and store it in the"
        " provided file. When this option is selected the story file"
        " is not executed. If no frontispiece image is present in the"
        " story file the application fails." },
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

  g_free ( opts->cover_fn );
  g_free ( opts->transcript_fn );
  g_free ( opts->conf_fn );
  
} // end free_opts


// Torna cert si s'ha pogut extraure.
static bool
extract_cover (
               const gchar *sf_fn,
               const gchar *cover_fn
               )
{

  const size_t BLOCK_SIZE= 1024;
  
  char *err;
  StoryFile *sf;
  uint8_t *data,*p;
  size_t size,remain,writeb;
  bool exist_cover;
  FILE *f;
  
  
  // Prepara.
  err= NULL;
  sf= NULL;
  data= NULL;
  f= NULL;
  exist_cover= true;

  // Extrau imatge.
  sf= story_file_new_from_file_name ( sf_fn, &err );
  if ( sf == NULL ) goto error;
  if ( !story_file_get_frontispiece ( sf, &data, &size, &err ) )
    goto error;
  exist_cover= data!=NULL;

  // Escriu en fitxer.
  if ( exist_cover )
    {
      f= fopen ( cover_fn, "wb" );
      if ( f == NULL )
        {
          msgerror ( &err, "Failed to create '%s'", cover_fn );
          goto error;
        }
      remain= size;
      p= data;
      while ( remain > 0 )
        {
          writeb= remain > BLOCK_SIZE ? BLOCK_SIZE : remain;
          if ( fwrite ( p, writeb, 1, f ) != 1 )
            goto error;
          p+= writeb;
          remain-= writeb;
        }
      fclose ( f );
    }
  
  // Allibera memòria.
  g_free ( data );
  story_file_free ( sf );
  
  return exist_cover;
  
 error:
  if ( f != NULL ) fclose ( f );
  g_free ( data );
  if ( sf != NULL ) story_file_free ( sf );
  fprintf ( stderr, "[EE] %s\n", err  );
  g_free ( err );
  return false;
  
} // end extract_cover




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
  bool ok;
  
  
  // Prepara.
  setlocale ( LC_ALL, "" );
  bindtextdomain ( GETTEXT_PACKAGE, GETTEXT_LOCALEDIR );
  bind_textdomain_codeset ( GETTEXT_PACKAGE, "UTF-8" );
  textdomain ( GETTEXT_PACKAGE );
  err= NULL;
  intp= NULL;
  conf= NULL;
  
  // Parseja opcions i configuració.
  usage ( &argc, &argv, &args, &opts );
  // --> Cas especial extraure cover
  if ( opts.cover_fn != NULL )
    {
      ok= extract_cover ( args.zcode_fn, opts.cover_fn );
      free_opts ( &opts );
      return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  conf= conf_new ( opts.verbose, opts.conf_fn, &err );
  if ( conf == NULL ) goto error;
  if ( SDL_Init ( SDL_INIT_VIDEO|SDL_INIT_EVENTS ) != 0 )
    {
      msgerror ( &err, "Failed to initialize SDL: %s", SDL_GetError () );
      goto error;
    }
  
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
