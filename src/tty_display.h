/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      tty_display.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_TTY_DISPLAY_H
#define FLRN_TTY_DISPLAY_H

#include "enc/enc_base.h"
#include "group.h"
#include "art_group.h"
#include "tty_keyboard.h"

extern int error_fin_displayed;

/* Les fonctions */

extern void Reset_screen(void);
extern int Init_screen(int);
extern void sig_winch(int );
extern int Aff_article_courant(int);
extern int Aff_grand_thread(int);
extern void Aff_newsgroup_name(int);
extern void Aff_newsgroup_courant(void);
extern void aff_try_reconnect(void);
extern void aff_end_reconnect(void);
extern void Aff_not_read_newsgroup_courant(void);
extern int Aff_summary_line(Article_List * ,int * ,
    flrn_char * , int );
extern Article_List * Menu_summary (int , int , int );
extern int Aff_fin(const flrn_char * ); 
extern int Aff_error(const flrn_char * );
extern int Aff_error_fin(const flrn_char * , int  , int );
extern int Ask_yes_no(const flrn_char *);
#if 0
extern int Aff_fin_utf8(const char * ); 
extern int Aff_error_utf8(const char * );
extern int Ask_yes_no_utf8(const char *);
extern int Aff_error_fin_utf8(const char * , int  , int );
#else
#define Aff_fin_utf8 Aff_fin
#define Aff_error_utf8 Aff_error
#define Aff_error_fin_utf8 Aff_error_fin
#define Ask_yes_no_utf8 Ask_yes_no
#endif
extern int put_string_utf8(char *);
extern int Aff_file(FILE * , char *, flrn_char *, struct key_entry *);
extern int Liste_groupe(int , flrn_char * , Newsgroup_List **);
/* extern int Aff_arbre(int,int,Article_List *, int, int, int, unsigned short **, int); */
extern void Aff_help_line(int);
extern int screen_changed_size(void);
extern int Aff_header(int, int, int, int, flrn_char *, int);

extern void Manage_progress_bar(char *, int); /* take utf8 string */

#endif
