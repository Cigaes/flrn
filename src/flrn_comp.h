/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_comp.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_COMP_H
#define FLRN_COMP_H

#include "tty_keyboard.h"
#include "enc/enc_base.h"

typedef struct liste_de_chaine {
  flrn_char *une_chaine;
  int complet;
  struct liste_de_chaine *suivant;
} Liste_Chaine;


extern void Aff_ligne_comp_cmd (flrn_char *, size_t , int );

extern int Comp_generic(Liste_Chaine *, flrn_char *, size_t , void *,
	  int , flrn_char *(void *, int ), flrn_char *, int *, int);


extern int Comp_general_command (flrn_char *, int , int,
      int (flrn_char *, size_t, Liste_Chaine *), 
      void (flrn_char *, size_t, int), struct key_entry *);

#endif
