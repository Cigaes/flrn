#ifndef FLRN_TTY_DISPLAY_H
#define FLRN_TTY_DISPLAY_H

#include "group.h"
#include "art_group.h"

extern int error_fin_displayed;

/* Les fonctions */

extern void Reset_screen(void);
extern int Init_screen(int);
extern void sig_winch(int );
extern int Aff_article_courant(int);
extern int Aff_grand_thread(int);
extern void Aff_newsgroup_name(int /* erase_scr */);
extern void Aff_newsgroup_courant(void);
extern void aff_try_reconnect(void);
extern void aff_end_reconnect(void);
extern void Aff_not_read_newsgroup_courant(void);
extern char * Prepare_summary_line(Article_List * /*article*/,
    char * /*prev_subject*/, int /*level*/, char * /*buf*/, int /*buflen*/, int);
extern int Aff_summary_line(Article_List * /*article*/,int * /*row*/,
    char * /*prev_subject*/, int /*level*/);
extern Article_List * Menu_summary (int /*deb*/, int /*fin*/, int /*thread*/);
extern int Aff_fin(const char * /*str*/);
extern int Aff_error(const char * /*str*/);
extern int Aff_error_fin(const char * /*str*/, int /*s_beep*/ , int );
extern int Aff_file(FILE * /*file*/, char *, char *);
extern int Liste_groupe(int /*n*/, char * /*mat*/, Newsgroup_List **);
extern int Aff_arbre(int,int,Article_List *, int, int, int, unsigned char **, int);

#endif
