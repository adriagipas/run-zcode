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


#include <glib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "debugger.h"
#include "tokenizer.h"




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

bool
debugger_run (
              const gboolean   verbose,
              char           **err
              )
{

  Tokenizer *t;
  const char **tokens;
  
  
  // Preparació.
  t= NULL;

  // Inicialitza.
  t= tokenizer_new ( stdin, err );
  if ( t == NULL ) goto error;
  
  while ( (tokens= tokenizer_get_line ( t, err ) ) != NULL )
    {
      for ( ; *tokens != NULL; ++tokens )
        printf ( "[%s]", *tokens );
      printf ( "\n" );
    }
  if ( tokenizer_check_error ( t ) ) goto error;
  
  // Allibera memòria.
  tokenizer_free ( t );
  
  return true;

 error:
  if ( t != NULL ) tokenizer_free ( t );
  return false;
  
} // end debugger_run
