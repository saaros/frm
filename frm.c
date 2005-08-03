/*
 * $Id$
 *
 * Copyright (c) 2003-2005 Oskari Saarenmaa <oskari@saarenmaa.fi>.
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


#define FRM_VERSION "0.3.1"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifdef __linux__
#include <getopt.h>
#else
extern char *optarg;
extern int optind, opterr, optopt;
#endif


#define FRMOPT_ONE     0x0001
#define FRMOPT_IGNORE  0x0002


static int  display_folder(char *);
static void display_mail();
static void handle_line(char *line);
static void mime_decode(char **);


char *from, *subject, head;
int  options = 0;


int
main(int argc, char **argv)
{
  int result, retval = 0;

  while((result = getopt(argc, argv, "1v")) != -1)
  {
    switch(result)
    {
      case '1' :
        options |= FRMOPT_ONE;
        break;

      case 'v' :
        printf("frm(1) version "FRM_VERSION"\n");
        exit(0);
    }
  }

  if(optind < argc)
  {
    while(optind < argc)
    {
      retval |= display_folder(argv[optind++]);
    }
  }
  else
  {
    retval |= display_folder(getenv("MAIL"));
  }

  return 0;
}

static int
display_folder(char *fn)
{
  char had_ignore = 0, pline[0x2000], lline[0xffff];
  FILE *fp;

  if(access(fn, R_OK) != 0)
  {
    perror(fn);
    return 1;
  }

  subject = NULL;
  from    = NULL;
  head    = 1;
  *lline  = 0;

  if(options & FRMOPT_ONE)
    if((had_ignore = options & FRMOPT_IGNORE) == 0)
      options |= FRMOPT_IGNORE;

  fp = fopen(fn, "r");
  for(;;)
  {
    char *p, *ret;

    ret = fgets(pline, sizeof pline, fp);
    if(ret == NULL || feof(fp))
      break;
    if(isspace(*(p=pline)))
    {
      if(strlen(lline) >= sizeof(lline) - sizeof(pline) - 2)
        continue; /* discard entire line */
      while(isspace(*p))
        p++;
      if(strlen(p) == 0)
      {
        head = 0;
        continue;
      }
      else
      {
        char *q = lline+strlen(lline);
        if(!(*(p+0) == '=' && *(p+1) == '?' && *(q-1) == '=' && *(q-2) == '?'))
          *q++ = ' ';
        strcpy(q, p);
      }
    }
    else
    {
      handle_line(lline);
      strcpy(lline, pline);
    }
    p = strrchr(lline, 0) - 1;
    while(isspace(*p))
      *p-- = 0;
  }
  fclose(fp);

  if(had_ignore == 0)
    options &= ~FRMOPT_IGNORE;

  display_mail(); /* show the last mail */

  return 0;
}

static char *
utf8_decode(const char *str)
{
  unsigned int x, sz=strlen(str);
  char *out=malloc(sz+4), *op=out;

  for(x=0; x<sz;)
  {
    unsigned char C = (unsigned char)str[x];
    unsigned int c = C, b;
    if(c >= 0xf0)
      b = 4;
    else if(c >= 0xe0)
      b = 3;
    else if(c >= 0xc0)
      b = 2;
    else
    {
      *op++ = (char)c;
      x++;
      continue;
    }
    if(x+b > sz)
    {
      strcpy(op, "XXX");
      return out;
    }
    c = (str[x+b-1]&63) | ((str[x+b-2]&63)<<6);
    if(b > 2)
      c |= (str[x+b-3]&63)<<12;
    if(b > 3)
      c |= (str[x+b-4]&63)<<18;
    if(c > 0xff)
      *op++ = '?';
    else
      *op++ = (char)c;
    x += b;
  }
  *op = 0;

  return out;
}

static void
mime_decode(char **strvarp)
{
  char *str = *strvarp, *strp, *p, *q;

  strp = str;
  for(;;)
  {
    char *tstart, *start, *end, *rep, *charset;

    p = strstr(strp, "=?");
    if(p == NULL) /* not mime encoded */
      return;

    charset = p+2;

    q = strchr(charset, '?');
    if(q == NULL) /* malformed */
      return;
    *q++ = 0;

    tstart = p;
    start = q+2;
    end = strstr(start, "?=");
    if(end == NULL) /* malformed */
      return;
    rep = (char*)malloc(end-start+1);
    memset(rep, 0, end-start+1);

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

    if(strcasecmp(charset, "UTF-8") == 0)
    {
      char *dec = utf8_decode(rep);
      free(rep);
      rep = dec;
    }

    strcpy(tstart, rep);
    memmove(tstart+strlen(rep), end+2, strlen(end+2)+1);
    strp = tstart+strlen(rep);
    free(rep);
  }
}

static char *
unmangle_from()
{
  char *a = NULL, *p, *q;
  size_t len;

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

  mime_decode(&from);

  return from;
}

static char *
unmangle_subject()
{
  mime_decode(&subject);

  return subject;
}

static void
display_mail()
{
  if(!subject && !from)
    return;

  if(!(options & FRMOPT_IGNORE))
  {
    printf("%-22s  %s\n", unmangle_from(), unmangle_subject());
  }

  free(from);
  free(subject);

  from    = NULL;
  subject = NULL;
}

static void
handle_line(char *line)
{
  char *key, *val, *p;

  if(strstr(line, "From ") == line)
  {
    display_mail();

    head    = 1;
    from    = strdup("<no sender>");
    subject = strdup("<no subject>");

    return;
  }
  else if(!head || (p = strchr(line, ':')) == NULL)
  {
    return;
  }

  val = p+1;
  *p = 0;
  while(isspace(*val))
    val++;
  key = line;
  for(p=key; *p; p++)
    *p = tolower(*p);

  if(strcmp(key, "from") == 0)
  {
    free(from);
    from = strdup(val);
  }
  else if(strcmp(key, "subject") == 0)
  {
    free(subject);
    if(!strlen(val))
      subject = strdup("<empty subject>");
    else
      subject = strdup(val);
  }
}
