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
 *  error.c - Implementació de 'error.h'.
 *
 */


#include <errno.h>
#include <glib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
msgerror (
          char       **err,
          const char  *format,
          ...
          )
{

  va_list ap;
  static char buffer[1000];
  
  
  if  ( err == NULL ) return;
  va_start ( ap, format );
  vsnprintf ( buffer, 1000, format, ap );
  va_end ( ap );
  *err= g_new ( char, strlen(buffer)+1 );
  strcpy ( *err, buffer );
  
} // end msgerror


void
error_file_ (
             char       **err,
             const char  *msg,
             const char  *file_name
             )
{

  if ( errno != 0 )
    msgerror ( err, "%s [%s]: %s",
               msg, strerror ( errno ), file_name );
  else
    msgerror ( err, "%s: %s", msg, file_name );
  
} // end error_file_
