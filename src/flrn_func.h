/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_func.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_FUNC_H
#define FLRN_FLRN_FUNC_H

#include "flrn.h"
#include "flrn_tags.h"
#include "group.h"
#include "art_group.h"

/* un numéro d'article, ou l'article lui-même, c'est selon */
union element {
   long number;
   Article_List *article;
};



/* Fonctions externes */

/* gestion des tags */
extern int is_tag_valid (Flrn_Tag *, Newsgroup_List **);
extern int goto_tag (int, union element *, Newsgroup_List **);
extern int put_tag (Article_List *, int);

/* gestion des groupes */
extern int get_group (Newsgroup_List **, int, char *);

#endif
