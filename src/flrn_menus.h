#ifndef FLRN_MENUS_H
#define FLRN_MENUS_H

/* La structure utilisée est pour l'instant celle-ci. Je ne sais pas si */
/* c'est la meilleure, mais bon... On verra bien. Peut-être rajouter un */
/* lien vers une fonction d'expression de l'objet... Ou non, le mieux   */
/* est de le passer en paramètre de la fonction de menu.	        */
typedef struct liste_menu_desc {
   char *nom;  /* En read-only, peut être un pointeur vers n'importe ou */
   void *lobjet; /* Ceci n'est pas libéré non plus !!! */
   struct liste_menu_desc *prec, *suiv;
} Liste_Menu;

#ifdef IN_FLRN_MENUS_C

#include "flrn_slang.h"

#define NB_FLCMD_MENU 6
#define FLCMD_MENU_UNDEF -1

char *Flcmds_menu[NB_FLCMD_MENU] = 
   { "up",
#define FLCMD_MENU_UP 0
     "down",
#define FLCMD_MENU_DOWN 1
     "pgup",
#define FLCMD_MENU_PGUP 2
     "pgdown",
#define FLCMD_MENU_PGDOWN 3
     "quit",
#define FLCMD_MENU_QUIT 4
     "select",
#define FLCMD_MENU_SELECT 5
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
};

#endif

#endif
