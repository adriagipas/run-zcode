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
 *  screen.c - Implementació de 'screen.h'.
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include "screen.h"
#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define C_BLACK   0x0000
#define C_RED     0x001D
#define C_GREEN   0x0340
#define C_YELLOW  0x03BD
#define C_BLUE    0x59A0
#define C_MAGENTA 0x7C1F
#define C_CYAN    0x77A0
#define C_WHITE   0x7FFF

#define W_UP  1
#define W_LOW 0




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static uint32_t
true_color_to_u32 (
                   const Screen   *s,
                   const uint16_t  color
                   )
{

  uint8_t r,g,b;


  r= (color&0x1F)<<3;
  g= ((color>>5)&0x1F)<<3;
  b= ((color>>10)&0x1F)<<3;

  return window_get_color ( s->_win, r, g, b );
  
} // end true_color_to_u32


static void
true_color_to_sdlcolor (
                        const uint16_t  color,
                        SDL_Color      *ret
                        )
{

  ret->r= (color&0x1F)<<3;
  ret->g= ((color>>5)&0x1F)<<3;
  ret->b= ((color>>10)&0x1F)<<3;
  ret->a= 0xff;
  
} // end true_color_to_sdlcolor


static bool
redraw_fb (
           Screen  *s,
           char   **err
           )
{
  return window_update ( s->_win, s->_fb, err );
} // end redraw_fb


static void
scroll_low (
            Screen *s
            )
{

  uint32_t *fb;
  uint32_t color;
  size_t i,j,line_size,nvals;

  
  fb= s->_fb;

  assert ( s->_upwin_lines < s->_lines );
  
  // Cópia amunt
  line_size= ((size_t) s->_line_height)*((size_t) s->_width);
  nvals= ((size_t) ((s->_lines-s->_upwin_lines)-1)) * line_size;
  for ( i= line_size*((size_t) s->_upwin_lines), j= i+line_size;
        i < nvals;
        ++i, ++j )
    fb[i]= fb[j];

  // Última línia
  color= true_color_to_u32 ( s, s->_bg_color );
  for ( i= line_size*((size_t) (s->_lines-1)), j= 0;
        j < line_size;
        ++i, ++j )
    fb[i]= color;
  
} // end scroll_low


static void
new_line (
          Screen *s
          )
{
  
  // Si estem en la finestra superior
  if ( s->_current_win == W_UP )
    {
      if ( s->_cursors[W_UP].line < s->_upwin_lines )
        ++(s->_cursors[W_UP].line);
    }

  // Finestra inferior
  else
    {
      if ( s->_cursors[W_LOW].line < (s->_lines-1) )
        ++(s->_cursors[W_LOW].line);
      else scroll_low ( s );
    }
  
} // end new_line


static void
init_split (
            Screen     *s,
            const char *text
            )
{

  size_t tmp,len;

  
  // Reserva memòria si cal.
  tmp= strlen ( text );
  len= tmp + 1;
  if ( tmp > len ) ee ( "init_split - cannot allocate memory" );
  if ( len > s->_split.size )
    {
      s->_split.buf= g_renew ( char, s->_split.buf, len );
      s->_split.size= len;
    }

  // Inicialitza.
  strcpy ( s->_split.buf, text );
  s->_split.p= s->_split.buf;
  s->_split.nl_buffered= false;
  
} // end init_split


// Torna: NULL si no queden, "" si és una nova línia, o text normal.
static char *
split_next (
            Screen *s
            )
{

  char *ret,*p;


  ret= s->_split.p;
  if ( s->_split.nl_buffered )
    {
      s->_split.p= ret+1;
      s->_split.nl_buffered= false;
    }
  else if ( *ret == '\0' )
    ret= NULL;
  else if ( *ret == '\n' )
    {
      s->_split.p= ret+1;
      *ret= '\0';
    }
  else
    {
      for ( p= ret; *p != '\0' && *p != '\n'; ++p );
      if ( *p == '\n' )
        {
          *p= '\0';
          s->_split.nl_buffered= true;
        }
      s->_split.p= p;
    }
  
  return ret;
  
} // end split_next


