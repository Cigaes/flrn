/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_glob.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_FLRN_GLOB_H
#define FLRN_FLRN_GLOB_H

#include <pwd.h>

#include "flrn_config.h"

#ifdef CHECK_MAIL
extern char *mailbox;
#endif

/* Le nom du programme */
extern char *name_program;

/* l'erreur entre notre heure et celle du serveur */
extern int Date_offset;

/* Ligne de lecture, et la socket (pour flrn_shell.c) */
extern char tcp_line_read[MAX_READ_SIZE];
extern int tcp_fd;

/* User */
extern struct passwd *flrn_user;

/* Mode debug */
extern int debug;

/* Stockage de certains signaux */
volatile int sigwinch_received; /* Pour stoper les posts */

/* Variables globales de gestion clavier/ecran */
/* Les attributs SLtt_Char_Type */
typedef unsigned long FL_Char_Type;

extern int KeyBoard_Quit; 		/* ^C */
extern int Screen_Rows, Screen_Cols, Screen_Rows2; /* Taille de l'écran */
extern int Screen_Tab_Width;		/* Taille du tab (8) */
extern int num_help_line;

/* Gestion des scrolls */
typedef struct _File_Line_Type
{
   struct _File_Line_Type *next;
   struct _File_Line_Type *prev;
   unsigned short *data;                       
   unsigned short *data_save;
   unsigned long data_len;
}
File_Line_Type;

extern File_Line_Type *Text_scroll;

extern int overview_usable;

/* pour toute recherche : pager, menu */

#define SIZE_PATTERN_SEARCH 80
extern char pattern_search[SIZE_PATTERN_SEARCH];

#endif
