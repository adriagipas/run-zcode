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
 *  dictionary.c - Implementació 'dictionary.h'
 *
 */


#include <assert.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dictionary.h"

#include "utils/error.h"
#include "utils/log.h"




/**********/
/* MACROS */
/**********/

#define ZC_NULL  0
#define ZC_SPACE 32




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static bool
token_add (
           Dictionary     *d,
           const uint8_t   val,
           char          **err
           )
{

  size_t nsize;

  
  if ( d->_token.N == d->_token.size )
    {
      nsize= d->_token.size*2;
      if ( nsize < d->_token.size )
        {
          msgerror ( err, "dictionary.c - token_add: "
                     "Failed to allocate memory" );
          return false;
        }
      d->_token.v= g_renew ( uint8_t, d->_token.v, nsize );
      d->_token.size= nsize;
    }
  d->_token.v[d->_token.N++]= val;

  return true;
  
} // end token_add


// a < b  --> -1
// a == b -->  0
// a > b  -->  1
static int
cmp_words (
           const Dictionary *d,
           const uint8_t    *a,
           const uint8_t    *b
           )
{

  uint8_t i;


  for ( i= 0; i < d->_text_length; ++i )
    if ( a[i] < b[i] ) return -1;
    else if ( a[i] > b[i] ) return 1;

  return 0;
  
} // end cmp_words


// token té longitut _text_length. Torna l'adreça o 0 si no es troba.
static uint16_t
search_word (
             const Dictionary *d,
             const uint8_t    *word
             )
{

  const DictionaryEntry *entry;
  int l,m,r,cmp;


  if ( d->_N == 0 ) return 0;

  l= 0; r= ((int) d->_N) - 1;
  while ( l <= r )
    {

      // Valor a comparar.
      if ( l == r ) m= l;
      else m= l + (r-l)/2;
      entry= &(d->_entries[m]);

      // Compara
      cmp= cmp_words ( d, word, entry->bytes );
      if ( cmp == 0 ) return entry->addr;
      else if ( cmp == -1 ) r= m-1;
      else l= m+1;
      
    }

  return 0; // No trobat.
  
} // end search_word


// Torna l'adreça o 0 si no està.
static bool
find_token (
            Dictionary  *d,
            uint16_t    *addr,
            char       **err
            )
{

  uint8_t buf[4*10],buf_enc[4*10],zc; // Espai de sobra. 10 caràcters escapats.
  int N,Nenc;
  size_t i;
  
  
  assert ( d->_token.N > 0 );

  // Si és massa llarg trunca.
  if ( d->_token.N > d->_real_text_length )
    d->_token.N= d->_real_text_length;
  
  // Codifica.
  // NOTA!!! En realitat en aquest punt sols trobarem minúscules.
  N= 0;
  for ( i= 0; i < d->_token.N; ++i )
    {
      zc= d->_token.v[i];
      switch ( d->_zscii2alph[zc].alph )
        {
        case 0:
          buf[N++]= d->_zscii2alph[zc].val;
          break;
        case 1:
          buf[N++]= d->_version<=2 ? 2 : 4;
          buf[N++]= d->_zscii2alph[zc].val;
          break;
        case 2:
          buf[N++]= d->_version<=2 ? 3 : 5;
          buf[N++]= d->_zscii2alph[zc].val;
          break;
        default: // -1, simplement escapem.
          buf[N++]= d->_version<=2 ? 3 : 5;
          buf[N++]= 6;
          buf[N++]= 0x00;
          buf[N++]= zc;
        }
    }

  // Torna a comprovar i a truncar.
  if ( N > d->_real_text_length ) N= d->_real_text_length;

  // Comprimeix.
  // --> Afegeix padding
  while ( N%3 != 0 ) buf[N++]= 5;
  // --> Codifica
  Nenc= 0;
  for ( i= 0; i < N; i+= 3 )
    {
      buf_enc[Nenc++]= (buf[i]<<2) | (buf[i+1]>>3);
      buf_enc[Nenc++]= (buf[i+1]<<5) | (buf[i+2]);
    }
  // --> Comprova longitut
  if ( Nenc > d->_text_length ) { *addr= 0; return true; }
  // --> Padding i símbol final
  while ( Nenc < d->_text_length )
    {
      // Padding 5-5-5
      buf_enc[Nenc++]= 0x14;// (buf[i]<<2) | (buf[i+1]>>3);
      buf_enc[Nenc++]= 0xa5;// (buf[i+1]<<5) | (buf[i+2]);
    }
  buf_enc[Nenc-2]|= 0x80;

  // Cerca
  *addr= search_word ( d, buf_enc );

  // DEBUG
  //for(int j= 0; j < N; ++j)printf("%X ",buf[j]);printf("\n");
  //for(int j= 0; j < Nenc; ++j)printf("%X ",buf_enc[j]);printf("\n");
  
  return true;
  
} // end find_token