static bool
resize_cursor (
               ScreenCursor  *c,
               char         **err
               )
{

  size_t nsize;


  nsize= c->size*2;
  if ( nsize < c->size )
    {
      msgerror ( err, "resize_cursor - cannot allocate memory" );
      return false;
    }
  c->text= g_renew ( char, c->text, nsize );
  c->size= nsize;

  return true;
  
} // end resize_cursor


static void
draw_render_buf (
                 Screen    *s,
                 const int  x,
                 const int  y,
                 const int  width
                 )
{
  
  uint32_t *fb;
  int c,r,off;
  const uint8_t *data;
  const SDL_Surface *surface;


  surface= s->_render_buf;
  data= (const uint8_t *) (surface->pixels);
  fb= s->_fb;
  off= y*s->_width + x;
  for ( r= 0; r < surface->h; ++r )
    {
      for ( c= 0; c < width; ++c )
        fb[off + c]= ((const uint32_t *) data)[c];
      off+= s->_width;
      data+= surface->pitch;
    }
  
} // end draw_render_buf


// Fa retrocedir els comptadors N i Nc d'acord amb el text que es
// passa.
static void
rewind_utf8_chars (
                   const char *text,
                   const int   count,
                   size_t     *N,
                   size_t     *Nc
                   )
{

  size_t l_Nc,l_N;

  
  l_N= *N;
  l_Nc= *Nc;
  while ( count < l_Nc )
    {
      if ( (text[--l_N]&0xC0) != 0x80 )
        --l_Nc;
    }
  *N= l_N;
  *Nc= l_Nc;
  
} // end rewind_utf_chars


static bool
rewind_to_space (
                 const char   *text,
                 const size_t  old_N,
                 size_t       *N,
                 size_t       *Nc
                 )
{

  size_t l_Nc,l_N;

  
  l_N= *N;
  l_Nc= *Nc;
  while ( l_N > old_N && text[l_N] != ' ' )
    {
      if ( (text[--l_N]&0xC0) != 0x80 )
        --l_Nc;
    }
  if ( l_N == old_N ) return false;
  *N= l_N;
  *Nc= l_Nc;
  
  return true;
  
} // end rewind_to_space


