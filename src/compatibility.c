/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      compatibility.c : fonctions de compatibilit�
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "flrn.h"
#include "config.h"
#include "compatibility.h"

static char UNUSED rcsid[]="$Id$";

#ifndef HAVE_SNPRINTF
int snprintf(char *out, int len, char *fmt, ...) {
   va_list ap;
   int res;
   va_start(ap, fmt);
   res=vsprintf (out, fmt, ap);
   va_end(ap);
   return res;
}
#endif

/* Les fonctions qui suivent ne sont pas vraiment la pour la compatibilite.  */
/* Plutot pour eviter les OS qui louzent...				     */
#ifndef DEBUG_MALLOC
void *safe_malloc(size_t s) {
#else
void *__safe_malloc(size_t s, char *f, char *fi, int l) {
#endif
   void *res;
   if (s==0) return NULL;
   if ((res=malloc(s))==NULL) {
      fprintf(stderr, "Not enough memory!\n");
      exit(-1);
   } else {
#ifdef DEBUG_MALLOC
     fprintf(stderr,"%8lx malloc %ld, %s :%s, %d\n",(long) res,(long) s,
	 f,fi,l);
#endif
     return res;
   }
   return NULL;
}

#ifndef DEBUG_MALLOC
void *safe_calloc(size_t s, size_t t) {
#else
void *__safe_calloc(size_t s, size_t t, char *f, char *fi, int l)
{
#endif
   void *res;
   if (s==0) return NULL;
   if ((res=calloc(s,t))==NULL) {
      fprintf(stderr, "Not enough memory!\n");
      exit(-1);
   } else {
#ifdef DEBUG_MALLOC
     fprintf(stderr,"%8lx calloc %ld * %ld, %s :%s, %d\n",(long) res,(long) s,
	 (long) t,f,fi,l);
#endif
     return res;
   }
   return NULL;
}

#ifndef DEBUG_MALLOC
void *safe_realloc(void *ptr, size_t s) {
#else
void *__safe_realloc(void *ptr, size_t s,
    char *f, char *fi, int l) {
#endif
   void *res;
   if (s==0) {
      free(ptr);
      return NULL;
   }
   if (ptr==NULL) return safe_malloc(s); else
     if ((res=realloc(ptr,s))==NULL) {
        fprintf(stderr, "Not enough memory!\n");
	exit(-1);
     } else {
#ifdef DEBUG_MALLOC
       fprintf(stderr,"%8lx free *realloc* (%8lx) %ld, %s :%s, %d\n",
	   (long) ptr, (long) res,
	   (long) s, f,fi,l);
       fprintf(stderr,"%8lx malloc *realloc* (%8lx) %ld, %s :%s, %d\n",
	   (long) res, (long) ptr,
	   (long) s, f,fi,l);
#endif
       return res;
     }
   return NULL;
}

#ifndef DEBUG_MALLOC
void *safe_strdup(const char *s) {
#else
void *__safe_strdup(const char *s, char *f, char *fi, int l) {
#endif
   char *res;
#ifdef DEBUG_MALLOC
   { char blah[1024]="*safe_strdup* ";
     strcat(blah,f);
     res=__safe_malloc(strlen(s)+1,blah,fi,l);
   }
#else
   res=safe_malloc(strlen(s)+1);
#endif
   strcpy(res,s);
   return res;
}

#ifndef DEBUG_MALLOC
void *safe_flstrdup(const flrn_char *s) {
#else
void *__safe_flstrdup(const flrn_char *s, char *f, char *fi, int l) {
#endif
   char *res;
#ifdef DEBUG_MALLOC
   { char blah[1024]="*safe_flstrdup* ";
     strcat(blah,f);
     res=__safe_malloc((strlen(s)+1)*sizeof(flrn_char),blah,fi,l);
   }
#else
   res=safe_malloc((strlen(s)+1)*sizeof(flrn_char));
#endif
   fl_strcpy(res,s);
   return res;
}

#ifndef DEBUG_MALLOC
void *safe_strappend(char *s, const char *a) {
#else
void *__safe_strappend(char *s,const char *a, char *f, char *fi, int l) {
#endif
#ifdef DEBUG_MALLOC
  char blah[1024]="*safe_strappend* ";
  strcat(blah,f);
  if(s) { 
    char *new=__safe_realloc(s,strlen(s)+strlen(a)+1,blah,fi,l);
    strcat(new,a);
    return new;
  } else return __safe_strdup(a,blah,fi,l);
#else
  if(s) { 
    char *new=safe_realloc(s,strlen(s)+strlen(a)+1);
    strcat(new,a);
    return new;
  } else return safe_strdup(a);
#endif
}
