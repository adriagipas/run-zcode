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
 *  log.c - Implementació de 'log.h'.
 *
 */


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
print_msg (
           FILE       *f,
           const char *prefix,
           const char *format,
           va_list     ap
           )
{

  fprintf ( f, "%s", prefix );
  vfprintf ( f, format, ap );
  fprintf ( f, "\n" );
  fflush ( f );
  
} // end print_msg




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
ii (
    const char *format,
    ...
    )
{

  va_list ap;
  
  
  va_start ( ap, format );
  print_msg ( stderr, "[II] ", format, ap );
  va_end ( ap );
  
} // end ii


void
ww (
    const char *format,
    ...
    )
{

  va_list ap;
  
  
  va_start ( ap, format );
  print_msg ( stderr, "[WW] ", format, ap );
  va_end ( ap );
  
} // end ww


void
ee (
    const char *format,
    ...
    )
{

  va_list ap;
  
  
  va_start ( ap, format );
  print_msg ( stderr, "[EE] ", format, ap );
  va_end ( ap );
  exit ( EXIT_FAILURE );
  
} // end ee