// L'estil del cursor actual ja està actualitzat. El text pot ser ""
// per a indicar nova línia, o directament una línia de text que no
// inclou \n.
static bool
print_line (
            Screen      *s,
            const char  *text,
            char       **err
            )
{

  ScreenCursor *c;
  const char *p,*remain;
  size_t new_N,new_Nc;
  int extent,count,draw_w;
  SDL_Surface *surface;
  SDL_Color color;
  uint32_t bg_color;
  SDL_Rect rect;
  
  
  // Prepara.
  c= &(s->_cursors[s->_current_win]);
  remain= text;

  // Mentre hi haja text pendent
  while ( remain != NULL )
    {
      
      // Si estem en la upper i baix de tot no fem res
      if ( s->_current_win == W_UP && c->line >= s->_upwin_lines )
        return true;
      
      // Nova línia
      if ( *remain == '\0' )
        {
          c->x= 0;
          *(c->text)= '\0';
          c->N= 0;
          c->Nc= 0;
          c->space= false;
          new_line ( s );
          return true;
        }
      
      // Append chars.
      new_Nc= c->Nc;
      for ( p= remain, new_N= c->N; *p != '\0'; ++p, ++new_N )
        {
          if ( new_N == c->size )
            { if ( !resize_cursor ( c, err ) ) return false; }
          c->text[new_N]= *p;
          if ( ((*p)&0xC0) != 0x80 ) ++new_Nc;
        }
      if ( new_N == c->size ) { if ( !resize_cursor ( c, err ) ) return false; }
      c->text[new_N]= *p;
      
      // Calcula si cap o no, i reajusta.
      if ( TTF_MeasureUTF8 ( s->_fonts->_fonts[c->font][c->style], c->text,
                             s->_width - c->x, &extent, &count ) )
        goto error_render;
      if ( count < new_Nc )
        {
          rewind_utf8_chars ( c->text, count, &new_N, &new_Nc );
          assert ( count == new_Nc );
          if ( c->buffered )
            {
              // Mou fins primer espai que pot ser l'últim impres
              if ( !rewind_to_space ( c->text, c->N, &new_N, &new_Nc ) &&
                   c->space )
                { new_N= c->N; new_Nc= c->Nc; }
            }
          c->text[new_N]= '\0';
          remain+= new_N-c->N;
        }
      else remain= NULL;
      
      // Renderitza
      if ( count > 0 )
        {
          // --> Renderitza text.
          true_color_to_sdlcolor ( c->fg_color, &color );
          surface= TTF_RenderUTF8_Blended
            ( s->_fonts->_fonts[c->font][c->style], c->text, color );
          if ( surface == NULL ) goto error_render;
          // --> Ompli fons color
          bg_color= true_color_to_u32 ( s, c->bg_color );
          draw_w= remain!=NULL ? (s->_width-c->x) : surface->w;
          if ( draw_w > s->_width ) draw_w= s->_width;
          rect.x= 0; rect.w= draw_w;
          rect.y= 0; rect.h= s->_line_height;
          if ( SDL_FillRect ( s->_render_buf, &rect, (Uint32) bg_color ) != 0 )
            goto error_render_sdl;
          // --> Aplica text a surface
          if ( SDL_BlitSurface ( surface, NULL, s->_render_buf, &rect ) != 0 )
            goto error_render_sdl;
          // --> Pinta
          draw_render_buf ( s, c->x, c->line*s->_line_height, draw_w );
          // --> Allibera memòria
          SDL_FreeSurface ( surface );
        }
      
      // Actualitza c
      c->N= new_N;
      c->Nc= new_Nc;
      c->space= (new_N>1 && c->text[new_N-1]==' ');
      
      // Si hi ha remain canvia de línia e imprimeix.
      if ( remain != NULL )
        {
          c->x= 0;
          *(c->text)= '\0';
          c->N= 0;
          c->Nc= 0;
          c->space= false;
          new_line ( s );
        }
      
    }
  
  return true;

 error_render_sdl:
  msgerror ( err, "Failed to render: %s", SDL_GetError () );
  return false;
 error_render:
  msgerror ( err, "Failed to render: %s", c->text );
  return false;
  
} // end print_line




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
screen_free (
             Screen *s
             )
{

  int i;


  if ( s->_render_buf != NULL ) SDL_FreeSurface ( s->_render_buf );
  g_free ( s->_split.buf );
  for ( i= 0; i < 2; ++i )
    if ( s->_cursors[i].text != NULL )
      g_free ( s->_cursors[i].text );
  if ( s->_fb != NULL ) g_free ( s->_fb );
  if ( s->_fonts != NULL ) fonts_free ( s->_fonts );
  if ( s->_win != NULL ) window_free ( s->_win );
  g_free ( s );
  
} // end screen_free


