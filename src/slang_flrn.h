/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2000  Damien Massé et Joël-Yann Fourré
 *
 *      slang_flrn.h : entêtes pour langage de script slang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_SLANG_FLRN_H
#define FLRN_SLANG_FLRN_H

#include "flrn_config.h"

#ifdef USE_SLANG_LANGUAGE

#include "art_group.h"
#include "group.h"
#include "slang.h"

extern int flrn_SLang_inited;

extern int flrn_init_SLang(void);
extern int source_SLang_string(char *, char **);
extern int source_SLang_file (char *);
extern int change_SLang_Error_Hook(int);
extern SLang_Name_Type *Parse_fun_slang (char *, int *);
extern int Parse_type_fun_slang(char *);
extern int Push_article_on_stack (Article_List *, Newsgroup_List *);
extern int Push_newsgroup_on_stack (Newsgroup_List *);

extern int try_hook_newsgroup_string (char *, Newsgroup_List *, char **);


#endif

#endif
