/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Adapte a flrn...
 */ 

/* Attention, il faudra definir proprement Charset, et le renommer
 * Options.Charset... */

static char Charset[]="iso-8859-1";

#include "rfc2047.h"

#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "flrn.h"

#define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0
#define hexval(c) Index_hex[(int)(c)]

#define IsPrint(c) (isprint((unsigned char)(c)) || \
	((unsigned char)(c) >= 0xa0))


static char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
  't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', '+', '/'
};

int Index_64[128] = {
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
  52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
  -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
  15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
  -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
  41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

/* on est tolérant... un caractère bugué peut être 0 ! */
/* comme ça, "= A" est reconnu comme "=0A" */
int Index_hex[128] = {
   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0, 1, 2, 3,  4, 5, 6, 7,  8, 9, 0, 0,  0, 0, 0, 0,
   0,10,11,12, 13,14,15, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0,10,11,12, 13,14,15, 0,  0, 0, 0, 0,  0, 0, 0, 0,
   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
};

const char *tspecials = "@<>()[];:.,\\\"?/=\n\r";


typedef void encode_t (char *, unsigned char *, size_t);


static void q_encode_string (char *d, unsigned char *s, size_t len)
{
  char charset[SHORT_STRING];
  int cslen;
  int wordlen;
  char *wptr = d;

  if (!s) return;
#if HAVE_SNPRINTF
  snprintf (charset, sizeof (charset),
   "=?%s?Q?", strcasecmp ("us-ascii", Charset)==0 ?"unknown-8bit":Charset);
#else
  sprintf (charset,
   "=?%s?Q?", strcasecmp ("us-ascii", Charset)==0 ?"unknown-8bit":Charset);
#endif
  cslen = strlen (charset);

  /*
   * Hack to pull the Re: and Fwd: out of the encoded word for better
   * handling by agents which do not support RFC2047.
   */

  if (strncasecmp ("re: ", (char *) s, 4) == 0)
  {
    strncpy (wptr, (char *) s, 4);
    wptr += 4;
    s += 4;
  }
  else if (strncasecmp ("fwd: ", (char *) s, 5) == 0)
  {
    strncpy (wptr, (char *) s, 5);
    wptr += 5;
    s += 5;
  }

  strcpy (wptr, charset);
  wptr += cslen;
  wordlen = cslen;

  while (*s)
  {
    if (wordlen >= 72)
    {
      strcpy(wptr, "?=\n ");
      wptr += 4;
      strcpy(wptr, charset);
      wptr += cslen;
      wordlen = cslen;
    }

    if (*s == ' ')
    {
      *wptr++ = '_';
      wordlen++;
    }
    else if (*s & 0x80 || (strchr (tspecials, *s) != NULL))
    {
      if (wordlen >= 70)
      {
	strcpy (wptr, "?=\n ");
	wptr += 4;
	strcpy (wptr, charset);
	wptr += cslen;
	wordlen = cslen;
      }
      sprintf (wptr, "=%02X", *s);
      wptr += 3;
      wordlen += 3;
    }
    else
    {
      *wptr++ = *s;
      wordlen++;
    }
    s++;
  }
  strcpy (wptr, "?=");
}

static void b_encode_string (char *d, unsigned char *s, size_t len)
{
  char charset[SHORT_STRING];
  char *wptr = d;
  int cslen;
  int wordlen;

  if (!s) return;

#if HAVE_SNPRINTF
  snprintf (charset, sizeof (charset), "=?%s?B?", Charset);
#else
  sprintf (charset, "=?%s?B?", Charset);
#endif
  cslen = strlen (charset);
  strcpy (wptr, charset);
  wptr += cslen;
  wordlen = cslen;

  while (*s)
  {
    if (wordlen >= 71)
    {
      strcpy(wptr, "?=\n ");
      wptr += 4;
      strcpy(wptr, charset);
      wptr += cslen;
      wordlen = cslen;
    }

    *wptr++ = B64Chars[ (*s >> 2) & 0x3f ];
    *wptr++ = B64Chars[ ((*s & 0x3) << 4) | ((*(s+1) >> 4) & 0xf) ];
    s++;
    if (*s)
    {
      *wptr++ = B64Chars[ ((*s & 0xf) << 2) | ((*(s+1) >> 6) & 0x3) ];
      s++;
      if (*s)
      {
	*wptr++ = B64Chars[ *s & 0x3f ];
	s++;
      }
      else
	*wptr++ = '=';
    }
    else
    {
      *wptr++ = '=';
      *wptr++ = '=';
    }

    wordlen += 4;
  }

  strcpy(wptr, "?=");
}

void rfc2047_encode_string (char *d, unsigned char *s, size_t l)
{
  int count = 0;
  int len;
  unsigned char *p = s;
  encode_t *encoder;

  /* First check to see if there are any 8-bit characters */
  while (*p)
  {
    if (*p & 0x80) count++;
    p++;
  }
  if (!count)
  {
    strfcpy (d, (char *)s, l);
    return;
  }

  if (strcasecmp("us-ascii", Charset) == 0 ||
      strncasecmp("iso-8859", Charset, 8) == 0)
    encoder = q_encode_string;
  else
  {
    /* figure out which encoding generates the most compact representation */
    len = strlen ((char *) s);
    if ((count * 2) + len <= (4 * len) / 3)
      encoder = q_encode_string;
    else
      encoder = b_encode_string;
  }

  (*encoder)(d, (unsigned char*) safe_strdup((char *)s), l);
}

static int rfc2047_decode_word (char *d, const char *s, size_t len)
{
  char *p = safe_strdup (s);
  char *pp = p;
  char *pd = d;
  int enc = 0, filter = 0, count = 0, c1, c2, c3, c4;
  if (!p) return -1;

  while ((pp = strtok (pp, "?")) != NULL)
  {
    count++;
    switch (count)
    {
      case 2:
	if (strcasecmp (pp, Charset) != 0)
	  filter = 1;
	break;
      case 3:
	if (toupper (*pp) == 'Q')
	  enc = ENCQUOTEDPRINTABLE;
	else if (toupper (*pp) == 'B')
	  enc = ENCBASE64;
	else
	  return (-1);
	break;
      case 4:
	if (enc == ENCQUOTEDPRINTABLE)
	{
	  while (*pp && len > 0)
	  {
	    if (*pp == '_')
	    {
	      *pd++ = ' ';
	      len--;
	    }
	    else if (*pp == '=')
	    {
	      *pd++ = (hexval(pp[1]) << 4) | hexval(pp[2]);
	      len--;
	      pp += 2;
	    }
	    else
	    {
	      *pd++ = *pp;
	      len--;
	    }
	    pp++;
	  }
	  *pd = 0;
	}
	else if (enc == ENCBASE64)
	{
	  while (*pp && len > 0)
	  {
	    c1 = Index_64[(int) pp[0]];
	    c2 = Index_64[(int) pp[1]];
	    *pd++ = (c1 << 2) | ((c2 >> 4) & 0x3);
	    if (--len == 0) break;
	    
	    if (pp[2] == '=') break;

	    c3 = Index_64[(int) pp[2]];
	    *pd++ = ((c2 & 0xf) << 4) | ((c3 >> 2) & 0xf);
	    if (--len == 0)
	      break;

	    if (pp[3] == '=')
	      break;

	    c4 = Index_64[(int) pp[3]];   
	    *pd++ = ((c3 & 0x3) << 6) | c4;
	    if (--len == 0)
	      break;

	    pp += 4;
	  }
	  *pd = 0;
	}
	break;
    }
    pp = 0;
  }
  free (p);
  p=strchr(d,'\n');
  while(p) {
    if (p[1] != ' ') *p=' ';
    p++;
    p=strchr(p,'\n');
  }
  if (filter)
  {
    pd = d;
    while (*pd)
    {
      if (!IsPrint ((unsigned char) *pd))
	*pd = '?';
      pd++;
    }
  }
  return (0);
}

/* try to decode anything that looks like a valid RFC2047 encoded
 * header field, ignoring RFC822 parsing rules
 */
void rfc2047_decode (char *d, const char *s, size_t dlen)
{
  const char *p, *q;
  size_t n;
  int found_encoded = 0;

  dlen--; /* save room for the terminal nul */
  if ((dlen==0) && (d != s)) {
     strfcpy (d, s, dlen + 1);
     return;
  }

  while (*s && dlen > 0)
  {
    if ((p = strstr (s, "=?")) == NULL ||
	(q = strchr (p + 2, '?')) == NULL ||
	(q = strchr (q + 1, '?')) == NULL ||
	(q = strstr (q + 1, "?=")) == NULL)
    {
      /* no encoded words */
      if (d != s)
	strfcpy (d, s, dlen + 1);
      return;
    }

    if (p != s)
    {
      n = (size_t) (p - s);
      /* ignore spaces between encoded words */
      if (!found_encoded || strspn (s, " \t\n") != n)
      {
	if (n > dlen)
	  n = dlen;
	if (d != s)
	  memcpy (d, s, n);
	d += n;
	dlen -= n;
      }
    }

    rfc2047_decode_word (d, p, dlen);
    found_encoded = 1;
    s = q + 2;
    n = strlen (d);
    dlen -= n;
    d += n;
  }
  *d = 0;
}
