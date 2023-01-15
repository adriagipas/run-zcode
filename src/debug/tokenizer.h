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
 *  tokenizer.h - Tokenitza línia a línia un fitxer d'entrada.
 *
 */

#ifndef __DEBUG__TOKENIZER_H__
#define __DEBUG__TOKENIZER_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct
{

  // CAMPS PRIVATS!.
  FILE    *f;
  bool     error;
  char    *line;
  size_t   line_size;
  size_t   line_N;
  char   **tokens;
  size_t   tokens_size;
  size_t   tokens_N;
  
} Tokenizer;

void
tokenizer_free (
                Tokenizer *t
                );

Tokenizer *
tokenizer_new (
               FILE  *f, // No el tanca
               char **err
               );

// Torna un vector de tokens amb l'últim apuntant a NULL. Si torna
// NULL vol dir que no s'ha pogut llegir la següent línia. Per a
// comprovar si ha sigut un error o no cal cridar a check_error.
const char **
tokenizer_get_line (
                    Tokenizer *t,
                    char      **err
                    );

// Comprova si està en estat d'error.
#define tokenizer_check_error(T) ((T)->error)

#endif // __DEBUG__TOKENIZER_H__
