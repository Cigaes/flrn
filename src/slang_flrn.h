/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2000  Damien Massé et Joël-Yann Fourré
 *
 *      slang_flrn.h : entêtes pour langage de script slang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_SLANG_FLRN_H
#define FLRN_SLANG_FLRN_H

#include "flrn_config.h"

#ifdef USE_SLANG_LANGUAGE

extern int flrn_SLang_inited;

extern int flrn_init_SLang(void);
extern int source_SLang_string(char *, char **);

#endif

#endif
