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

#define REPAINT_TICKS 20 // 50 FPS




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

  bool ret;
  Uint32 t;
  
  
  t= SDL_GetTicks ();
  if ( t < s->_last_redraw_t || (t-s->_last_redraw_t) >= REPAINT_TICKS )
    {
      ret= window_update ( s->_win, s->_fb, err );
      s->_last_redraw_t= t;
    }
  else ret= true;
  
  return ret;
  
} // end redraw_fb


static void
scroll_low (
            Screen *s
            )
{

  uint32_t *fb;
  uint32_t color;
  size_t i,j,line_size,end;

  
  fb= s->_fb;

  assert ( s->_upwin_lines < s->_lines );
  
  // Cópia amunt
  line_size= ((size_t) s->_line_height)*((size_t) s->_width);
  end= ((size_t) ((s->_lines-1))) * line_size;
  for ( i= line_size*((size_t) s->_upwin_lines), j= i+line_size;
        i < end;
        ++i, ++j )
    fb[i]= fb[j];

  // Última línia
  color= true_color_to_u32 ( s, s->_cursors[W_LOW].set_bg_color );
  for ( i= end, j= 0; j < line_size; ++i, ++j )
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


static bool
resize_cursor_remain (
                      ScreenCursor  *c,
                      char         **err
                      )
{

  size_t nsize;


  nsize= c->size_remain*2;
  if ( nsize < c->size_remain )
    {
      msgerror ( err, "resize_cursor_remain - cannot allocate memory" );
      return false;
    }
  c->text_remain= g_renew ( char, c->text_remain, nsize );
  c->size_remain= nsize;

  return true;
  
} // end resize_cursor_remain


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


static bool
copy_remain (
             ScreenCursor  *c,
             const size_t   pos,
             char         **err
             )
{

  size_t N;
  const char *p;


  for ( p= &(c->text[pos]), N= 0; *p != '\0'; ++p, ++N )
    {
      if ( N == c->size_remain )
        { if ( !resize_cursor_remain ( c, err ) ) return false; }
      c->text_remain[N]= *p;
    }
  if ( N == c->size_remain )
    { if ( !resize_cursor_remain ( c, err ) ) return false; }
  c->text_remain[N]= '\0';

  return true;
  
} // end copy_remain


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
  int extent,count,draw_w,new_width;
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
          c->width= 0;
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
      // NOTA!!!! TTF_MeasureUTF8 a vegades es ratlla i vols
      // desdibuixar més caràcters dels que estem pintant ara (és a
      // dir, coses que ha pintat ara diuen que no caben), per tal
      // d'intentar solucionar açò vaig a forçar que count no supere
      // mai a la longitut del text nou, independentment que amb
      // buffering calga rebobinar més)
      // NOTA!! MOLT ESTRANY, LA FULLA NO ACABA DE SOLUCIONAR-HO
      if ( count < new_Nc )
        {
          if ( count < c->Nc ) count= c->Nc; // <-- FULLA
          rewind_utf8_chars ( c->text, count, &new_N, &new_Nc );
          assert ( count == new_Nc );
          if ( c->buffered )
            {
              // Mou fins primer espai que pot ser l'últim impres
              if ( !rewind_to_space ( c->text, c->N, &new_N, &new_Nc ) &&
                   c->space )
                { new_N= c->N; new_Nc= c->Nc; }
            }
          if ( !copy_remain ( c, new_N, err ) ) return false;
          c->text[new_N]= '\0';
          remain= c->text_remain;
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
          new_width= draw_w;
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
      else new_width= draw_w;

      // Actualitza c
      c->N= new_N;
      c->Nc= new_Nc;
      c->width= new_width;
      c->space= (new_N>1 && c->text[new_N-1]==' ');
      
      // Si hi ha remain canvia de línia e imprimeix.
      if ( remain != NULL )
        {
          c->x= 0;
          c->width= 0;
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


static bool
add_nontext_zscii (
                   const SDL_Event *event,
                   uint8_t          buf[SCREEN_INPUT_TEXT_BUF],
                   int             *N
                   )
{

  bool ret;

  
  if ( *N == SCREEN_INPUT_TEXT_BUF ) return false;
  
  // FALTA GESTIONAR KEYPAD!!!! EL PROBLEMA QUE TINC ÉS QUE NO SÉ
  // DISTINGIR KEYPAD DE TEXTINPUT EVENT, ÉS A DIR, UN KEYPAD NUMERIC
  // ES CODIFICA SEMPRE COM ASCII. PERÒ CREC QUE NO IMPORTA PERQUÈ AL
  // FINAL NO CREC QUE NINGUN JOC EXIGISCA DISTINGIR AIXÒ.
  ret= true;
  switch ( event->key.keysym.sym )
    {
    case SDLK_BACKSPACE: buf[(*N)++]= 8; break;
    case SDLK_RETURN:    buf[(*N)++]= 13; break;
    case SDLK_ESCAPE:    buf[(*N)++]= 27; break;
    case SDLK_UP:        buf[(*N)++]= 129; break;
    case SDLK_DOWN:      buf[(*N)++]= 130; break;
    case SDLK_LEFT:      buf[(*N)++]= 131; break;
    case SDLK_RIGHT:     buf[(*N)++]= 132; break;
    case SDLK_F1:        buf[(*N)++]= 133; break;
    case SDLK_F2:        buf[(*N)++]= 134; break;
    case SDLK_F3:        buf[(*N)++]= 135; break;
    case SDLK_F4:        buf[(*N)++]= 136; break;
    case SDLK_F5:        buf[(*N)++]= 137; break;
    case SDLK_F6:        buf[(*N)++]= 138; break;
    case SDLK_F7:        buf[(*N)++]= 139; break;
    case SDLK_F8:        buf[(*N)++]= 140; break;
    case SDLK_F9:        buf[(*N)++]= 141; break;
    case SDLK_F10:       buf[(*N)++]= 142; break;
    case SDLK_F11:       buf[(*N)++]= 143; break;
    case SDLK_F12:       buf[(*N)++]= 144; break;
    default: ret= false; break;
      
    }

  return ret;
  
} // end add_nontext_zscii


static void
text_input2zscii (
                  const char *text,
                  uint8_t     buf[SCREEN_INPUT_TEXT_BUF],
                  int        *N
                  )
{

  const char *p;


  for ( p= text; *p != '\0' && *N < SCREEN_INPUT_TEXT_BUF; ++p )
    {
      // Standard ascii
      if ( *p >= 32 && *p <= 126 ) buf[(*N)++]= *p;
    }
  
} // end text_input2zscii


static void
set_colour (
            Screen         *screen,
            const uint16_t  colour,
            uint16_t       *dst,
            const uint16_t  default_colour
            )
{

  // Pixel under the cursor
  if ( colour == 0xFFFD && screen->_version == 6 )
    ww ( "True colour -3 for V6 not supported" );
  // Transparent
  else if ( colour == 0xFFFC && screen->_version == 6 )
    ww ( "True colour -4 for V6 not supported" );
  // Default
  else if ( colour == 0xFFFF ) *dst= default_colour;
  // Regular
  else if ( (colour&0x8000) == 0 ) *dst= colour;
  // Unknown
  else if ( colour != 0xFFFE ) // Current
    ww ( "Unsupported true colour %X", colour );
  
} // end set_colour


static void
reset_cursor (
              Screen    *s,
              const int  window
              )
{

  if ( window == W_LOW )
    s->_cursors[W_LOW].line= (s->_version==4) ? s->_lines-1 : s->_upwin_lines;
  else
    s->_cursors[W_UP].line= 0;
  s->_cursors[window].x= 0;
  s->_cursors[window].text[0]= '\0';
  s->_cursors[window].width= 0;
  s->_cursors[window].N= 0;
  s->_cursors[window].Nc= 0;
  
} // end reset_cursor


static void
unsplit_window (
                Screen *s
                )
{

  reset_cursor ( s, W_UP );
  s->_upwin_lines= 0;
  s->_current_win= W_LOW;
  
} // end unsplit_window


// window - W_UP/W_LOW
static void
erase_window (
              Screen    *s,
              const int  window
              )
{

  int beg,end,r;
  uint32_t color;
  uint32_t *fb;
  size_t i,j,line_size;
  
  
  // Prepara i mou cursor
  if ( window == W_UP )
    {
      beg= 0; end= s->_upwin_lines;
      s->_cursors[W_UP].line= 0;
    }
  else
    {
      beg= s->_upwin_lines; end= s->_lines;
      s->_cursors[W_LOW].line= (s->_version == 4) ? s->_lines-1 : beg;
    }
  s->_cursors[window].x= 0;
  s->_cursors[window].text[0]= '\0';
  s->_cursors[window].width= 0;
  s->_cursors[window].N= 0;
  s->_cursors[window].Nc= 0;

  // Neteja
  fb= s->_fb;
  line_size= ((size_t) s->_line_height)*((size_t) s->_width);
  color= true_color_to_u32 ( s, s->_cursors[window].set_bg_color );
  for ( i= beg*line_size, r= beg; r != end; ++r )
    for ( j= 0; j < line_size; ++j, ++i )
      fb[i]= color;
  
} // end erase_window




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
screen_free (
             Screen *s
             )
{

  int i;


  SDL_StopTextInput ();
  if ( s->_undo.cursor.text != NULL ) g_free ( s->_undo.cursor.text );
  if ( s->_undo.fb != NULL ) g_free ( s->_undo.fb );
  if ( s->_render_buf != NULL ) SDL_FreeSurface ( s->_render_buf );
  g_free ( s->_split.buf );
  for ( i= 0; i < 2; ++i )
    {
      if ( s->_cursors[i].text != NULL )
        g_free ( s->_cursors[i].text );
      if ( s->_cursors[i].text_remain != NULL )
        g_free ( s->_cursors[i].text_remain );
    }
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
    {
      ret->_cursors[n].text= NULL;
      ret->_cursors[n].text_remain= NULL;
    }
  ret->_split.buf= NULL;
  ret->_render_buf= NULL;
  ret->_undo.fb= NULL;
  ret->_undo.cursor.text= NULL;
  
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
  ret->_reverse_color= false;
  color= true_color_to_u32 ( ret, C_WHITE );
  for ( n= 0; n < ret->_width*ret->_height; ++n )
    ret->_fb[n]= color;
  ret->_last_redraw_t= (Uint32) -1;
  if ( !redraw_fb ( ret, err ) ) goto error;
  
  // Altres.
  ret->_upwin_lines= 0;
  ret->_current_win= W_LOW;
  ret->_current_font= ret->_version<=4 ? F_FPITCH : F_NORMAL;
  ret->_current_style= F_ROMAN;
  ret->_current_style_val= 0x00;
  ret->_current_font_val= 1;
  
  // Inicialitza els cursors
  for ( n= 0; n < 2; ++n )
    {
      ret->_cursors[n].font= ret->_current_font;
      ret->_cursors[n].style= ret->_current_style;
      ret->_cursors[n].fg_color= C_BLACK;
      ret->_cursors[n].bg_color= C_WHITE;
      ret->_cursors[n].set_fg_color= C_BLACK;
      ret->_cursors[n].set_bg_color= C_WHITE;
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
      ret->_cursors[n].text_remain= g_new ( char, 1 );
      ret->_cursors[n].size_remain= 1;
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

  // Undo.
  ret->_undo.cursor.text= g_new ( char, 1 );
  ret->_undo.cursor.size= 1;
  ret->_undo.fb= g_new ( uint32_t, ret->_width*ret->_height );
  
  // Altres
  SDL_StartTextInput ();
  
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
  c= &(s->_cursors[s->_current_win]);
  if ( s->_reverse_color )
    { fg_color= c->set_bg_color; bg_color= c->set_fg_color; }
  else
    { fg_color= c->set_fg_color; bg_color= c->set_bg_color; }
  
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


void
screen_set_style (
                  Screen         *screen,
                  const uint16_t  style
                  )
{

  // NOTA!! No he de preocupar-me de la font de la finestra UP.
  screen->_current_style_val= style;
  screen->_reverse_color= (style&0x1)!=0;
  if ( (style&0x08) == 0 && screen->_version >= 5 )
    {
      switch ( screen->_current_font_val )
        {
        case 1: screen->_current_font= F_NORMAL; break;
        case 2:
          screen->_current_font= F_NORMAL;
          ww ( "Picture font not implemented" );
          break;
        case 3:
          screen->_current_font= F_NORMAL;
          ww ( "Character graphics font not implemented" );
          break;
        case 4: screen->_current_font= F_FPITCH; break;
        }
    }
  else screen->_current_font= F_FPITCH;
  switch ( (style>>1)&0x3 )
    {
    case 0: screen->_current_style= F_ROMAN; break;
    case 1: screen->_current_style= F_BOLD; break;
    case 2: screen->_current_style= F_ITALIC; break;
    case 3: screen->_current_style= F_BOLD_ITALIC; break;
    }
  
} // end screen_set_style


void
screen_set_colour (
                   Screen         *screen,
                   const uint16_t  fg,
                   const uint16_t  bg
                   )
{

  int i;

  
  // NOTA!! Tot pareix apuntar a que afecta a les dos finestres.
  for ( i= 0; i < 2; ++i )
    {
      set_colour ( screen, fg, &screen->_cursors[i].set_fg_color, C_BLACK );
      set_colour ( screen, bg, &screen->_cursors[i].set_bg_color, C_WHITE );
    }
  
} // end screen_set_colour


bool
screen_read_char (
                  Screen   *screen,
                  uint8_t   buf[SCREEN_INPUT_TEXT_BUF],
                  int      *N,
                  char    **err
                  )
{

  SDL_Event e;

  
  // Repinta si cal.
  if ( !redraw_fb ( screen, err ) ) return false;
  
  // Intenta llegit caràcter i aprofita per a gestionar events interns
  // de la interfície.
  *N= 0;
  while ( window_next_event ( screen->_win, &e ) )
    switch ( e.type )
      {
      case SDL_KEYDOWN:
        if ( add_nontext_zscii ( &e, buf, N ) )
          return true;
        break;
      case SDL_TEXTINPUT:
        if ( *e.text.text != '\0' )
          {
            text_input2zscii ( e.text.text, buf, N );
            return true;
          }
        break;
      }
  
  return true;
  
} // end screen_read_char


void
screen_set_undo_mark (
                      Screen *screen
                      )
{

  ScreenCursor *c;

  
  c= &(screen->_cursors[screen->_current_win]);
  if ( c->size > screen->_undo.cursor.size )
    {
      screen->_undo.cursor.text=
        g_renew ( char, screen->_undo.cursor.text, c->size );
      screen->_undo.cursor.size= c->size;
    }
  memcpy ( screen->_undo.fb, screen->_fb,
           screen->_width*screen->_height*sizeof(uint32_t) );
  strcpy ( screen->_undo.cursor.text, c->text );
  screen->_undo.cursor.line= c->line;
  screen->_undo.cursor.x= c->x;
  screen->_undo.cursor.width= c->width;
  screen->_undo.cursor.N= c->N;
  screen->_undo.cursor.Nc= c->Nc;
  screen->_undo.cursor.space= c->space;
  
} // end screen_set_undo_mark


void
screen_undo (
             Screen *screen
             )
{

  ScreenCursor *c;

  
  c= &(screen->_cursors[screen->_current_win]);
  memcpy ( screen->_fb, screen->_undo.fb,
           screen->_width*screen->_height*sizeof(uint32_t) );
  strcpy ( c->text, screen->_undo.cursor.text );
  c->line= screen->_undo.cursor.line;
  c->x= screen->_undo.cursor.x;
  c->width= screen->_undo.cursor.width;
  c->N= screen->_undo.cursor.N;
  c->Nc= screen->_undo.cursor.Nc;
  c->space= screen->_undo.cursor.space;
  
} // end screen_undo


bool
screen_erase_window (
                     Screen     *screen,
                     const int   window,
                     char      **err
                     )
{

  // Neteja
  if ( window == -2 ) // Neteja les dos finestres
    {
      erase_window ( screen, W_UP );
      erase_window ( screen, W_LOW );
    }
  else if ( window == -1 ) // Unsplit i neteja
    {
      unsplit_window ( screen );
      erase_window ( screen, W_LOW );
    }
  else if ( window == W_UP || window == W_LOW )
    erase_window ( screen, window );
  else
    ww ( "Cannot erase window %d because it not exist", window );

  // Redibuixa
  if ( !redraw_fb ( screen, err ) ) return false;

  return true;
  
} // end screen_erase_window


bool
screen_split_window (
                     Screen     *screen,
                     const int   lines,
                     char      **err
                     )
{

  // Teòricament sols es pot splitejar si window == W_LOW, però com en
  // realitat no està definit què fer quan window == W_UP i la
  // documentació continua fer el mateix vaig a ignorar-ho.

  // Cas especial on fa un unsplit
  if ( lines == 0 )
    {
      unsplit_window ( screen );
      return true;
    }
  
  // Spliteja
  if ( lines < 0 || lines-1 > screen->_lines )
    {
      msgerror ( err,
                 "Failed to split window: wrong number of lines (%d)"
                 " for upper window", lines );
      return false;
    }
  screen->_upwin_lines= lines;

  // Mou cursors si cal
  if ( screen->_cursors[W_UP].line >= screen->_upwin_lines )
    reset_cursor ( screen, W_UP );
  if ( screen->_cursors[W_LOW].line < screen->_upwin_lines )
    reset_cursor ( screen, W_LOW );
  
  // Si estem en la versió 3 erase_window up
  if ( screen->_version == 3 ) erase_window ( screen, W_UP );
  
  return true;
  
} // end screen_split_window


void
screen_set_buffered (
                     Screen     *screen,
                     const bool  value
                     )
{
  screen->_cursors[W_LOW].buffered= value;
} // end screen_set_buffered


bool
screen_set_window (
                   Screen     *screen,
                   const int   window,
                   char      **err
                   )
{

  if ( window == W_UP || window == W_LOW )
    screen->_current_win= window;
  else
    {
      msgerror ( err, "Failed to execute set_window: unknown window %d",
                 window );
      return false;
    }
  
  return true;
  
} // end screen_set_window


bool
screen_set_cursor (
                   Screen     *screen,
                   const int   x,
                   const int   y,
                   char      **err
                   )
{

  // Comprovacions
  if ( screen->_current_win == W_LOW )
    {
      msgerror ( err, "Failed to execute set_cursor: lower window"
                 " does not support this function" );
      return false;
    }
  if ( x < 1 || x > screen->_width_chars ||
       y < 1 || y > screen->_upwin_lines )
    {
      msgerror ( err,
                 "Failed to execute set_cursor: invalid position (%d,%d)",
                 x, y );
      return false;
    }
  
  // Fixa cursor.
  reset_cursor ( screen, W_UP );
  screen->_cursors[W_UP].x= (x-1)*screen->_char_width;
  screen->_cursors[W_UP].line= y-1;
  
  return true;
  
} // end screen_set_cursor