// Si el token està buit simplement no fa res.
static bool
parse_token (
             Dictionary      *d,
             const int        pos,
             const uint16_t   parse_buf,
             uint8_t         *cwords,
             char           **err
             )
{

  uint16_t addr;
  uint32_t dst_addr;
  int real_pos;
  
  
  if ( d->_token.N == 0 ) return true;

  // NOTA!! Ignorem paraules de més de 255 lletres.
  if ( d->_token.N > 255 )
    {
      ww ( "Ignoring %d letters long word", d->_token.N );
      return true;
    }
  // NOTA!! Ignorem paraules amb posició superior a 255
  if ( pos > 255 )
    {
      ww ( "Ignoring word at position %d", pos );
      return true;
    }
  
  if ( !find_token ( d, &addr, err ) ) return false;
  dst_addr= ((uint32_t) parse_buf) + 2 + 4*((uint32_t) *cwords);
  // --> ADDR DICT
  if ( !memory_map_WRITEB ( d->_mem, dst_addr,
                            (uint8_t) (addr>>8), true, err ) )
    return false;
  if ( !memory_map_WRITEB ( d->_mem, dst_addr+1,
                            (uint8_t) addr, true, err ) )
    return false;
  // --> LETTERS
  if ( !memory_map_WRITEB ( d->_mem, dst_addr+2,
                            (uint8_t) (d->_token.N), true, err ) )
    return false;
  // --> Position
  real_pos= pos + (d->_version>=5 ? 2 : 1);
  if ( !memory_map_WRITEB ( d->_mem, dst_addr+3,
                            (uint8_t) (real_pos), true, err ) )
    return false;

  // Incrementa nombre paraules
  ++(*cwords);

  // DEBUG
  //printf("TOKEN POS:%d LEN:%d ADDR:%X=>",pos,d->_token.N,addr);
  //for ( size_t i= 0; i < d->_token.N; ++i )
  //  printf(" %d",d->_token.v[i]);
  //printf("\n");
  
  return true;
  
} // end parse_token


static bool
check_is_wsep (
               Dictionary    *d,
               const uint8_t  zc
               )
{

  int n;


  for ( n= 0; n < (int) (d->_N_wseps); ++n )
    if ( d->_wseps[n] == zc )
      return true;

  return false;
  
} // end check_is_wsep


#define SET_ZSCII2ALPH(CHAR,CODE,ALPH)                                  \
  d->_zscii2alph[(CHAR)].val= (CODE); d->_zscii2alph[(CHAR)].alph= (ALPH)

