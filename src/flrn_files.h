/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_files.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_FILES_H
#define FLRN_FLRN_FILES_H

#include <stdio.h>
#include <sys/stat.h>
#include "flrn_lists.h"
#include "art_group.h"

/* les fonctions */

extern FILE *open_flrnfile (char * /*file*/,char * /*mode*/, int, time_t *);
extern void rename_flnewsfile (char * /*old_link*/,char * /*new_link*/);
extern FILE *open_postfile (char * /*file*/,char * /*mode*/, char * /* name */,
	                    int /* tmp */);
extern FILE *open_flhelpfile (char /*ext*/);
extern void Copy_article (FILE * /*dest*/, Article_List * /*article*/,
    int /*copie_head*/, char * /*avant*/, int /*with_sig*/, int /*raw*/);
extern int init_kill_file(void);
extern int newmail(char *);
extern int read_list_file(char *, Flrn_liste *);
extern int write_list_file(char *, Flrn_liste *);

#endif
