/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_color.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_COLOR_H
#define FLRN_COLOR_H

#include <stdio.h>

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
};
#endif

#define FIELD_NORMAL  0
#define FIELD_HEADER  1
#define FIELD_STATUS  2
#define FIELD_ERROR   3
#define FIELD_QUOTED  4
#define FIELD_AFF_FIN 5
#define FIELD_SIG     6
#define FIELD_FILE    7
#define FIELD_SUMMARY 8
#define FIELD_SEARCH  9

#define NROF_FIELDS  10

/* Les fonctions */

extern void free_highlights(void);
extern int parse_option_color(int /*func*/, char * /*line*/);
extern void Init_couleurs(void);
extern int Aff_color_line(int /*to_print*/, unsigned short * /*format_line*/,
    int * /*format_len*/, int /*field*/, char * /*line*/, int /*len*/,
    int /*bol*/, int /*def_color*/);
extern unsigned short *cree_chaine_mono (const char *, int, int);
extern void dump_colors_in_flrnrc (FILE *file);
extern unsigned short *Search_in_line (unsigned short * /* line */, char *out, 
                             long /* len */, regex_t * /*sreg*/);

#endif
