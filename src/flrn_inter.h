/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_inter.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_INTER_H
#define FLRN_INTER_H

#include "flrn_comp.h"

typedef struct command_desc {
   char *nom;
   int  key;
   int  key_nm;
   int  flags; /* 1 : chaine de caractère a prendre
   		  2 : possibilites d'articles       
                  4 : demande une chaine dans le mode forum (numéros ou autre)
                  8 : demande une chaine dans le nouveau mode
		  16: si il y a des tokens, le dernier doit etre la chaine
		  32: demande à avoir un groupe valide */
#define CMD_NEED_GROUP 32
   int (*appel)(int);
   int (*comp)(char *, int, Liste_Chaine *);
} Flcmd;


/* Les fonctions : */

extern int fonction_to_number(char *);
extern int call_func(int, char *);
extern int loop(char * /*opt*/);
extern int aff_opt_c(char *);
extern void init_Flcmd_rev(void);
extern int Bind_command_explicite(char * /*nom*/, int /*key*/, char *, int);
extern void save_etat_loop(void);
extern void restore_etat_loop(void);

#endif
