/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_lists.c : gestion de listes de chaines
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* gestion de listes de chaines pour flrn */

#include <stdlib.h>

#include "flrn.h"
#include "flrn_config.h"
#include "flrn_lists.h"
#include "enc_strings.h"

static UNUSED char rcsid[]="$Id$";

Flrn_liste *alloue_liste() {
  return (Flrn_liste *) safe_calloc(1,sizeof(Flrn_liste));
}

static int free_liste_el(Flrn_liste_els *e) {
    free(e->ptr);
    free(e);
    return 0;
}

int free_liste(Flrn_liste *l) {
  Flrn_liste_els *a=l->first, *b;
  while(a) {
    b=a->next;
    free_liste_el(a);
    a=b;
  }
  free(l);
  return 0;
}

int find_in_liste(Flrn_liste *l, flrn_char *el) {
  Flrn_liste_els *a;
  a=l->first;
  while(a) {
    if (fl_strcmp(a->ptr,el)==0) return 1;
    a=a->next;
  }
  return 0;
}

int add_to_liste(Flrn_liste *l, flrn_char *el) {
  Flrn_liste_els *a,*b;
  a=l->first;
  /* on verifie qu'il n'y est pas deja */
  if (find_in_liste(l,el)) return -1;
  b=safe_calloc(1,sizeof(Flrn_liste_els));
  b->ptr=safe_flstrdup(el);
  l->first=b;
  b->next=a;
  return 0;
}

int remove_from_liste(Flrn_liste *l, flrn_char *el) {
  Flrn_liste_els *a,*b;
  a=l->first;
  b=NULL;
  while(a) {
    if (fl_strcmp(a->ptr,el)==0) {
      if (b) b->next=a->next;
      else l->first=a->next;
      free_liste_el(a);
      return 0;
    }
    b=a;
    a=a->next;
  }
  return -1;
}

int write_liste(Flrn_liste *l, FILE *fi) {
  Flrn_liste_els *a;
  char *trad;
  int res=0, rc;
  if (l) { a=l->first;
    while(a) { 
	rc=conversion_to_utf8(a->ptr,&trad,0,(size_t)(-1));
        if (fprintf(fi,"%s\n",trad)<0) res=-1;
	if (rc==0) free(trad);
        a=a->next;}
  }
  return res;
}
