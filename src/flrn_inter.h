/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_inter.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_INTER_H
#define FLRN_INTER_H

#include "flrn_comp.h"
#include "group.h"

typedef struct command_desc {
   char *nom;
   int  key;
   int  key_nm;
   int  cmd_flags; /* 1 : chaine de caractère a prendre
   		  2 : possibilites d'articles       
                  4 : demande une chaine dans le mode forum (numéros ou autre)
                  8 : demande une chaine dans le nouveau mode
		  16: si il y a des tokens, le dernier doit etre la chaine
		  32: demande à avoir un groupe valide */
#define CMD_TAKE_STRING 1
#define CMD_TAKE_ARTICLES 2
#define CMD_TAKE_BOTH 3
#define CMD_STR_FORUM 4
#define CMD_STR_NOFORUM 8
#define CMD_STR_ALWAYS 12
#define CMD_LAST_IS_STR 16
#define CMD_PARSE_FILTER 19
#define CMD_STRING_AND_ARTICLE 31
#define CMD_NEED_GROUP 32
   int (*appel)(int);
   int (*comp)(flrn_char *, size_t, Liste_Chaine *);
} Flcmd;


/* Les fonctions : */

extern int get_and_execute_command (int, Cmd_return *, int );
extern int loop(flrn_char * );
extern int aff_opt_c(flrn_char *, int);
extern void init_Flcmd_rev(void);
extern int Bind_command_explicite(flrn_char *, struct key_entry *,
	flrn_char *, int);
extern void save_etat_loop(void);
extern void restore_etat_loop(void);

#ifdef USE_SLANG_LANGUAGE
extern int side_effect_of_slang_command (int, Newsgroup_List *,
	         int , int , int );
#endif

#endif
