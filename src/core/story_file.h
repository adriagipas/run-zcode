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
 *  story_file.h - Representa en memòria el ZCode i punter als
 *                 recursos.
 *
 */

#ifndef __CORE__STORY_FILE_H__
#define __CORE__STORY_FILE_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  STORY_FILE_RESOURCE_PICTURE_PNG= 0,
  STORY_FILE_RESOURCE_PICTURE_JPEG,
  STORY_FILE_RESOURCE_PICTURE_PLACEHOLDER,
  STORY_FILE_RESOURCE_SOUND_AIFF,
  STORY_FILE_RESOURCE_SOUND_OGG,
  STORY_FILE_RESOURCE_SOUND_MOD
} StoryFileResourceType;

typedef struct
{
  StoryFileResourceType  type;   // Tipus
  long                   offset; // Posició dins del fitxer.
  size_t                 size;   // Grandària del recurs.
  char                  *desc;   // Descripció textual UTF-8. Pot ser
                                 // NULL.
} StoryFileResource;

typedef struct
{
  uint8_t           *data;      // Bytes del story file
  size_t             size;      // Nombre de bytes
  StoryFileResource *resources; // Llista de 'resources'
  uint32_t           Nres;      // Nombre de 'resources'
  FILE              *fres;      // Descriptor fitxer on llegir
                                // 'resources'.
} StoryFile;


void
story_file_free (
                 StoryFile *sf
                 );

// Torna NULL en cas d'error.
StoryFile *
story_file_new_from_file_name (
                               const char  *file_name,
                               char       **err
                               );

#endif // __CORE__STORY_FILE_H__
