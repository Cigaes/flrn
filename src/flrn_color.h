/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_color.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_COLOR_H
#define FLRN_COLOR_H

#include <stdio.h>
#include "flrn_slang.h"

#ifdef DEF_FILD_NAMES
static char *Field_names[] ={
  "normal",
  "header",
  "status",
  "error",
  "quoted",
  "fin",
  "sig",
  "file",
  "summary",
  "search",
  "at_mine",
  "at_other",
};
#endif

#define FIELD_NORMAL   0
#define FIELD_HEADER   1
#define FIELD_STATUS   2
#define FIELD_ERROR    3
#define FIELD_QUOTED   4
#define FIELD_AFF_FIN  5
#define FIELD_SIG      6
#define FIELD_FILE     7
#define FIELD_SUMMARY  8
#define FIELD_SEARCH   9
#define FIELD_AT_MINE 10
#define FIELD_AT_OTH  11

#define NROF_FIELDS  12

/* Les fonctions */

typedef void add_line_fun(flrn_char *, size_t , int);


extern void free_highlights(void);
extern int parse_option_color(int /*func*/, flrn_char * /*line*/);
extern void Init_couleurs(void);
extern int create_Color_line (add_line_fun, int, flrn_char *, size_t, int);
extern Colored_Char_Type *cree_chaine_mono (const char *, int, size_t,
	                                     size_t *);
extern void dump_colors_in_flrnrc (FILE *file);
extern Colored_Char_Type *Search_in_line 
                         (Colored_Char_Type * /* line */, flrn_char *out, 
                             size_t /* len */, regex_t * /*sreg*/);

#endif