static void
init_zscii2alph_default (
                         Dictionary *d
                         )
{

  int i;
  

  // Valors per defecte.
  for ( i= 0; i < 256; ++i )
    {
      d->_zscii2alph[i].val= 0x00;
      d->_zscii2alph[i].alph= -1;
    }

  // Valors ALPH=0
  SET_ZSCII2ALPH('a',0x06,0);
  SET_ZSCII2ALPH('b',0x07,0);
  SET_ZSCII2ALPH('c',0x08,0);
  SET_ZSCII2ALPH('d',0x09,0);
  SET_ZSCII2ALPH('e',0x0a,0);
  SET_ZSCII2ALPH('f',0x0b,0);
  SET_ZSCII2ALPH('g',0x0c,0);
  SET_ZSCII2ALPH('h',0x0d,0);
  SET_ZSCII2ALPH('i',0x0e,0);
  SET_ZSCII2ALPH('j',0x0f,0);
  SET_ZSCII2ALPH('k',0x10,0);
  SET_ZSCII2ALPH('l',0x11,0);
  SET_ZSCII2ALPH('m',0x12,0);
  SET_ZSCII2ALPH('n',0x13,0);
  SET_ZSCII2ALPH('o',0x14,0);
  SET_ZSCII2ALPH('p',0x15,0);
  SET_ZSCII2ALPH('q',0x16,0);
  SET_ZSCII2ALPH('r',0x17,0);
  SET_ZSCII2ALPH('s',0x18,0);
  SET_ZSCII2ALPH('t',0x19,0);
  SET_ZSCII2ALPH('u',0x1a,0);
  SET_ZSCII2ALPH('v',0x1b,0);
  SET_ZSCII2ALPH('w',0x1c,0);
  SET_ZSCII2ALPH('x',0x1d,0);
  SET_ZSCII2ALPH('y',0x1e,0);
  SET_ZSCII2ALPH('z',0x1f,0);

  // Valors ALPH=1
  SET_ZSCII2ALPH('A',0x06,1);
  SET_ZSCII2ALPH('B',0x07,1);
  SET_ZSCII2ALPH('C',0x08,1);
  SET_ZSCII2ALPH('D',0x09,1);
  SET_ZSCII2ALPH('E',0x0a,1);
  SET_ZSCII2ALPH('F',0x0b,1);
  SET_ZSCII2ALPH('G',0x0c,1);
  SET_ZSCII2ALPH('H',0x0d,1);
  SET_ZSCII2ALPH('I',0x0e,1);
  SET_ZSCII2ALPH('J',0x0f,1);
  SET_ZSCII2ALPH('K',0x10,1);
  SET_ZSCII2ALPH('L',0x11,1);
  SET_ZSCII2ALPH('M',0x12,1);
  SET_ZSCII2ALPH('N',0x13,1);
  SET_ZSCII2ALPH('O',0x14,1);
  SET_ZSCII2ALPH('P',0x15,1);
  SET_ZSCII2ALPH('Q',0x16,1);
  SET_ZSCII2ALPH('R',0x17,1);
  SET_ZSCII2ALPH('S',0x18,1);
  SET_ZSCII2ALPH('T',0x19,1);
  SET_ZSCII2ALPH('U',0x1a,1);
  SET_ZSCII2ALPH('V',0x1b,1);
  SET_ZSCII2ALPH('W',0x1c,1);
  SET_ZSCII2ALPH('X',0x1d,1);
  SET_ZSCII2ALPH('Y',0x1e,1);
  SET_ZSCII2ALPH('Z',0x1f,1);

  // Valors ALPH=2
  if ( d->_version == 1 )
    {
      SET_ZSCII2ALPH('0',0x07,2);
      SET_ZSCII2ALPH('1',0x08,2);
      SET_ZSCII2ALPH('2',0x09,2);
      SET_ZSCII2ALPH('3',0x0a,2);
      SET_ZSCII2ALPH('4',0x0b,2);
      SET_ZSCII2ALPH('5',0x0c,2);
      SET_ZSCII2ALPH('6',0x0d,2);
      SET_ZSCII2ALPH('7',0x0e,2);
      SET_ZSCII2ALPH('8',0x0f,2);
      SET_ZSCII2ALPH('9',0x10,2);
      SET_ZSCII2ALPH('.',0x11,2);
      SET_ZSCII2ALPH(',',0x12,2);
      SET_ZSCII2ALPH('!',0x13,2);
      SET_ZSCII2ALPH('?',0x14,2);
      SET_ZSCII2ALPH('_',0x15,2);
      SET_ZSCII2ALPH('#',0x16,2);
      SET_ZSCII2ALPH('\'',0x17,2);
      SET_ZSCII2ALPH('"',0x18,2);
      SET_ZSCII2ALPH('/',0x19,2);
      SET_ZSCII2ALPH('\\',0x1a,2);
      SET_ZSCII2ALPH('<',0x1b,2);
      SET_ZSCII2ALPH('-',0x1c,2);
      SET_ZSCII2ALPH(':',0x1d,2);
      SET_ZSCII2ALPH('(',0x1e,2);
      SET_ZSCII2ALPH(')',0x1f,2);
    }
  else
    {
      SET_ZSCII2ALPH(13,0x07,2);
      SET_ZSCII2ALPH('0',0x08,2);
      SET_ZSCII2ALPH('1',0x09,2);
      SET_ZSCII2ALPH('2',0x0a,2);
      SET_ZSCII2ALPH('3',0x0b,2);
      SET_ZSCII2ALPH('4',0x0c,2);
      SET_ZSCII2ALPH('5',0x0d,2);
      SET_ZSCII2ALPH('6',0x0e,2);
      SET_ZSCII2ALPH('7',0x0f,2);
      SET_ZSCII2ALPH('8',0x10,2);
      SET_ZSCII2ALPH('9',0x11,2);
      SET_ZSCII2ALPH('.',0x12,2);
      SET_ZSCII2ALPH(',',0x13,2);
      SET_ZSCII2ALPH('!',0x14,2);
      SET_ZSCII2ALPH('?',0x15,2);
      SET_ZSCII2ALPH('_',0x16,2);
      SET_ZSCII2ALPH('#',0x17,2);
      SET_ZSCII2ALPH('\'',0x18,2);
      SET_ZSCII2ALPH('"',0x19,2);
      SET_ZSCII2ALPH('/',0x1a,2);
      SET_ZSCII2ALPH('\\',0x1b,2);
      SET_ZSCII2ALPH('-',0x1c,2);
      SET_ZSCII2ALPH(':',0x1d,2);
      SET_ZSCII2ALPH('(',0x1e,2);
      SET_ZSCII2ALPH(')',0x1f,2);
    }
  
} // end init_zscii2alph_default


