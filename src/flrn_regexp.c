/* Traitement de regexp pour flrn 
 * en fait, qqes fonctions qui enrichissent les regexp POSIX */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>


#include "flrn.h"

/* renvoie une chaine qui doit être matchée pour que la regexp donnée
 * matche 
 * flag=1 => possibilite de mettre des * dans la chaine
 * Attention : il faudra faire un free sur cette chaine */

char *reg_string(char *pattern, int flag) {
  char *ptr;
  char *result;
  int len;

  /* c'est trop complique pour moi */
  if (strchr(pattern,'|')) return NULL;
  result = safe_malloc(128);

  len=0;
  ptr=pattern;
  /* est-ce /^.../ ? */
  if (flag && (*ptr!='^')) result[len++]='*';

  while(*ptr && (len < 126)) {
    /* gestion des \truc */
    if (*ptr=='\\') {
      ptr++;
      /* c'est un caractère a protéger */
      if (index(".^$*[]+{}()\\",*ptr)) {result[len++]=*ptr;
	ptr ++;
	continue;
      }
      else break; /* c'est probablement un truc special */
    }
    if (*ptr=='[') { /* on traite [...] comme . et on fait gaffe a []] */
      if (len) { if (flag) result[len++]='*'; else break; }
      if (*(++ptr)) ptr++; /* on passe toujours un caractère, pour []] */
      ptr=index(ptr,']');
      if (ptr) {ptr++; continue;}
      ptr=pattern; break;
    }
    if (*ptr=='{') { /* c'est pour nous * ou + */
      if (*(++ptr)=='0') {
	if (len) len--; 
      }
      if (len) {
	if (flag) result[len++]='*'; else break;
      }
      ptr=index(ptr,'}');
      if (ptr) {ptr++; continue;}
      ptr=pattern; break;
    }
    if (index("[]{}()",*ptr)) break; /* en fait ca devrait etre que (, les
				      autres sont gérés avant */
    if (index("*?",*ptr)) {if (len) len --;}
    if (index("*?+.",*ptr)) {
      if (len) { if (flag) result[len++]='*'; else break; }
    }
    if (index("^$",*ptr)) { if (len) break; }
    if (!index(".^$*?[]+\\{}()",*ptr))
       result[len++]=*ptr;
    ptr++;
  }
  if (flag && (*ptr != '$')) result[len++]='*';
  result[len]='\0';
  if (len) return result;
  free(result);
  return NULL;
}
