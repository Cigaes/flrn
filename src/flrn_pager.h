/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_pager.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_PAGER_H
#define FLRN_FLRN_PAGER_H

#define NB_FLCMD_PAGER 7
#define FLCMD_PAGER_UNDEF -1
extern char *Flcmds_pager[NB_FLCMD_PAGER];

#ifdef IN_FLRN_PAGER_C

char *Flcmds_pager[NB_FLCMD_PAGER] = 
   { "up",
#define FLCMD_PAGER_UP 0
     "down",
#define FLCMD_PAGER_DOWN 1
     "pgup",
#define FLCMD_PAGER_PGUP 2
     "pgdown",
#define FLCMD_PAGER_PGDOWN 3
     "quit",
#define FLCMD_PAGER_QUIT 4
     "search",
#define FLCMD_PAGER_SEARCH 5
     "nxt-search",
#define FLCMD_PAGER_NXT_SCH 6
};

#define CMD_DEF_PAGER (sizeof(Cmd_Def_Pager)/sizeof(Cmd_Def_Pager[0]))
struct cmd_predef_pager {
  int key;
  int cmd;
} Cmd_Def_Pager[] = {
  { 11, FLCMD_PAGER_UP },
  { 10, FLCMD_PAGER_DOWN },
  { '\r', FLCMD_PAGER_DOWN },
  { 2 , FLCMD_PAGER_PGUP },
  { 6 , FLCMD_PAGER_PGDOWN },
  { ' ', FLCMD_PAGER_PGDOWN },
  { 'q', FLCMD_PAGER_QUIT },
  { '/', FLCMD_PAGER_SEARCH },
};

#endif /* IN_FLRN_PAGER_C */

/* les fonctions */

extern int Page_message (int /*num_elem*/, int /*short_exit*/, int /*key*/,
        int /*act_row*/, int /*row_deb*/, char * /*exit_chars*/, char *,
	int in_wait (int));
extern int Bind_command_pager(char *, int, char *, int);
extern void init_Flcmd_pager_rev(void);
extern int ajoute_pager(char *, int);

#endif
