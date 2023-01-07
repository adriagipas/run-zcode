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
 *  log.h - Per a mostrar missatges per pantalla.
 *
 */

#ifndef __UTILS__LOG_H__
#define __UTILS__LOG_H__

// Informació
void
ii (
    const char *format,
    ...
    );

// Avisos
void
ww (
    const char *format,
    ...
    );

// Errors
void
ee (
    const char *format,
    ...
    );

#endif // __UTILS__LOG_H__
