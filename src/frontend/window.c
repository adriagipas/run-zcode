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
 *  window.c - Implementa 'window.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "utils/error.h"
#include "utils/log.h"
#include "window.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
set_wsize (
           Window    *win,
           const int  width,
           const int  height
           )
{

  win->_wwidth0= width;
  win->_wheight0= height;
  win->_fullscreen= (width<=0 || height<=0);
  if ( win->_fullscreen )
    {
      win->_wwidth= win->_bounds.w-win->_bounds.x;
      win->_wheight= win->_bounds.h-win->_bounds.y;
    }
  else
    {
      win->_wwidth= width;
      win->_wheight= height;
    }
  
} // end set_wsize


static int
get_desp (
          const Uint32 mask
          )
{
  
  switch ( mask )
    {
    case 0x000000ff: return 0;
    case 0x0000ff00: return 8;
    case 0x00ff0000: return 16;
    case 0xff000000: return 24;
    default: return -1;
    }
  
} // end get_desp


static bool
calc_desp_rgba (
                Window  *win,
                char   **err
                )
{

  Uint32 rmask, gmask, bmask, amask;
  int bpp;
  

  // Obté màscares
  if ( !SDL_PixelFormatEnumToMasks( win->_pfmt, &bpp, &rmask,
                                    &gmask, &bmask, &amask ) )
    {
      msgerror ( err, "Failed to create SDL window: %s", SDL_GetError () );
      return false;
    }
  if ( bpp != 32 ) goto error;
  
  // Obté desplaçaments.
  win->_desp_r= get_desp ( rmask );
  win->_desp_g= get_desp ( gmask );
  win->_desp_b= get_desp ( bmask );
  win->_desp_a= get_desp ( amask );
  if ( amask == 0 ) win->_desp_a= 32; // Indica que no es gasta win->_desp_a
  if ( win->_desp_r == -1 || win->_desp_g == -1 ||
       win->_desp_b == -1 || win->_desp_a == -1 )
    goto error;
  
  return true;
  
 error:
  msgerror ( err, "Failed to create SDL window: unsupported pixel format" );
  return false;
  
} // end calc_desp_rgba


