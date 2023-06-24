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
 *  window.h - Finestra bàsica que renderitza un 'frame buffer'.
 *
 */

#ifndef __FRONTEND__WINDOW_H__
#define __FRONTEND__WINDOW_H__

#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>

typedef struct
{

  // CAMPS PRIVATS
  SDL_Rect      _bounds; // Grandària del display.
  int           _wwidth,_wwidth0;
  int           _wheight,_wheight0;
  bool          _fullscreen;
  SDL_Window   *_win;
  Uint32        _pfmt;
  bool          _cursor_enabled;
  SDL_Renderer *_renderer;
  SDL_Texture  *_fb;
  int           _fbwidth;
  int           _fbheight;
  int           _desp_r,_desp_g,_desp_b,_desp_a;
  SDL_Rect      _coords;
  
} Window;

void
window_free (
             Window *win
             );

// window_width i window_height són les dimensions de la finestra,
// fbwidth i fbheight són les dimensions del framebuffer, title és el
// títol. Si icon és NULL no es fica icona.
Window *
window_new (
            const int     window_width,
            const int     window_height,
            const int     fb_width,
            const int     fb_height,
            const char   *title,
            SDL_Surface  *icon,
            char        **err
            );

// Torna true si s'ha pogut llegir un event.
bool
window_next_event (
                   Window    *win,
                   SDL_Event *event
                   );

// De la finestra.
bool
window_set_size (
                 Window     *win,
                 const int   width,
                 const int   height,
                 char      **err
                 );

// Del frambuffer.
bool
window_set_fbsize (
                   Window     *win,
                   const int   width,
                   const int   height,
                   char      **err
                   );

void
window_set_title (
                  Window     *win,
                  const char *title
                  );

bool
window_update (
               Window          *win,
               const uint32_t  *fb,
               char           **err
               );

void
window_redraw (
               Window *win
               );

void
window_hide (
             Window *win
             );

void
window_show (
             Window *win
             );

void
window_raise (
              Window *win
              );

uint32_t
window_get_color (
                  const Window *win,
                  const uint8_t r,
                  const uint8_t g,
                  const uint8_t b
                  );

// Crea un surface amb el format que utilitza la finestra.
SDL_Surface *
window_get_surface (
                    const Window  *win,
                    const int      width,
                    const int      height,
                    char         **err
                    );

#endif // __FRONTEND__WINDOW_H__
