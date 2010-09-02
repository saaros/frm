/*
 * $Id$
 *
 * Copyright (c) 2003-2010 Oskari Saarenmaa <oskari@saarenmaa.fi>.
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

#define FRM_VERSION "0.5"

/* drop pages from cache every MAP_RECYCLE bytes */
#define MAP_RECYCLE (1024*1024)

#define _GNU_SOURCE
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <locale.h>
#include <langinfo.h>

#include "utf8.h"

static int display_folder(const char *fn, int skip, int last_only);
static void display_mail(char *from, char *subject);
static char *read_header(const char *start, size_t llen, size_t hsize);

static int is_utf8 = 0;

int
main(int argc, char **argv)
{
  int result, retval = 0;
  int skip = 0, last_only = 0;

  setlocale(LC_CTYPE, "");
  is_utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") == 0;

  while ((result = getopt(argc, argv, "1s:v")) != -1)
    {
      switch(result)
        {
        case '1' :
          last_only = 1;
          break;

        case 's' :
          skip = atoi(optarg);
          break;

        case 'v' :
          printf("frm(1) version "FRM_VERSION"\n");
          exit(0);
        }
    }

  if (optind < argc)
    {
      while (optind < argc)
        {
          retval |= display_folder(argv[optind++], skip, last_only);
        }
    }
  else
    {
      retval |= display_folder(getenv("MAIL"), skip, last_only);
    }

  return 0;
}

static int
display_folder(const char *fn, int skip, int last_only)
{
  const char new_mail_str[] = "From ";
  const size_t new_mail_str_len = sizeof(new_mail_str)-1;
  char *from, *subject;
  char *p, *q, *b, *block, *body, *head;
  struct stat st;
  ssize_t left;
  size_t llen, hsize, fsize;
  off_t off;
  int fd;

  fd = open(fn, O_RDONLY);
  if (fd < 0)
    {
      perror(fn);
      return 1;
    }

  if (fstat(fd, &st) < 0)
    {
      perror(fn);
      return 1;
    }

  subject = NULL;
  from = NULL;
  fsize = st.st_size;

  /* New mails start with "\n\r?From ", look for those blocks
   * and make sure we won't miss mails that are split in
   * two blocks (look at fsize-7 bytes on all but the last block)
   */
  block = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
  if (block == MAP_FAILED)
    block = NULL;

  head = block;
  /* special case a bit here to seeking backwards for the last header */
  if (block && last_only)
    {
      for (b = block+fsize-0xffff, left=0xffff;
           b > block;
           b-= 0xffff, left += 0xffff)
        {
          p = memmem(b, left, new_mail_str, new_mail_str_len);
          // check for \n\r
          if (p && (*(p-1) == '\n' || (*(p-2) == '\n' && *(p-1) == '\r')))
            {
              head = p;
              break;
            }
        }
    }

  /* Look first for "From ", and then make sure that it is
   * preceded by possibly a \r and a \n.
   */
  for (off=0, b=head; b; )
    {
      llen = b-block;
      left = fsize-llen;
      if (left <= 0)
        break;

      if (llen-off > MAP_RECYCLE)
        {
          /* drop some pages from the cache */
          madvise(block, llen, MADV_DONTNEED);
          off = llen;
        }

      p = memmem(b, left, new_mail_str, new_mail_str_len);
      // check for \n\r
      if (p && p != block)
        if (! (*(p-1) == '\n' || (p > block+1 && *(p-2) == '\n' && *(p-1) == '\r')))
          {
            /* From was not preceded by \n or \n\r */
            b = p + new_mail_str_len;
            continue;
          }

      if (p == NULL)
        {
          /* No new mail was found in this block.  Increment "pos" and start again. We need
           * to make sure that we don't increment "pos" too much, if there was a header
           * split on block boundary we want to catch it
           */
          break;
        }

      /* So we found a header.  Yay. */
      head = p;
      left = fsize-(head-block);

      /* Look for an end-of-header. */
      for (body = NULL; body == NULL; p = q+1)
        {
          if ((q = memchr(p, '\n', left)) == NULL)
            break;

          left = fsize-(q-block);
          if (left >= 0 && *(q+1) == '\n')
            body = q+2;
          else if (left >= 1 && *(q+1) == '\r' && *(q+2) == '\n')
            body = q+3;
          else
            left = left-1;
        }

      if (body == NULL)
        break; /* invalid mail / end of mbox */

      b = body; /* for next block */
      hsize = body-head; /* size of header */

      /* read the header, look for subject: and from: */

      if (from) { free(from); from = 0; }
      if (subject) { free(subject); subject = 0; }

      for (p=head; hsize>0; )
        {
          q = memchr(p, '\n', hsize);
          if (q == NULL) /* end of header, apparently. */
            break;
          llen = q-p-1;

          if (llen > 5 && strncasecmp(p, "from:", 5) == 0)
            from = read_header(p, llen, hsize);
          else if (llen > 8 && strncasecmp(p, "subject:", 8) == 0)
            subject = read_header(p, llen, hsize);

          if (from && subject)
            {
              skip --;
              if (!last_only && skip <= 0)
                display_mail(from, subject);
              break;
            }

          hsize -= llen+2;
          p = q+1;
          if (*p == '\r')
            {
              hsize--;
              p++;
            }
        }

      /* read next mail */
    }

  if (block)
    munmap(block, fsize);
  close(fd);

  if (last_only && from && subject)
    display_mail(from, subject); /* show the last mail */

  return 0;
}

