/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_string.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_STRING_H
#define FLRN_FLRN_STRING_H

#include "flrn_config.h"
#include "enc/enc_base.h"

typedef struct flrn_lecture_list {
    struct flrn_lecture_list *prec,*suivant;
    flrn_char lu[MAX_READ_SIZE];
    int size;
} Lecture_List;

/* Les fonctions */

extern Lecture_List *alloue_chaine(void);
extern int free_chaine(Lecture_List * /*chaine*/);
extern int ajoute_char(Lecture_List ** /*chaine*/, int /*chr*/);
extern int enleve_char(Lecture_List ** /*chaine*/);
extern char get_char(Lecture_List * /*chaine*/, int /*n*/);
extern int str_cat (Lecture_List ** /*chaine1*/, flrn_char * /*chaine*/);
extern int str_ch_cat(Lecture_List ** /*chaine1*/, Lecture_List * /*chaine2*/,
    int /*place*/, flrn_char /*chr*/);
extern void cherche_char(Lecture_List ** /*chaine*/, int * /*place*/, flrn_char /*c*/);

#endif