static bool
init_sdl (
          Window       *win,
          const int     wwidth,
          const int     wheight,
          const int     fbwidth,
          const int     fbheight,
          const char   *title,
          SDL_Surface  *icon,
          char        **err
          )
{

  assert ( title != NULL );

  // Obté les dimensions del desktop.
  if ( SDL_GetDisplayBounds ( 0, &(win->_bounds) ) != 0 )
    {
      msgerror ( err, "Failed to estimate screen dimensions: %s",
                 SDL_GetError () );
      return false;
    }

  // Crea la finestra
  set_wsize ( win, wwidth, wheight );
  win->_win= SDL_CreateWindow ( title,
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                win->_wwidth, win->_wheight,
                                0 );
  if ( win->_win == NULL )
    {
      msgerror ( err, "Failed to create SDL window: %s", SDL_GetError () );
      return false;
    }
  win->_pfmt= SDL_GetWindowPixelFormat ( win->_win );
  if ( win->_pfmt == SDL_PIXELFORMAT_UNKNOWN )
    {
      msgerror ( err, "Uknonwn pixel format: %s", SDL_GetError () );
      return false;
    }
  if ( win->_fullscreen )
    {
      if ( SDL_SetWindowFullscreen ( win->_win,
                                     SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0 )
        {
          msgerror ( err, "Failed to enter fullscreen mode: %s",
                     SDL_GetError () );
          return false;
        }
    }

  // Fixa la icona
  if ( icon != NULL ) SDL_SetWindowIcon ( win->_win, icon );
  
  // Amaga el cursor.
  win->_cursor_enabled= false;
  SDL_ShowCursor ( 0 );

  // Crea el renderer
  win->_renderer= SDL_CreateRenderer ( win->_win, -1,
                                       SDL_RENDERER_ACCELERATED |
                                       SDL_RENDERER_PRESENTVSYNC );
  if ( win->_renderer == NULL )
    {
      msgerror ( err, "Failed to create SDL renderer: %s", SDL_GetError () );
      return false;
    }

  // Crea el framebuffer
  win->_fbwidth= fbwidth;
  win->_fbheight= fbheight;
  win->_fb= SDL_CreateTexture ( win->_renderer, win->_pfmt,
                                SDL_TEXTUREACCESS_STREAMING,
                                fbwidth, fbheight );
  if ( win->_fb == NULL )
    {
      msgerror ( err, "Failed to create SDL texture: %s", SDL_GetError ()  );
      return false;
    }

  // Calcula els desplaçaments.
  if ( !calc_desp_rgba ( win, err ) ) return false;
  
  return true;
  
} // end init_sdl


static void
update_coords (
               Window *win
               )
{
  
  double fbratio, wratio;
  
  
  fbratio= win->_fbwidth / (double) win->_fbheight;
  wratio= (double) win->_wwidth / (double) win->_wheight;
  if ( wratio >= fbratio ) // Marges horizontals
    {
      win->_coords.w= (int) ((fbratio/wratio)*win->_wwidth + 0.5);
      if ( win->_coords.w > win->_wwidth ) win->_coords.w= win->_wwidth;
      win->_coords.h= win->_wheight;
      win->_coords.x= (win->_wwidth-win->_coords.w)/2;
      win->_coords.y= 0;
    }
  else // Marges verticals
    {
      win->_coords.w= win->_wwidth;
      win->_coords.h= (int) ((wratio/fbratio)*win->_wheight + 0.5);
      if ( win->_coords.h > win->_wheight ) win->_coords.h= win->_wheight;
      win->_coords.x= 0;
      win->_coords.y= (win->_wheight-win->_coords.h)/2;
    }
  
} // end update_coords


static bool
update_fbsize (
               Window     *win,
               const int   width,
               const int   height,
               char      **err
               )
{

  // Allibera memòria.
  SDL_DestroyTexture ( win->_fb );
  win->_fb= NULL;

  // Crea nou framebuffer.
  win->_fb= SDL_CreateTexture ( win->_renderer, win->_pfmt,
                                SDL_TEXTUREACCESS_STREAMING,
                                width, height );
  if ( win->_fb == NULL )
    {
      msgerror ( err, "Failed to create SDL texture: %s", SDL_GetError () );
      return false;
    }
  win->_fbwidth= width;
  win->_fbheight= height;
  update_coords ( win );

  return true;
  
} // end update_fbsize


static void
draw (
      Window *win
      )
{

  SDL_SetRenderDrawColor ( win->_renderer, 0, 0, 0, 0xff );
  SDL_RenderFillRect ( win->_renderer, NULL );
  SDL_RenderCopy ( win->_renderer, win->_fb, NULL, &(win->_coords) );
  SDL_RenderPresent ( win->_renderer );
  
} // end draw




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
window_free (
             Window *win
             )
{

  if ( win->_fb != NULL ) SDL_DestroyTexture ( win->_fb );
  if ( win->_renderer != NULL ) SDL_DestroyRenderer ( win->_renderer );
  if ( win->_win != NULL ) SDL_DestroyWindow ( win->_win );
  g_free ( win );
  
} // end window_free


Window *
window_new (
            const int     window_width,
            const int     window_height,
            const int     fb_width,
            const int     fb_height,
            const char   *title, // Pot ser NULL
            SDL_Surface  *icon,
            char        **err
            )
{

  Window *ret;

  
  // Prepara
  ret= g_new ( Window, 1 );
  ret->_win= NULL;
  ret->_renderer= NULL;
  ret->_fb= NULL;
  
  // Inicialitza
  if ( !init_sdl ( ret, window_width, window_height,
                   fb_width, fb_height, title, icon, err ) )
    goto error;
  update_coords ( ret );
  
  return ret;
  
 error:
  window_free ( ret );
  return NULL;
  
} // end window_new


bool
window_next_event (
                   Window    *win,
                   SDL_Event *event
                   )
{

  while ( SDL_PollEvent ( event ) )
    switch ( event->type )
      {
      case SDL_WINDOWEVENT:
        if ( event->window.event == SDL_WINDOWEVENT_EXPOSED )
          draw ( win );
        else return true;
        break;

        /* TODO!!!!
        // Si està habilitat el suport del cursor, tots els events
        // fora de l'àrea del framebuffer s'ignoren, i en els que
        // estan dins abans de tornar-los es reajusten les coordenades
        // perquè facen referència al framebuffer i no a la finestra.
      case SDL_MOUSEMOTION:
        if ( _sdl.cursor_enabled )
          {
            if ( translate_xy_cursor_coords ( &(event->motion.x),
                                              &(event->motion.y) ) )
              return TRUE;
          }
        break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
        if ( _sdl.cursor_enabled )
          {
            if ( translate_xy_cursor_coords ( &(event->button.x),
                                              &(event->button.y) ) )
              return TRUE;
          }
        break;
      case SDL_MOUSEWHEEL:
        if ( _sdl.cursor_enabled ) return TRUE;
        break;
        */
      default: return true;
      }
  
  return false;
  
} // end window_next_event


bool
window_set_size (
                 Window     *win,
                 const int   width,
                 const int   height,
                 char      **err
                 )
{

  // Comprova dimensió anterior.
  if ( width == win->_wwidth0 && height == win->_wheight0 )
    return true;
  
  // Obté noves dimensions i redimensiona.
  if ( win->_fullscreen )
    {
      if ( SDL_SetWindowFullscreen ( win->_win, 0 ) != 0 ) goto sdl_error;
    }
  set_wsize ( win, width, height );
  SDL_SetWindowSize ( win->_win, win->_wwidth, win->_wheight );
  if ( win->_fullscreen )
    {
      if ( SDL_SetWindowFullscreen ( win->_win,
                                     SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0 )
        goto sdl_error;
    }
  
  // Recalcula posició.
  update_coords ( win );
  
  // Dibuixa.
  draw ( win );
  
  return true;
  
 sdl_error:
  msgerror ( err, "Failed to change window size: %s", SDL_GetError () );
  return false;
  
} // end window_set_size


bool
window_set_fbsize (
                   Window     *win,
                   const int   width,
                   const int   height,
                   char      **err
                   )
{

  if ( width == win->_fbwidth && height == win->_fbheight )
    return true;
  if ( !update_fbsize ( win, width, height, err ) )
    return false;

  return true;
  
} // end window_set_fbsize


void
window_set_title (
                  Window     *win,
                  const char *title
                  )
{
  SDL_SetWindowTitle ( win->_win, title );
} // end window_set_title


bool
window_update (
               Window          *win,
               const uint32_t  *fb,
               char           **err
               )
{
  
  int r,c,i,pitch;
  uint8_t *buffer;
  
  
  if ( SDL_LockTexture ( win->_fb, NULL, (void **) &buffer, &pitch ) != 0 )
    {
      msgerror ( err, "Failed to update window frame buffer: %s",
                 SDL_GetError () );
      return false;
    }
  for ( r= i= 0; r < win->_fbheight; ++r )
    {
      for ( c= 0; c < win->_fbwidth; ++c, ++i )
        ((uint32_t *) buffer)[c]= fb[i];
      buffer+= pitch;
    }
  SDL_UnlockTexture ( win->_fb );
  draw ( win );
  
  return true;
  
} // end window_update


void
window_redraw (
               Window *win
               )
{
  draw ( win );
} // end window_redraw


void
window_hide (
             Window *win
             )
{
  SDL_HideWindow ( win->_win );
} // end window_hide


void
window_show (
             Window *win
             )
{
  SDL_ShowWindow ( win->_win );
} // end window_show


void
window_raise (
              Window *win
              )
{
  
  SDL_ShowWindow ( win->_win );
  SDL_RaiseWindow ( win->_win );
  
} // end window_raise


uint32_t
window_get_color (
                  const Window *win,
                  const uint8_t r,
                  const uint8_t g,
                  const uint8_t b
                  )
{

  uint32_t ret;

  
  if ( win->_desp_a == 32 )
    ret=
      (r<<win->_desp_r) |
      (g<<win->_desp_g) |
      (b<<win->_desp_b);
  else
    ret=
      (r<<win->_desp_r) |
      (g<<win->_desp_g) |
      (b<<win->_desp_b) |
      (0xff<<win->_desp_a);
  
  return ret;
  
} // end window_get_color


SDL_Surface *
window_get_surface (
                    const Window  *win,
                    const int      width,
                    const int      height,
                    char         **err
                    )
{
  
  SDL_Surface *ret;
  
  
  ret= SDL_CreateRGBSurface ( 0, width, height, 32,
                              0x000000ff<<win->_desp_r,
                              0x000000ff<<win->_desp_g,
                              0x000000ff<<win->_desp_b,
                              win->_desp_a==32 ? 0 :
                              (0x000000ff<<win->_desp_a) );
  if ( ret == NULL )
    {
      msgerror ( err, "Failed to create SDL surface: %s", SDL_GetError () );
      return NULL;
    }
  
  return ret;
  
} // end window_get_surface
