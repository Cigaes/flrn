/* flrn v 0.1 : variables globales */

#ifndef FLRN_FLRN_GLOB_H
#define FLRN_FLRN_GLOB_H

#include <pwd.h>

#include "flrn_config.h"

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
extern int Screen_Rows, Screen_Cols;	/* Taille de l'écran */
extern int Screen_Tab_Width;		/* Taille du tab (8) */

/* Gestion des scrolls */
typedef struct _File_Line_Type
{
   struct _File_Line_Type *next;
   struct _File_Line_Type *prev;
   unsigned short *data;                       
   unsigned long data_len;
}
File_Line_Type;

extern File_Line_Type *Text_scroll;

extern int overview_usable;

#endif
