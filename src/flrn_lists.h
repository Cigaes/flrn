/* flrn v 0.1                                                           */
/*              flrn_liste.h          07/98                             */
/*                                                                      */
/* Headers pour flrn_liste.c                                            */

#ifndef FLRN_FLRN_LISTE_H
#define FLRN_FLRN_LISTE_H

#include "flrn_config.h"
typedef struct _Flrn_liste_els {
  struct _Flrn_liste_els *next;
  char *ptr;
} Flrn_liste_els;

typedef struct _Flrn_liste {
  Flrn_liste_els *first;
} Flrn_liste;

extern Flrn_liste *alloue_liste(void);
extern int free_liste(Flrn_liste *);
extern int find_in_liste(Flrn_liste *, char *);
extern int add_to_liste(Flrn_liste *, char *);
extern int remove_from_liste(Flrn_liste *, char *);

#endif
