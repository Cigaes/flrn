/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_liste.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_LISTE_H
#define FLRN_FLRN_LISTE_H

#include "flrn_config.h"
#include "enc_base.h"
typedef struct _Flrn_liste_els {
  struct _Flrn_liste_els *next;
  flrn_char *ptr;
} Flrn_liste_els;

typedef struct _Flrn_liste {
  Flrn_liste_els *first;
} Flrn_liste;

extern Flrn_liste *alloue_liste(void);
extern int free_liste(Flrn_liste *);
extern int find_in_liste(Flrn_liste *, flrn_char *);
extern int add_to_liste(Flrn_liste *, flrn_char *);
extern int remove_from_liste(Flrn_liste *, flrn_char *);
extern int write_liste(Flrn_liste *, FILE *);

#endif