static bool
init_zscii2alph (
                 Dictionary  *d,
                 uint32_t     addr,
                 char       **err
                 )
{

  int i,j;
  uint8_t zc;
  

  // Valors per defecte.
  for ( i= 0; i < 256; ++i )
    {
      d->_zscii2alph[i].val= 0x00;
      d->_zscii2alph[i].alph= -1;
    }

  // Llig la taula
  for ( i= 0; i < 3; ++i )
    for ( j= 0; j < 26; ++j, ++addr )
      {
        if ( !memory_map_READB ( d->_mem, addr, &zc, false, err ) )
          return false;
        if ( i != 2 || (j != 6 && j != 7) )
          {
            d->_zscii2alph[zc].val= j+6;
            d->_zscii2alph[zc].alph= i;
          }
      }
  // Newline
  d->_zscii2alph[13].val= 0x07;
  d->_zscii2alph[13].alph= 2;

  return true;
  
} // end init_zscii2alph




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
dictionary_free (
                 Dictionary *d
                 )
{

  g_free ( d->_token.v );
  g_free ( d->_entries );
  g_free ( d );
  
} // end dictionary_free


Dictionary *
dictionary_new (
                MemoryMap  *mem,
                char      **err
                )
{

  Dictionary *ret;
  uint32_t alphabet_table_addr;
  

  // Prepara.
  ret= g_new ( Dictionary, 1 );
  ret->_mem= mem;
  ret->_entries= g_new ( DictionaryEntry, 1 );
  ret->_size= 1;
  ret->_N= 0;
  ret->_N_wseps= 0;
  ret->_token.v= g_new ( uint8_t, 1 );
  ret->_token.size= 1;
  ret->_token.N= 0;
  ret->_version= mem->sf_mem[0];
  if ( ret->_version >= 5 )
    {
      alphabet_table_addr=
        (((uint32_t) ret->_mem->sf_mem[0x34])<<8) |
        ((uint32_t) ret->_mem->sf_mem[0x35])
        ;
    }
  else alphabet_table_addr= 0;

  // Inicialitza zscii2alpha
  if ( alphabet_table_addr == 0 )
    init_zscii2alph_default ( ret );
  else
    {
      if ( !init_zscii2alph ( ret, alphabet_table_addr, err ) )
        goto error;
    }
  
  return ret;

 error:
  dictionary_free ( ret );
  return NULL;
  
} // end dictionary_new


