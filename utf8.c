/*
 * $Id$
 *
 * Copyright (c) 2006-2010 Oskari Saarenmaa <oskari@saarenmaa.fi>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include "utf8.h"

// decodes a single utf8 character into a single latin9 byte (or '?')
// returns the number of bytes decoded
int fg_utf8_decode_char(const unsigned char *str, size_t len, unsigned char *out)
{
  unsigned int c = (unsigned int) (str[0]), b = 1;

  if (c >= 0xf0) /* four-byte utf8 character */
    b = 4;
  else if (c >= 0xe0) /* three-byte utf8 character */
    b = 3;
  else if (c >= 0xc0) /* two-byte utf8 character */
    b = 2;
  else /* one-byte utf8 character, direct mapping to iso-8859-1 */
    {
      *out = *str;
      return 1;
    }

  if (b > len) /* buffer underrun */
    {
      *out = '?';
      return len;
    }

  c = (str[b-1] & 63) | ((str[b-2] & 63) << 6);
  if (b > 2)
    c |= (str[b-3] & 63) << 12;
  if (b > 3)
    c |= (str[b-4] & 63) << 18;

  if (c <= 0xff)                 /* 8 bit character */
    *out = (unsigned char) c;    /* direct mapping */
  else if (c == 0x220ac)         /* utf8 euro (U+20AC) */
    *out = (unsigned char) 0xa4; /* latin9 euro */
  else                           /* character does not fit in iso-8859-1 space */
    *out = (unsigned char) '?';  /* everything else becomes a question mark */

  return b;
}

unsigned char *fg_utf8_decode(const unsigned char *str, size_t *szp)
{
  size_t sz = *szp;
  unsigned char *out = malloc(sz+1), *op = out;
  unsigned int x;

  for (x=0; x<sz;)
    {
      x += fg_utf8_decode_char(str + x, sz - x, op);
      op += 1;
    }

  *op = '\0';
  *szp = op-out;

  return out;
}

// encode a single character, out must be at least 4 bytes long.
// returns the number of bytes written.
int fg_utf8_encode_char(unsigned char c, unsigned char *out)
{
  unsigned char *op = out;
  if (c == 0xa4) /* latin9 euro - not direct mapped */
    {
      *op++ = 0xe2; /* hardcoded utf8 encoding for euro */
      *op++ = 0x82;
      *op++ = 0xac;
    }
  else if (c >= 0x80) /* other latin9 characters should be representable like this.. */
    {
      *op++ = 0xc0 | (c >> 6);
      *op++ = 0x80 | (c & 63);
    }
  else /* direct mapping in utf8 */
    {
      *op++ = c;
    }
  return op - out;
}

unsigned char *fg_utf8_encode(const unsigned char *str, size_t *szp)
{
  size_t sz = *szp;
  unsigned char *out = malloc(sz*3+1), *op=out;
  unsigned int x;

  for (x=0; x<sz; x++)
    {
      op += fg_utf8_encode_char(str[x], op);
    }
  *op = '\0';
  *szp = op-out;

  return out;
}
