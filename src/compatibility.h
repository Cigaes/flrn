/* On definit ce qui pourait nous manquer */

#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>

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
static inline void *safe_malloc(size_t s) {
#else
#define safe_malloc(a) __safe_malloc(a,__PRETTY_FUNCTION__,__FILE__,__LINE__)
static inline void *__safe_malloc(size_t s, char *f, char *fi, int l) {
#endif
   void *res;
   if (s==0) return NULL;
   if ((res=malloc(s))==NULL) {
      fprintf(stderr, "Mémoire insuffisante !\n");
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
static inline void *safe_calloc(size_t s, size_t t) {
#else
#define safe_calloc(a,b) __safe_calloc(a,b,__PRETTY_FUNCTION__,__FILE__,__LINE__)
static inline void *__safe_calloc(size_t s, size_t t, char *f, char *fi, int l)
{
#endif
   void *res;
   if (s==0) return NULL;
   if ((res=calloc(s,t))==NULL) {
      fprintf(stderr, "Mémoire insuffisante !\n");
      exit(-1);
   } else {
#ifdef DEBUG_MALLOC
     fprintf(stderr,"%8lx calloc %ld * %ld, %s :%s, %d\n",(long) res,(long) s,
	 (long t),f,fi,l);
#endif
     return res;
   }
   return NULL;
}

#ifndef DEBUG_MALLOC
static inline void *safe_realloc(void *ptr, size_t s) {
#else
#define safe_realloc(ptr,s) __safe_realloc(ptr,s,\
    __PRETTY_FUNCTION__,__FILE__,__LINE__)
static inline void *__safe_realloc(void *ptr, size_t s,
    char *f, char *fi, int l) {
#endif
   void *res;
   if (s==0) {
      free(ptr);
      return NULL;
   }
   if (ptr==NULL) return safe_malloc(s); else
     if ((res=realloc(ptr,s))==NULL) {
        fprintf(stderr, "Mémoire insuffisante !\n");
	exit(-1);
     } else {
#ifdef DEBUG_MALLOC
       fprintf(stderr,"%8lx free *realloc* (%8lx) %ld, %s :%s, %d\n",
	   (long) ptr, (long) res,
	   (long) s, __PRETTY_FUNCTION__,__FILE__,__LINE__);
       fprintf(stderr,"%8lx malloc *realloc* (%8lx) %ld, %s :%s, %d\n",
	   (long) res, (long) ptr,
	   (long) s, __PRETTY_FUNCTION__,__FILE__,__LINE__);
#endif
       return res;
     }
   return NULL;
}

#ifndef DEBUG_MALLOC
static inline void *safe_strdup(const char *s) {
#else
#define safe_strdup(s) __safe_strdup(s,__PRETTY_FUNCTION__,__FILE__,__LINE__)
static inline void *__safe_strdup(const char *s, char *f, char *fi, int l) {
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

#ifdef DEBUG_MALLOC
#define free(a) (fprintf(stderr,"%8lx free, %s :%s, %d\n",(long) a,\
    __PRETTY_FUNCTION__,__FILE__,__LINE__), free(a))
#endif