static char *
read_header(const char *start, size_t llen, size_t hsize)
{
  char done=0, pline[0x2000], *p, *q, *lp = pline;

  p = (char *) start;
  q = memchr(p, ':', llen);
  llen -= q-p-1;
  p = q+1;

  while (isspace(*p) && llen > 0)
    {
      p ++;
      llen --;
    }

  if (llen > 0)
    {
      lp = mempcpy(pline, p, llen);
      while (isspace(*(lp-1)))
        {
          lp --;
          llen --;
        }
    }

  for (q=p+llen+1; q<start+hsize; q=p)
    {
      while ((*q == '\n' || *q == '\r') && q < (start+hsize))
        q ++;
      if (q >= start+hsize || !isspace(*q))
        break;
      llen = hsize-(q-start);
      while (llen > 0 && isspace(*q))
        {
          q ++;
          llen --;
        }

      p = memchr(q, '\n', llen);
      if (p)
        llen = p-q;
      else
        done = 1;

      if (lp > pline+1 && llen > 1)
        if (!(*q == '=' && *(q+1) == '?' && *(lp-1) == '=' && *(lp-2) == '?'))
          *lp ++ = ' ';
      lp = mempcpy(lp, q, llen);

      if (done)
        break;
    }

  *lp = 0;
  return strdup(pline);
}

static char *
mime_decode(char *strvarp)
{
  char *str = strvarp, *strp, *p, *q;

  if (strvarp == NULL)
    return "(null)";

  strp = str;
  for(;;)
  {
    char *tstart, *start, *end, *rep, *charset;
    int cs_utf8;

    p = strstr(strp, "=?");
    if (p == NULL) /* not mime encoded */
      return strvarp;

    charset = p+2;

    q = strchr(charset, '?');
    if (q == NULL) /* malformed */
      return strvarp;
    *q++ = 0;

    tstart = p;
    start = q+2;
    end = strstr(start, "?=");
    if (end == NULL) /* malformed */
      return strvarp;
    rep = calloc(end-start+1, sizeof(char));

    if(tolower(*q) == 'q')
    {
      for(p=start, q=rep; p<end; p++)
      {
        if(*p == '_')
          *q++ = ' ';
        else if(*p == '=')
        {
          unsigned int cid;
          if(sscanf(p+1, "%2X", &cid) == 1)
            *q++ = cid;
          else
            *q++ = '?';
          p += 2;
        }
        else
          *q++ = *p;
      }
    }
    else if(tolower(*q) == 'b')
    {
#define BASE64_LOOKUP(x) ( \
        (x >= 'A' && x <= 'Z') ? x-65 : \
        (x >= 'a' && x <= 'z') ? x-71 : \
        (x >= '0' && x <= '9') ? x+ 4 : \
        (x == '+') ? 62 : \
        (x == '/') ? 63 : \
        0)

      for(p=start, q=rep; p<end; p+=4)
      {
        int data = BASE64_LOOKUP(*(p+0)) << 18 |
                   BASE64_LOOKUP(*(p+1)) << 12 |
                   BASE64_LOOKUP(*(p+2)) <<  6 |
                   BASE64_LOOKUP(*(p+3));
        *q++ = data >> 16 & 0xff;
        *q++ = data >>  8 & 0xff;
        *q++ = data >>  0 & 0xff;
      }
    }
    else /* unknown encoding method */
    {
      strp = end + 2;
      free(rep);
      continue;
    }

    cs_utf8 = strcasecmp(charset, "UTF-8") == 0;
    if (cs_utf8 && !is_utf8)
    {
      size_t len = strlen(rep);
      char *dec = (char *) fg_utf8_decode((unsigned char *) rep, &len);
      free(rep);
      rep = dec;
    }
    else if (!cs_utf8 && is_utf8)
    {
      size_t len = strlen(rep);
      char *enc = (char *) fg_utf8_encode((unsigned char *) rep, &len);
      free(rep);
      rep = enc;
    }

    strcpy(tstart, rep);
    memmove(tstart+strlen(rep), end+2, strlen(end+2)+1);
    strp = tstart+strlen(rep);
    free(rep);
  }
  return strvarp;
}

static char *
unmangle_from(char *from)
{
  char *a = NULL, *p, *q;
  size_t len;

  if (from == NULL)
    return "(unknown)";

  /* From: "Real Name" <address@example.com> */
  if((p = strstr(from, "<")) != NULL)
  {
    a = p+1;
    if(p > from && isspace(*(p-1)))
      p--;
    *p = 0;
    if((p = strchr(a, '>')) != NULL)
      *p = 0;
  }
  /* From: address@example.com (Real Name) */
  else if((p = strstr(from, "(")) != NULL)
  {
    if((q = strchr(p, ')')) != NULL)
      *q = 0;
    memmove(from, p+1, strlen(p+1)+1);
  }

  p = from;
  q = from+strlen(from)-1;
  if((*p == '<' && *q == '>') || (*p == '"' && *q == '"'))
  {
    *q = 0;
    memmove(from, from+1, strlen(from)+1);
  }
  if(strlen(from) == 0 && a != NULL && (len = strlen(a)))
  {
    memmove(from, a, len+1); /* string plus zero */
  }

  return mime_decode(from);
}

static void
display_mail(char *from, char *subject)
{
  printf("%-22s  %s\n", unmangle_from(from), mime_decode(subject));
}