Screen *
screen_new (
            Conf            *conf,
            const int        version,
            const char      *title,
            const gboolean   verbose,
            char           **err
            )
{

  Screen *ret;
  int n;
  uint32_t color;
  

  assert ( version >= 1 && version <= 8 && version != 6 );
  
  // Prepara.
  ret= g_new ( Screen, 1 );
  ret->_conf= conf;
  ret->_win= NULL;
  ret->_fonts= NULL;
  ret->_version= version;
  ret->_fb= NULL;
  for ( n= 0; n < 2; ++n )
    ret->_cursors[n].text= NULL;
  ret->_split.buf= NULL;
  ret->_render_buf= NULL;
  
  // Inicialitza fonts i calcula dimensions pantalla.
  ret->_fonts= fonts_new ( conf, verbose, err );
  if ( ret->_fonts == NULL ) goto error;
  ret->_line_height= fonts_char_height ( ret->_fonts );
  if ( ret->_line_height <= 0 )
    {
      msgerror ( err, "Failed to create screen: invalid font height %d",
                 ret->_line_height );
      goto error;
    }
  if ( !fonts_char0_width ( ret->_fonts, &(ret->_char_width), err ) )
    goto error;
  if ( ret->_char_width <= 0 )
    {
      msgerror ( err, "Failed to create screen: invalid char width %d",
                 ret->_char_width );
      goto error;
    }
  ret->_lines= conf->screen_lines;
  ret->_height= ret->_lines*ret->_line_height;
  if ( ret->_version <= 3 ) ret->_height+= ret->_line_height;
  ret->_width_chars= conf->screen_width;
  ret->_width= ret->_width_chars*ret->_char_width;
  if ( ret->_width <= 0 || ret->_height <= 0 )
    {
      msgerror ( err, "Failed to create screen" );
      goto error;
    }

  // Crea finestra.
  ret->_win= window_new ( ret->_width, ret->_height,
                          ret->_width, ret->_height,
                          title, NULL, err );
  if ( ret->_win == NULL ) goto error;
  window_show ( ret->_win );

  // Inicialitza framebuffer
  ret->_fb= g_new ( uint32_t, ret->_width*ret->_height );
  ret->_bg_color= C_WHITE;
  ret->_fg_color= C_BLACK;
  ret->_reverse_color= false;
  color= true_color_to_u32 ( ret, ret->_bg_color );
  for ( n= 0; n < ret->_width*ret->_height; ++n )
    ret->_fb[n]= color;
  if ( !redraw_fb ( ret, err ) ) goto error;
  
  // Altres.
  ret->_upwin_lines= 0;
  ret->_current_win= W_LOW;
  ret->_current_font= ret->_version<=3 ? F_FPITCH : F_NORMAL;
  ret->_current_style= F_ROMAN;
  
  // Inicialitza els cursors
  for ( n= 0; n < 2; ++n )
    {
      ret->_cursors[n].font= ret->_current_font;
      ret->_cursors[n].style= ret->_current_style;
      ret->_cursors[n].fg_color= C_BLACK;
      ret->_cursors[n].bg_color= C_WHITE;
      ret->_cursors[n].line= 0;
      ret->_cursors[n].x= 0;
      ret->_cursors[n].width= 0;
      ret->_cursors[n].text= g_new ( char, 1 );
      ret->_cursors[n].text[0]= '\0';
      ret->_cursors[n].size= 1;
      ret->_cursors[n].N= 0;
      ret->_cursors[n].Nc= 0;
      ret->_cursors[n].buffered= false;
      ret->_cursors[n].space= false;
    }
  ret->_cursors[W_UP].font= F_FPITCH;
  if ( ret->_version == 4 )
    ret->_cursors[W_LOW].line= ret->_lines-1;

  // Inicialitza el tokenitzador.
  ret->_split.buf= g_new ( char, 1 );
  ret->_split.buf[0]= '\0';
  ret->_split.size= 1;

  // Inicialitza el buffer de renderitzat.
  ret->_render_buf= window_get_surface ( ret->_win, ret->_width,
                                         ret->_line_height, err );
  if ( ret->_render_buf == NULL ) goto error;
  
  return ret;
  
 error:
  screen_free ( ret );
  return NULL;
  
} // end screen_new


bool
screen_print (
              Screen      *s,
              const char  *text,
              char       **err
              )
{

  int font,style;
  uint16_t fg_color,bg_color;
  ScreenCursor *c;
  char *line_text;
  
  
  // Obté estil, color, etc.
  if ( s->_current_win == W_UP ) font= F_FPITCH;
  else font= s->_current_font;
  style= s->_current_style;
  if ( s->_reverse_color ) { fg_color= s->_bg_color; bg_color= s->_fg_color; }
  else                     { fg_color= s->_fg_color; bg_color= s->_bg_color; }
  c= &(s->_cursors[s->_current_win]);
  
  // Adelanta el cursos si canvia l'estil.
  if ( c->bg_color != bg_color || c->fg_color != fg_color ||
       c->font != font || c->style != style )
    {
      c->x+= c->width;
      *(c->text)= '\0';
      c->N= 0;
      c->Nc= 0;
      c->space= false;
      c->bg_color= bg_color;
      c->fg_color= fg_color;
      c->font= font;
      c->style= style;
      if ( c->x >= s->_width ) new_line ( s );
    }
  
  // Tokenitza el text en línies (pel caràcter '\n')
  init_split ( s, text );
  while ( (line_text= split_next ( s )) != NULL )
    if ( !print_line ( s, line_text, err ) )
      return false;

  // Actualitza
  if ( !redraw_fb ( s, err ) ) return false;
  
  return true;
  
} // end screen_print
