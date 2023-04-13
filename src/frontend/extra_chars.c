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
 *  extra_chars.c - Implementació de 'extra_chars.h'.
 *
 */


#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extra_chars.h"
#include "utils/error.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
cmpentries (
            const void *p1,
            const void *p2
            )
{

  uint16_t val1,val2;
  int ret;
  

  val1= ((const ExtraCharsEntry *) p1)->unicode;
  val2= ((const ExtraCharsEntry *) p2)->unicode;
  if ( val1 < val2 )       ret= -1;
  else if ( val1 == val2 ) ret= 0;
  else                     ret= 1;

  return ret;
  
} // end cmpentries


static uint8_t
find_char (
           ExtraChars     *ec,
           const uint32_t  unicode_val
           )
{

  uint16_t val,m_val;
  int l,m,r;
  
  
  // Comprovacions bàsiques.
  if ( unicode_val == 0 || unicode_val > 0xFFFF || ec->_N == 0 )
    return 0;
  val= (uint16_t) unicode_val;

  // Ordena si no ho està.
  if ( !ec->_sorted )
    {
      ec->_sorted= true;
      qsort ( ec->_v, ec->_N, sizeof(ExtraCharsEntry), cmpentries );
    }

  // Cerca binària.
  l= 0; r= ec->_N-1;
  while ( l <= r )
    {
      m= l + (r-l+1)/2;
      m_val= ec->_v[m].unicode;
      if ( val == m_val ) return ec->_v[m].zcode;
      else if ( val < m_val ) r= m-1;
      else                    l= m+1;
    }
  
  return 0;
  
} // end find_char




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
extra_chars_free (
                  ExtraChars *ec
                  )
{

  g_free ( ec->_v );
  g_free ( ec );
  
} // end extra_chars_free


ExtraChars *
extra_chars_new (void)
{

  ExtraChars *ret;


  ret= g_new ( ExtraChars, 1 );
  ret->_sorted= false;
  ret->_size= 1;
  ret->_v= g_new ( ExtraCharsEntry, 1 );
  ret->_N= 0;

  return ret;
  
} // end extra_chars_new


bool
extra_chars_add (
                 ExtraChars      *ec,
                 const uint16_t   unicode,
                 const uint8_t    zcode,
                 char           **err
                 )
{

  int nsize;

  
  if ( ec->_N == ec->_size )
    {
      nsize= ec->_size*2;
      if ( nsize <= ec->_size )
        {
          msgerror ( err,
                     "Failed to register an additional extra character:"
                     " cannot allocate memory" );
          return false;
        }
      ec->_size= nsize;
      ec->_v= g_renew ( ExtraCharsEntry, ec->_v, nsize );
    }
  ec->_v[ec->_N].unicode= unicode;
  ec->_v[ec->_N].zcode= zcode;
  ++(ec->_N);
  ec->_sorted= false;
  
  return true;
  
} // end extra_chars_add


uint8_t
extra_chars_decode_next_char (
                              ExtraChars  *ec,
                              const char  *text,
                              int         *end_pos
                              )
{

  const char *p;
  bool stop;
  uint8_t val,ret;
  uint32_t unicode_val;
  int unicode_count;
  
  
  // Obté següent caràcter.
  unicode_count= 0;
  unicode_val= 0; // Per defecte nul
  for ( p= text, stop= false; *p != '\0' && !stop; ++p )
    {
      val= (uint8_t) *p;
      if ( val < 0x80 )
        {
          if ( unicode_count == 0 )
            unicode_val= (uint32_t) val;
          stop= false;
        }
      else if ( (val&0xf8) == 0xf0 )
        {
          if ( unicode_count == 0 )
            {
              unicode_count= 3;
              unicode_val= (uint32_t) (val&0x7);
            }
          else stop= false;
        }
      else if ( (val&0xf0) == 0xe0 )
        {
          if ( unicode_count == 0 )
            {
              unicode_count= 2;
              unicode_val= (uint32_t) (val&0xf);
            }
          else stop= false;
        }
      else if ( (val&0xe0) == 0xc0 )
        {
          if ( unicode_count == 0 )
            {
              unicode_count= 1;
              unicode_val= (uint32_t) (val&0x1f);
            }
          else stop= false;
        }
      else if ( (val&0xc0) == 0x80 )
        {
          if ( unicode_count > 0 )
            {
              unicode_val<<= 6;
              unicode_val|= (uint8_t) (val&0x3f);
              if ( --unicode_count == 0 ) stop= false;
            }
          else stop= false;
        }
      else stop= false; // Pot passar ?????
    }
  *end_pos= (int) (p-text);

  // Retorna valor
  ret= find_char ( ec, unicode_val );
  
  return ret;
  
} // end extra_chars_decode_next_char
