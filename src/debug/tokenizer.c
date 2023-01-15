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
 *  tokenizer.c - Implementació de 'tokenizer.h'.
 *
 */


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "tokenizer.h"
#include "utils/error.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static bool
line_add_char (
               Tokenizer  *t,
               const int   c,
               char      **err
               )
{

  size_t nsize;

  
  if ( t->line_N == t->line_size )
    {
      nsize= t->line_size*2;
      if ( nsize <= t->line_size )
        {
          msgerror ( err,
                     "[tokenizer] Failed to read next line: line too large" );
          t->error= true;
          return false;
        }
      t->line= g_renew ( char, t->line, nsize );
      t->line_size= nsize;
    }
  t->line[t->line_N++]= (char) c;
  
  return true;
  
} // end line_add_char


// false pot significar eof o error. Cal fer un check_error.
static bool
read_next_line (
                Tokenizer  *t,
                char      **err
                )
{

  int c;

  
  assert ( !t->error );

  // Clear i Busca el primer caràcter no espai.
  t->line_N= 0;
  while ( (c= fgetc ( t->f )) != EOF && isspace ( c ) );
  if ( c == EOF ) goto return_eof;
  if ( !line_add_char ( t, c, err ) ) return false;

  // Mentre no trobe un '\n' continua llegint.
  while ( (c= fgetc ( t->f )) != EOF && c != '\n' )
    if ( c != '\a' && c != '\b' && c != '\r' && c != '\0' )
      if ( !line_add_char ( t, c, err ) ) return false;
  if ( c == EOF && ferror ( t->f ) ) goto return_eof;

  // Acaba línia
  if ( !line_add_char ( t, '\0', err ) ) return false;
  
  return true;
  
 return_eof:
  if ( ferror ( t->f ) )
    {
      t->error= true;
      if ( errno != 0 )
        {
          msgerror ( err, "[tokenizer] Failed to read next line: %s",
                     strerror ( errno ) );
          errno= 0;
        }
      else msgerror ( err, "[tokenizer] Failed to read next line" );
    }
  return false;
  
} // end read_next_line


static bool
add_token (
           Tokenizer  *t,
           char       *token,
           char      **err
           )
{

  size_t nsize;

  
  if ( t->tokens_N == t->tokens_size )
    {
      nsize= t->tokens_size*2;
      if ( nsize <= t->tokens_size )
        {
          msgerror ( err,
                     "[tokenizer] Failed to tokenise next line:"
                     " too much tokens" );
          t->error= true;
          return false;
        }
      t->tokens= g_renew ( char *, t->tokens, nsize );
      t->tokens_size= nsize;
    }
  t->tokens[t->tokens_N++]= token;
  
  return true;
  
} // end add_token


static bool
tokenise_line (
               Tokenizer  *t,
               char      **err
               )
{

  char *p;

  
  assert ( !t->error );

  t->tokens_N= 0;
  p= t->line;
  for (;;)
    {

      // Busca principi token.
      for ( ; *p != '\0' && isspace(*p); ++p );
      if ( *p == '\0' ) break;
      
      // Afegeix token
      if ( !add_token ( t, p, err ) ) return false;

      // Busca final token
      for ( ++p; *p != '\0' && !isspace(*p); ++p );
      if ( *p == '\0' ) break;
      *(p++)= '\0';
      
    }
  if ( !add_token ( t, NULL, err ) ) return false;
  
  return true;
  
} // end tokenizer_line




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
tokenizer_free (
                Tokenizer *t
                )
{

  g_free ( t->tokens );
  g_free ( t->line );
  g_free ( t );
  
} // end tokenizer_free


Tokenizer *
tokenizer_new (
               FILE  *f, // No el tanca
               char **err
               )
{

  Tokenizer *ret;


  ret= g_new ( Tokenizer, 1 );
  ret->f= f;
  ret->error= false;
  ret->line_size= 1;
  ret->line= g_new ( char, ret->line_size );
  ret->line_N= 0;
  ret->tokens_size= 1;
  ret->tokens= g_new ( char *, ret->tokens_size );
  ret->tokens_N= 0;
  
  return ret;
  
} // end tokenizer_new


const char **
tokenizer_get_line (
                    Tokenizer *t,
                    char      **err
                    )
{

  if ( t->error ) return NULL;
  if ( !read_next_line ( t, err ) ) return NULL;
  if ( !tokenise_line ( t, err ) ) return NULL;
  
  return (const char **) t->tokens;
  
} // end tokenizer_get_line
