/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_xover.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */
#ifndef FLRN_FLRN_XOVER_H
#define FLRN_FLRN_XOVER_H

#include "art_group.h"

extern int va_dans_groupe(void);
extern int get_overview_fmt(void);
extern int cree_liste_xover(int /*n1*/, int /*n2*/, Article_List **,
    int *, int *);
extern int cree_liste_noxover(int /*n1*/, int /*n2*/,
    Article_List * /*article*/, int *, int *);
extern int cree_liste(int, int * );
extern int cree_liste_end(void);
extern int cree_liste_suite(int);
extern int launch_xhdr(int, int, char *);
extern int get_xhdr_line(int, char **, int, Article_List *);
extern int end_xhdr_line(void);
extern int check_in_xover(int);

#endif
