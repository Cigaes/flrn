/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_menus.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_MENUS_H
#define FLRN_MENUS_H

#include "enc/enc_base.h"
#include "flrn_slang.h"
#include "tty_keyboard.h"

/*struct elem_ligne_menu {
   flrn_char *affbase;
   char *toaff;
   int actwidth;
}; */
struct ligne_menu {
    /* struct elem_ligne_menu *tab; */
    File_Line_Type *line;
    int flags; /* 1 : changed   2 : toggled  */
};
struct format_elem_menu {
   size_t sbase;  /* 0 : non modifiable */
   size_t stoaff; /* 0 : idem */
   int maxwidth;
   int coldeb;
   int justify; /* 0 : left.  1 : right.  2 : center */
   int flags;  /* 1 : free toaff     2 : free affbase
                * 4 : reallocate affbase */
};
struct format_menu {
   int nbelem;
   struct format_elem_menu *tab;
};

/* formats existants */
/* menu de choix d'un groupe simple (défini dans flrn_func.c) */
extern struct format_menu fmt_getgrp_menu; /* { 1, &(fmt_getgrp_menu_e[0]) } */
/* menu de liste de groupes (défini dans tty_display.c) */
extern struct format_menu fmt_grp_menu; /* { 2, &(fmt_grp_menu_e[0]) } */
/* menu des options (défini dans options.c) */
extern struct format_menu fmt_option_menu; /* { 1, &(fmt_option_menu_e[0]) } */
/* menu des keybindings (défini dans flrn_command.c) */
extern struct format_menu fmt_keybind_menu1;
                 /* { 2, &(fmt_keybind_menu1_e[0]) } */
extern struct format_menu fmt_keybind_menu2;
                 /* { 4, &(fmt_keybind_menu2_e[0]) } */
extern struct format_menu fmt_hist_menu;

/* La structure utilisée est pour l'instant celle-ci. Je ne sais pas si */
/* c'est la meilleure, mais bon... On verra bien. Peut-être rajouter un */
/* lien vers une fonction d'expression de l'objet... Ou non, le mieux   */
/* est de le passer en paramètre de la fonction de menu.	        */
typedef struct liste_menu_desc {
   struct format_menu *format;
   struct ligne_menu laligne;
   void *lobjet; /* Ceci n'est pas libéré non plus !!! */
   struct liste_menu_desc *prec, *suiv;
   int order; /* pour la comparaison lors de la création du menu */
} Liste_Menu;

#define NB_FLCMD_MENU 12
#define FLCMD_MENU_UNDEF -1
extern char *Flcmds_menu[NB_FLCMD_MENU];

#ifdef IN_FLRN_MENUS_C

#include "flrn_slang.h"

char *Flcmds_menu[NB_FLCMD_MENU] = 
   { "up",
#define FLCMD_MENU_UP 0
     "down",
#define FLCMD_MENU_DOWN 1
     "lock-up",
#define FLCMD_MENU_LUP 2
     "lock-down",
#define FLCMD_MENU_LDOWN 3
     "pgup",
#define FLCMD_MENU_PGUP 4
     "pgdown",
#define FLCMD_MENU_PGDOWN 5
     "quit",
#define FLCMD_MENU_QUIT 6
     "select",
#define FLCMD_MENU_SELECT 7
     "search",
#define FLCMD_MENU_SEARCH 8
     "nxt-search",
#define FLCMD_MENU_NXT_SCH 9
     "toggle",
#define FLCMD_MENU_TOGGLE 10
     "cmd"
#define FLCMD_MENU_CMD 11
};

#define CMD_DEF_MENU (sizeof(Cmd_Def_Menu)/sizeof(Cmd_Def_Menu[0]))
struct cmd_predef_menu {
  int key;
  int cmd;
} Cmd_Def_Menu[] = {
  { FL_KEY_UP, FLCMD_MENU_UP },
  { '-', FLCMD_MENU_UP },
  { 'p', FLCMD_MENU_UP },
  { FL_KEY_DOWN, FLCMD_MENU_DOWN },
  { '(', FLCMD_MENU_UP },
  { ')', FLCMD_MENU_DOWN },
  { 2 , FLCMD_MENU_PGUP },
  { 6 , FLCMD_MENU_PGDOWN },
  { ' ', FLCMD_MENU_PGDOWN },
  { 'q', FLCMD_MENU_QUIT },
  { '\r', FLCMD_MENU_SELECT },
  { '/', FLCMD_MENU_SEARCH },
};

#endif     /* IN_FLRN_MENUS_C */

/* les fonctions */

#include "flrn_command.h"

typedef int (*Action_on_menu)(Liste_Menu *courant, flrn_char *arg);

extern void *Menu_simple (Liste_Menu * /*debut_menu*/, Liste_Menu * /*actuel*/,
    int action(void *,flrn_char **),
    int action_select(Liste_Menu *, Liste_Menu **, flrn_char **, int *,
	Cmd_return *),
    flrn_char * /*titre*/);
extern void Libere_menu (Liste_Menu *);
extern Liste_Menu *ajoute_menu(Liste_Menu * /*base*/, struct format_menu *,
	flrn_char **, void *);
extern Liste_Menu *ajoute_menu_ordre(Liste_Menu **, struct format_menu *,
	flrn_char **, void *, int, int);
extern int Bind_command_menu(flrn_char *, struct key_entry *, flrn_char *, int);
extern void init_Flcmd_menu_rev(void);
extern int get_menu_signification (flrn_char *);
extern int distribue_action_in_menu(flrn_char *, flrn_char *,
			Liste_Menu *, Liste_Menu **, Action_on_menu);
extern void change_menu_line(Liste_Menu *, int, flrn_char *);

#endif
