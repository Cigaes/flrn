/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      compatibility.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_COMPATIBILITY_H
#define FLRN_COMPATIBILITY_H

/* juste pour les rcsid, on le met ici car ce fichier est appelé partout */
#ifdef __GNUC__
#define UNUSED \
  __attribute__((__unused__))
#else
#define UNUSED
#endif

/* On definit ce qui pourait nous manquer */

#include <stdio.h>
#include <stdarg.h>
#include "enc/enc_base.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifndef __GNUC__
/* on veut pas d'erreurs sur inline */
#  define inline
#endif

#ifndef HAVE_SNPRINTF
extern int snprintf(char * /*out*/, int /*len*/, char * /*fmt*/, ...);
#endif

#ifndef HAVE_ISBLANK
static inline int isblank(int c) {
  return (c==' ') || (c=='\t');
}
#endif

#ifndef HAVE_ISDIGIT
static inline int isdigit(int c) {
  return (c>='0') && (c<='9');
}
#endif

#ifdef HAVE_REGCOMP
#  ifdef HAVE_REGDEF_H
#    include <regdef.h>
#  endif
#  include <regex.h>
#else
#  include "rxposix.h"
#endif

#include <string.h>
#include <strings.h>
#ifndef HAVE_STRSPN
static inline size_t strspn(const char *s, const char *accept) {
   int n=0;
   while (s[n] && (index(accept, s[n]))) n++;
   return n;
}
#endif      

/* Les fonctions qui suivent ne sont pas vraiment la pour la compatibilite.  */
/* Plutot pour eviter les OS qui louzent...				     */
#ifndef DEBUG_MALLOC
extern void *safe_malloc(size_t);
#else
#define safe_malloc(a) __safe_malloc(a,__PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_malloc(size_t s, char *f, char *fi, int l);
#endif

#ifndef DEBUG_MALLOC
extern void *safe_calloc(size_t, size_t );
#else
#define safe_calloc(a,b) __safe_calloc(a,b,__PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_calloc(size_t, size_t, char *, char *, int );
#endif

#ifndef DEBUG_MALLOC
extern void *safe_realloc(void *, size_t ) ;
#else
#define safe_realloc(ptr,s) __safe_realloc(ptr,s,\
    __PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_realloc(void *, size_t ,
    char *, char *, int ) ;
#endif

#ifndef DEBUG_MALLOC
extern void *safe_strdup(const char *);
#else
#define safe_strdup(s) __safe_strdup(s,__PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_strdup(const char *, char *, char *, int);
#endif

#ifndef DEBUG_MALLOC
extern void *safe_flstrdup(const flrn_char *);
#else
#define safe_flstrdup(s) __safe_flstrdup(s,__PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_flstrdup(const flrn_char *, char *, char *, int);
#endif

#ifndef DEBUG_MALLOC
extern void *safe_strappend(char *, const char *);
#else
#define safe_strappend(s,t) __safe_strappend(s,t,__PRETTY_FUNCTION__,__FILE__,__LINE__)
extern void *__safe_strappend(char *,const char *, char *, char *, int);
#endif

#ifdef DEBUG_MALLOC
#define free(a) (fprintf(stderr,"%8lx free, %s :%s, %d\n",(long) a,\
    __PRETTY_FUNCTION__,__FILE__,__LINE__), free(a))
#endif

#endif
