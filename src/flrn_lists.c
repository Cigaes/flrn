/* gestion de listes de chaines pour flrn */

#include <stdlib.h>

#include "flrn.h"
#include "flrn_config.h"
#include "flrn_lists.h"

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

int find_in_liste(Flrn_liste *l, char *el) {
  Flrn_liste_els *a;
  a=l->first;
  while(a) {
    if (strcmp(a->ptr,el)==0) return 1;
    a=a->next;
  }
  return 0;
}

int add_to_liste(Flrn_liste *l, char *el) {
  Flrn_liste_els *a,*b;
  a=l->first;
  /* on verifie qu'il n'y est pas deja */
  if (find_in_liste(l,el)) return -1;
  b=safe_calloc(1,sizeof(Flrn_liste_els));
  b->ptr=safe_strdup(el);
  l->first=b;
  b->next=a;
  return 0;
}

int remove_from_liste(Flrn_liste *l, char *el) {
  Flrn_liste_els *a,*b;
  a=l->first;
  b=NULL;
  while(a) {
    if (strcmp(a->ptr,el)==0) {
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
  int res=0;
  if (l) { a=l->first;
    while(a) { if (fprintf(fi,"%s\n",a->ptr)<0) res=-1;
      a=a->next;}
  }
  return res;
}
