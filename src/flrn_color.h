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

#define NROF_FIELDS   9

/* Les fonctions */

extern void free_highlights(void);
extern int parse_option_color(int /*func*/, char * /*line*/);
extern void Init_couleurs(void);
extern int Aff_color_line(int /*to_print*/, unsigned short * /*format_line*/,
    int * /*format_len*/, int /*field*/, char * /*line*/, int /*len*/,
    int /*bol*/, int /*def_color*/);
extern unsigned short *cree_chaine_mono (const char *, int, int);
extern void dump_colors_in_flrnrc (FILE *file);

#endif
