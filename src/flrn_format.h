/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_format.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_FORMAT_H
#define FLRN_FLRN_FORMAT_H

/* Les fonctions */

#include <stdio.h>
#include "enc_base.h"
#include "art_group.h"

extern time_t parse_date(flrn_char * /*s*/);
extern flrn_char *vrai_nom(flrn_char * /*nom*/);
extern int str_estime_width (char *, int , size_t );
extern size_t to_make_width (char *, int , int *, int );
extern size_t to_make_width_convert (flrn_char *, int , int *, int );
extern int format_flstring (char *,flrn_char *, int, size_t, int);
extern int format_flstring_from_right (char *,flrn_char *, int, size_t, int);
extern void Copy_format (FILE * /*tmp_file*/, flrn_char * /*chaine*/,
    Article_List * /*article*/, flrn_char *, size_t);
extern flrn_char * Prepare_summary_line(Article_List * ,
	    flrn_char * , int , flrn_char * , size_t , int , int, int, int);
extern void ajoute_parsed_from (flrn_char *, flrn_char *, flrn_char *);

#endif