bool
dictionary_load (
                 Dictionary      *d,
                 const uint32_t   addr,
                 char           **err
                 )
{

  int n,i;
  uint8_t aux,entry_length;
  uint32_t raddr;
  DictionaryEntry *entry;
  
  
  // Prepara
  if ( d->_version <= 3 )
    {
      d->_text_length= 4;
      d->_real_text_length= 6;
    }
  else
    {
      d->_text_length= 6;
      d->_real_text_length= 9;
    }
  raddr= addr;
  
  // Word separators
  if ( !memory_map_READB ( d->_mem, raddr, &(d->_N_wseps), false, err ) )
    return false;
  ++raddr;
  for ( n= 0; n < (int) (d->_N_wseps); ++n )
    {
      if ( !memory_map_READB ( d->_mem, raddr, &aux, false, err ) )
        return false;
      d->_wseps[n]= aux;
      ++raddr;
    }

  // Entry length
  if ( !memory_map_READB ( d->_mem, raddr, &entry_length, false, err ) )
    return false;
  ++raddr;
  if ( entry_length <= d->_text_length )
    {
      msgerror ( err, "Failed to load dictionary from address %X:"
                 " entry length too short %u", addr, entry_length );
      return false;
    }
  
  // Number entries
  if ( !memory_map_READW ( d->_mem, raddr, &(d->_N), false, err ) )
    return false;
  raddr+= 2;
  if ( d->_N > d->_size )
    {
      d->_size= d->_N;
      d->_entries= g_renew ( DictionaryEntry, d->_entries, d->_size );
    }
  
  // Entries
  for ( n= 0; n < (int) (d->_N); ++n )
    {
      entry= &(d->_entries[n]);
      if ( raddr > 0xFFFF )
        {
          msgerror ( err, "Failed to load dictionary from address %X:"
                     " Entry %d is located in out of range address %X",
                     addr, n, raddr );
          return false;
        }
      entry->addr= (uint16_t) raddr;
      for ( i= 0; i < (int) (d->_text_length); ++i )
        {
          if ( !memory_map_READB ( d->_mem, raddr, &(entry->bytes[i]),
                                   false, err ) )
            return false;
          ++raddr;
        }
      raddr+= (uint32_t) (entry_length-d->_text_length);
    }
  
  return true;
  
} // end dictionary_load


bool
dictionary_parse (
                  Dictionary  *d,
                  uint16_t     text_buf,
                  uint16_t     parse_buf,
                  char       **err
                  )
{

  int nchars,i,wpos; // 0 indica que s'acaba amb ZC_NULL
  uint16_t caddr,p;
  uint8_t tmp_u8,zc,max_words,cwords;
  bool stop;
  
  
  // Preparació.
  if ( d->_version <= 4 )
    {
      nchars= -1;
      caddr= text_buf+1;
    }
  else
    {
      if ( !memory_map_READB ( d->_mem, text_buf+1, &tmp_u8, true, err ) )
        return false;
      nchars= (int) tmp_u8;
      caddr= text_buf+2;
    }
  if ( !memory_map_READB ( d->_mem, parse_buf, &max_words, true, err ) )
    return false;
  
  // Separa en paraules
  d->_token.N= 0;
  for ( i= 0, stop= false, p= caddr, wpos= 0, cwords= 0;
        ((nchars != -1 && i < nchars) || (nchars == -1 && !stop))
          && cwords < max_words;
        ++i, ++p )
    {
      if ( !memory_map_READB ( d->_mem, p, &zc, true, err ) )
        return false;
      if ( nchars == 0 && zc == ZC_NULL ) stop= true;
      else if ( zc == ZC_SPACE )
        {
          if ( !parse_token ( d, wpos, parse_buf, &cwords, err ) )
            return false;
          d->_token.N= 0;
          wpos= i+1;
        }
      else if ( check_is_wsep ( d, zc ) )
        {
          // Token anterior si hi ha
          if ( !parse_token ( d, wpos, parse_buf, &cwords, err ) )
            return false;
          d->_token.N= 0;
          // Token que sols conté el separador
          wpos= i;
          if ( !token_add ( d, zc, err ) ) return false;
          if ( !parse_token ( d, wpos, parse_buf, &cwords, err ) )
            return false;
          // Prepara pròxim
          d->_token.N= 0;
          wpos= i+1;
        }
      else
        {
          if ( !token_add ( d, zc, err ) ) return false;
        }
    }
  if ( cwords < max_words )
    if ( !parse_token ( d, wpos, parse_buf, &cwords, err ) ) return false;

  // Desa paraules escrites.
  if ( !memory_map_WRITEB ( d->_mem, parse_buf+1, cwords, true, err ) )
    return false;
  
  return true;
  
} // end dictionary_parse
