/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_command.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_COMMAND_H
#define FLRN_COMMAND_H

#include "flrn_config.h"
#include "flrn_macros.h"

#define NUMBER_OF_CONTEXTS 3

#define CONTEXT_COMMAND 0
#define CONTEXT_PAGER 1
#define CONTEXT_MENU 2

extern char *Noms_contextes[NUMBER_OF_CONTEXTS];

/* tableau des commandes */
extern int Flcmd_rev[NUMBER_OF_CONTEXTS][MAX_FL_KEY];
extern Flcmd_macro_t *Flcmd_macro;
extern int Flcmd_num_macros;

/* les fonctions */

extern int Bind_command_new(int, int, char *, int, int);
extern void free_Macros(void);
extern int aff_ligne_binding(int, int, char *, int);

/* Compl�tions */
extern int Comp_cmd_explicite(char *, int , Liste_Chaine *);

/* Entr�e de commandes */
typedef struct command_return {
   int cmd[NUMBER_OF_CONTEXTS];
   char *before;
   char *after;
   int maybe_after;
} Cmd_return;
extern Cmd_return une_commande;

extern int get_command(int, int, int, Cmd_return *);
extern int Lit_cmd_explicite(char *, int, int, Cmd_return *la_commande);

#endif
