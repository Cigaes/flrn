/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
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

#include "config.h"
#include "flrn_config.h"
#include "flrn_macros.h"
#include "tty_keyboard.h"

#define NUMBER_OF_CONTEXTS 3

#define CONTEXT_COMMAND 0
#define CONTEXT_PAGER 1
#define CONTEXT_MENU 2

extern flrn_char *Noms_contextes[NUMBER_OF_CONTEXTS];

/* tableau des commandes */
typedef struct _flrn_assoc_key_cmd {
    struct key_entry key;
    int cmd[NUMBER_OF_CONTEXTS];
    struct _flrn_assoc_key_cmd *next;
} flrn_assoc_key_cmd;
extern flrn_assoc_key_cmd *Flcmd_rev[MAX_ASSOC_TAB];
extern Flcmd_macro_t *Flcmd_macro;
extern int Flcmd_num_macros;

/* Accès au tableau de commande */
extern void init_assoc_key_cmd();
extern int *get_adrcmd_from_key (int, struct key_entry *, int);
extern int add_cmd_for_slang_key (int, int, int);
extern int del_cmd_for_key (int,struct key_entry *);

/* les fonctions */

extern int Bind_command_new(struct key_entry *, int, flrn_char *,
#ifdef USE_SLANG_LANGUAGE
  flrn_char *,
#endif
  int, int);
extern void free_Macros(void);
extern int aff_ligne_binding(struct key_entry *, int, flrn_char ***,
	struct format_menu **);

/* Complétions */
extern int Comp_cmd_explicite(flrn_char *, size_t , Liste_Chaine *);

/* Entrée de commandes */
typedef struct command_return {
   int cmd[NUMBER_OF_CONTEXTS];
#ifdef USE_SLANG_LANGUAGE
/* Dans le cas où ce n'est pas obtenu par une macro... */
   flrn_char *fun_slang;
#endif
   flrn_char *before;
   flrn_char *after;
   int cmd_ret_flags;  /* 1 : maybe_after, 2 : keep description */
#define CMD_RET_MAYBE_AFTER 1
#define CMD_RET_KEEP_DESC 2
   flrn_char *description;
   size_t len_desc;
} Cmd_return;
extern Cmd_return une_commande;

extern int get_command(struct key_entry *, int, int, Cmd_return *);
extern int Lit_cmd_explicite(flrn_char *, int, int, Cmd_return *);
extern int Lit_cmd_key(struct key_entry *, int, int, Cmd_return *);
extern int save_command (Cmd_return *);

#endif
