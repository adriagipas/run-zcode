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
 *  debugger.h - Implementa la depuració.a
 *
 */

#ifndef __DEBUG__DEBUGGER_H__
#define __DEBUG__DEBUGGER_H__

#include <glib.h>
#include <stdbool.h>

bool
debugger_run (
              const char      *zcode_fn,
              const gboolean   verbose,
              char           **err
              );

#endif // __DEBUG__DEBUGGER_H__
