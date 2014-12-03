/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_regexp.c : regexps pour flrn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* Traitement de regexp pour flrn 
 * en fait, qqes fonctions qui enrichissent les regexp POSIX */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>


#include "flrn.h"
#include "flrn_regexp.h"
#include "enc_base.h"

static UNUSED char rcsid[]="$Id$";

/* renvoie une chaine qui doit être matchée pour que la regexp donnée
 * matche 
 * flag=1 => possibilite de mettre des * dans la chaine
 * Attention : il faudra faire un free sur cette chaine */

flrn_char *reg_string(flrn_char *pattern, int flag) {
  flrn_char *ptr;
  flrn_char *result;
  int len;

  /* c'est trop complique pour moi */
  if (fl_strchr(pattern,fl_static('|'))) return NULL;
  result = safe_malloc(128);

  len=0;
  ptr=pattern;
  /* est-ce /^.../ ? */
  if (flag && (*ptr!=fl_static('^'))) result[len++]=fl_static('*');

  while(*ptr && (len < 126)) {
    /* gestion des \truc */
    if (*ptr==fl_static('\\')) {
      ptr++;
      /* c'est un caractère a protéger */
      if (fl_strchr(fl_static(".^$*[]+{}()\\"),*ptr)) 
      { result[len++]=*(ptr++);
	continue;
      }
      else break; /* c'est probablement un truc special */
    }
    if (*ptr==fl_static('[')) { 
	/* on traite [...] comme . et on fait gaffe a []] */
      if (len) { if (flag) result[len++]=fl_static('*'); else break; }
      if (*(++ptr)) ptr++; /* on passe toujours un caractère, pour []] */
      ptr=fl_strchr(ptr,fl_static(']'));
      if (ptr) {ptr++; continue;}
      ptr=pattern; break;
    }
    if (*ptr==fl_static('{')) { /* c'est pour nous * ou + */
      if (*(++ptr)==fl_static('0')) {
	if (len) len--; 
      }
      if (len) {
	if (flag) result[len++]=fl_static('*'); else break;
      }
      ptr=fl_strchr(ptr,fl_static('}'));
      if (ptr) {ptr++; continue;}
      ptr=pattern; break;
    }
    if (fl_strchr(fl_static("[]{}()"),*ptr))
	break; /* en fait ca devrait etre que (, les
				      autres sont gérés avant */
    if (fl_strchr(fl_static("*?"),*ptr)) {if (len) len --;}
    if (fl_strchr(fl_static("*?+."),*ptr)) {
      if (len) { if (flag) result[len++]=fl_static('*'); else break; }
    }
    if (fl_strchr(fl_static("^$"),*ptr)) { if (len) break; }
    if (!fl_strchr(fl_static(".^$*?[]+\\{}()"),*ptr))
       result[len++]=*ptr;
    ptr++;
  }
  if (flag && (*ptr != fl_static('$'))) result[len++]=fl_static('*');
  result[len]=fl_static('\0');
  if (len) return result;
  free(result);
  return NULL;
}
