/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_comp.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_COMP_H
#define FLRN_COMP_H

typedef struct liste_de_chaine {
  char *une_chaine;
  int complet;
  struct liste_de_chaine *suivant;
} Liste_Chaine;


extern void Aff_ligne_comp_cmd (char *, int , int );

extern int Comp_generic(Liste_Chaine *, char *, int , void *,
	  int , char *(void *, int ), char *, int *, int);


extern int Comp_general_command (char *, int , int,
      int (char *, int, Liste_Chaine *), 
      void (char *, int, int));

#endif
