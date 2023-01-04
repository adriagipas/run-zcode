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
 *  error.h - Gestió d'errors.
 *
 */

#ifndef __UTILS__ERROR_H__
#define __UTILS__ERROR_H__

// Crea un missatge d'error en la variable indicada sempre que no siga
// NULL, en cas contrari no fa res.
void
msgerror (
          char       **err,
          const char  *format,
          ...
          );

// Com msgerror, però comprova si errno està actiu i en eixe cas
// afegeix al missatge l'error de errno.
void
error_file_ (
             char       **err,
             const char  *msg,
             const char  *file_name
             );

#define error_create_file(ERR,FILE_NAME)                        \
  error_file_ ( (ERR), "Failed to create file", (FILE_NAME) )

#define error_open_file(ERR,FILE_NAME)                          \
  error_file_ ( (ERR), "Failed to open file", (FILE_NAME) )

#define error_read_file(ERR,FILE_NAME)                          \
  error_file_ ( (ERR), "Failed to read from file", (FILE_NAME) )

#define error_write_file(ERR,FILE_NAME)                                 \
  error_file_ ( (ERR), "Failed to write into file", (FILE_NAME) )

#endif // __UTILS__ERROR_H__
