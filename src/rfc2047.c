/*
 *
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * ce code est repris à mutt et adapté à flrn
 *
 * Ce code est soumis à la GNU General Public License
 */ 

/* $Id$ */

#include "rfc2047.h"

#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include "config.h"
#include "flrn.h"

static UNUSED char rcsid[]="$Id$";

  
#define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0
#define hexval(c) Index_hex[(int)(c)]


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


typedef void encode_t (char **, unsigned char *, size_t *, const char *);


static void q_encode_string (char **d, unsigned char *s, size_t *len,
	                     const char *codeset)
{
  char charset[SHORT_STRING];
  size_t cslen;
  size_t wordlen, tmplen;

  if (!s) return;

#if HAVE_SNPRINTF
  snprintf (charset, sizeof (charset),
   "=?%s?Q?", codeset);
#else
  sprintf (charset,
   "=?%s?Q?", codeset);
#endif

  cslen = strlen (charset);

  /*
   * Hack to pull the Re: and Fwd: out of the encoded word for better
   * handling by agents which do not support RFC2047.
   */

  if (strncasecmp ("re: ", (char *) s, 4) == 0)
  {
    tmplen = (*len>3) ? 4 : *len;
    strncpy (*d, (char *) s, tmplen);
    (*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
    s += 4;
  }
  else if (strncasecmp ("fwd: ", (char *) s, 5) == 0)
  {
    tmplen = (*len>4) ? 5 : *len;
    strncpy (*d, (char *) s, tmplen);
    (*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
    s += 5;
  }

  if (cslen>*len) cslen=*len;
  strncpy ((*d), charset, cslen);
  (*d) += cslen; (*len) -= cslen; if (*len==0) return;
  wordlen = cslen;

  while (*s)
  {
    if (wordlen >= 72)
    {
      tmplen = (*len>3) ? 4 : *len;
      strncpy(*d, "?=\n ",*len);
      (*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
      if (cslen>*len) cslen=*len;
      strncpy ((*d), charset, cslen);
      (*d) += cslen; (*len) -= cslen; if (*len==0) return;
      wordlen = cslen;
    }

    if (*s == ' ')
    {
      *((*d)++) = '_';  (*len)--;
      wordlen++;
    }
    else if (*s & 0x80 || (strchr (tspecials, *s) != NULL))
    {
      if (wordlen >= 70)
      {
	tmplen = (*len>3) ? 4 : *len;
	strncpy(*d, "?=\n ",*len);
	(*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
	if (cslen>*len) cslen=*len;
	strncpy ((*d), charset, cslen);
	(*d) += cslen; (*len) -= cslen; if (*len==0) return;
	wordlen = cslen;
      }
      if (*len<3) return;
      sprintf ((*d), "=%02X", *s);
      (*d) += 3;  (*len) -= 3;
      wordlen += 3;
    }
    else
    {
      *((*d)++) = *s;  (*len)--;
      wordlen++;
    }
    s++;
  }
  tmplen = (*len>1) ? 2 : *len;
  strncpy ((*d), "?=", tmplen);
  (*d) += tmplen;  (*len) -= tmplen;
}

static void b_encode_string (char **d, unsigned char *s, size_t *len, 
	                      const char *codeset)
{
  char charset[SHORT_STRING];
  size_t cslen, tmplen;
  size_t wordlen;

  if (!s) return;

#if HAVE_SNPRINTF
  snprintf (charset, sizeof (charset), "=?%s?B?", codeset);
#else
  sprintf (charset, "=?%s?B?", codeset);
#endif
  cslen = strlen (charset);

  /*
   * Hack to pull the Re: and Fwd: out of the encoded word for better
   * handling by agents which do not support RFC2047.
   */

  if (strncasecmp ("re: ", (char *) s, 4) == 0)
  {
    tmplen = (*len>3) ? 4 : *len;
    strncpy (*d, (char *) s, tmplen);
    (*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
    s += 4;
  }
  else if (strncasecmp ("fwd: ", (char *) s, 5) == 0)
  {
    tmplen = (*len>4) ? 5 : *len;
    strncpy (*d, (char *) s, tmplen);
    (*d) += tmplen;  (*len) -= tmplen;  if (*len==0) return;
    s += 5;
  }

  if (cslen>*len) cslen=*len;
  strncpy (*d, charset, cslen);
  (*len) -= cslen;
  (*d) += cslen;
  if (*len==0) return;
  wordlen = cslen;

  while (*s)
  {
    if (wordlen >= 71)
    {
      tmplen = (*len)>3 ? 4 : (*len);
      strncpy(*d, "?=\n ", tmplen);
      (*len) -= tmplen; (*d)+=tmplen; if (*len==0) return;
      if (cslen>*len) cslen=*len;
      strncpy(*d, charset, cslen);
      (*len) -= cslen; (*d) += cslen;
      if (*len==0) return;
      wordlen = cslen;
    }

    *((*d)++) = B64Chars[ (*s >> 2) & 0x3f ];
    (*len)--; if (*len==0) return;
    *((*d)++) = B64Chars[ ((*s & 0x3) << 4) | ((*(s+1) >> 4) & 0xf) ];
    (*len)--; if (*len==0) return;
    s++;
    if (*s)
    {
      *((*d)++) = B64Chars[ ((*s & 0xf) << 2) | ((*(s+1) >> 6) & 0x3) ];
      (*len)--; if (*len==0) return;
      s++;
      if (*s)
      {
	*((*d)++) = B64Chars[ *s & 0x3f ];
        (*len)--; if (*len==0) return;
	s++;
      }
      else {
	*((*d)++) = '=';
        (*len)--; if (*len==0) return;
      }
    }
    else
    {
      *((*d)++) = '=';
      (*len)--; if (*len==0) return;
      *((*d)++) = '=';
      (*len)--; if (*len==0) return;
    }

    wordlen += 4;
  }

  tmplen = (*len)>1 ? 2 : (*len);
  strncpy(*d, "?=", tmplen);
  (*len) -= tmplen;
  (*d) += tmplen;
}

/* rfc2047 pose problème : que doit-on envoyer */
int good_encode_string (char **d, flrn_char *s, size_t len_s, size_t *len_d)
{
  int count = 0;
  int len;
  char *p;
  const char *codeset;
  char *ch=NULL;
  char *outbuf=NULL, *outptr=NULL;
  size_t len_ob=0,res;
  encode_t *encoder;

  /* first thing is to find the good encoding */
  find_best_conversion(s,len_s,&codeset,NULL);
  if (codeset==NULL) codeset="UTF-8";
  ch=strchr(codeset,':');
  if (ch) {
      size_t l=ch-codeset;
      ch=safe_malloc(l+1);
      strncpy(ch,codeset,l);
      ch[l]='\0';
  } else ch=safe_strdup(codeset);
  fl_revconv_open(ch);
  free(ch);
  find_best_conversion(NULL,0,NULL,NULL);
  res=fl_conv_from_flstring(&s,&len_s,&outbuf,&outptr,&len_ob);
  if (res!=(size_t)(-3)) {
      len_ob=outptr-outbuf;
      *outptr='\0';
  } else {
      len_ob=len_s;
      outbuf=safe_malloc(len_ob+1);
      strcpy(outbuf,s);
      outbuf[len_s]='\0';
  }
  fl_revconv_close();

  /* let's count the 8-bits caracters */
  p=outbuf;
  res=len_ob;
  while (res>0)
  {
    if (((unsigned char) (*p)) & 0x80) count++;
    p++; res--;
  }

  if ((codeset!=NULL) && (strncasecmp("iso-8859", codeset, 8) == 0))
    encoder = q_encode_string;
  else
  {
    /* figure out which encoding generates the most compact representation */
    len = (int)len_ob;
    if ((count * 2) + len <= (4 * len) / 3)
      encoder = q_encode_string;
    else
      encoder = b_encode_string;
  }

  (*encoder)(d, (unsigned char*) outbuf, len_d, codeset);
  free(outbuf);
  return 0;
}

/* encodage suivant la rfc 2047 */
/* critical = 1 -> on n'encode pas les adresses mails. On essaye, au moins */
/* c'est pas très propre, mais c'est le plus simple, et _le plus souvent_
 * ça marche */
int rfc2047_encode_string (char *d, flrn_char *s, size_t len,
	                   int critical) {

  flrn_char *p=s, *f8=s, *l8=s, *t8;
  int count=0,res;

  len--;
  /* the first thing is to check if we have any 8bit character */
  while (*p)
  {
    if ((*p)>>7) {
	if (!count) f8=p;
	l8=p;
	count++;
    }
    p++;
  }
  if (count==0) {
     p=s;
     while ((*p) && (len>0)) { *(d++)=(char) *(p++); len--; }
     *d='\0';
     return 0;
  }
  /* si le code n'est pas critique, on encode tout */
  if (!critical) { f8=s; l8=p; }
  t8=f8;
  while (t8<=l8) {
      if (critical) isolate_non_critical_element(s,t8,l8,&f8,&t8);
          else t8=l8;
      /* copie s */
      while ((s<f8) && (len>0)) { *(d++)=(char) *(s++); len--; }
      if (len==0) break;
      res=good_encode_string(&d,f8,(t8-f8+1),&len);
      if (len==0) break;
      t8++;
      s=t8;
      while ((t8<l8) && (((*t8)>>7)==0)) t8++;
  }
  while ((s<p) && (len>0)) { *(d++)=(char) *(s++); len--; }
  *d='\0';
  return 0;
}

/* first decode : return the decoded word, and the codeset */
static int rfc2047_decode_word (char *d, const char *s, size_t len,
	                        char **codeset)
{
  char *p = safe_strdup (s);
  char *pp = p;
  char *pd = d;
  int enc = 0, count = 0, c1, c2, c3, c4;

  if (!p) return -1;

  while ((pp = strtok (pp, "?")) != NULL)
  {
    count++;
    switch (count)
    {
      case 2:
	*codeset=safe_strdup(pp);
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
  return 0;
}


/* 0 : OK   ;  -1 error  */
int convert_decoded_word (char **d, flrn_char **res, const char *codeset, 
	                  size_t *slen)
{
  size_t inbl = strlen(*d);
  size_t res_t;
  flrn_char *dummy;
#if 0
#else 
  if ((codeset) && (strcasecmp(codeset,"utf-8")==0)) {
      strncpy(*res,*d,*slen);
      if (inbl>*slen) {
	  (*res)+=*slen;
	  *slen=0;
      } else {
	  (*res)+=inbl;
	  *slen-=inbl;
      }
      return 0;
  }
#endif
  if (fl_stdconv_open(codeset)<0) {
      if (fl_stdconv_open(NULL)<0) return -1;
  }

  while (*slen>0) {
     dummy=*res;
     res_t=fl_conv_to_flstring (d,&inbl,&dummy,res,slen);
     if (res_t==-1) {
           if (errno==EILSEQ) {
#if 0
	       *((*res)++)=L'?'; slen--;
#else
	       *((*res)++)='?'; slen--;
#endif
	       (*d)++; inbl--;
	       continue;
	   }
	   if (errno==EINVAL) {
#if 0
	       *((*res)++)=L'?'; slen--;
#else
	       *((*res)++)='?'; slen--;
#endif
	       break;
	   }
	   if (errno==E2BIG) break;
     }
     break;
  }
  fl_stdconv_close();
  return 0;
}

/* try to decode anything that looks like a valid RFC2047 encoded
 * header field, ignoring RFC822 parsing rules
 * raw=0 -> avec tout ; raw=1 -> sans décodage 2047 ; raw = 2 -> utf-8
 */
size_t rfc2047_decode (flrn_char *d, char *s, size_t dlen, int raw)
{
  flrn_char *init_ch=d;
  char *p, *q;
  char *tmpchr;
  char *codeset;
  size_t n;
  int found_encoded = 0;

  dlen--; /* save room for the terminal nul */
  if (raw) 
      convert_decoded_word(&s,&d,(raw==2 ? "utf-8" : NULL),&dlen);
  else 
  {
    while (*s && dlen > 0)
    {
      if ((p = strstr (s, "=?")) == NULL ||
  	(q = strchr (p + 2, '?')) == NULL ||
  	(q = strchr (q + 1, '?')) == NULL ||
  	(q = strstr (q + 1, "?=")) == NULL)
      {
        /* no encoded words */
        convert_decoded_word(&s,&d,NULL,&dlen);
        break;
      }
  
      if (p != s)
      {
        n = (size_t) (p - s);
        /* ignore spaces between encoded words */
        if (!found_encoded || strspn (s, " \t\n") != n)
        {
  	if (n > dlen)
  	  n = dlen;
  	tmpchr=safe_malloc(n+1);
  	strncpy(tmpchr,s,n);
  	tmpchr[n]='\0';
        { 
	    char *bla=tmpchr;
            convert_decoded_word(&bla,&d,NULL,&dlen);
        }
  	free(tmpchr);
  	if (dlen==0) break;
        }
      }
  
      n = strlen(p);
      tmpchr=safe_malloc(n+1);
      rfc2047_decode_word (tmpchr, p, n, &codeset);
      tmpchr[n]='\0';
      { 
	  char *bla=tmpchr;
          convert_decoded_word(&bla,&d,codeset,&dlen);
      }
      free(tmpchr);
      found_encoded = 1;
      s = q + 2;
    }
  } /* raw=0 */
#if 0
  *d=L'\0';
#else
  *d='\0';
#endif
  return (d-init_ch);
}
