#ifndef FLRN_FLRN_FORMAT_H
#define FLRN_FLRN_FORMAT_H

/* Les fonctions */

#include <stdio.h>
#include "art_group.h"

extern time_t parse_date(char * /*s*/);
extern char *vrai_nom(char * /*nom*/);
extern char *local_date (char * /*date*/);
extern int str_estime_len (char *, int , int );
extern int to_make_len (char *, int , int );
extern void Copy_format (FILE * /*tmp_file*/, char * /*chaine*/,
    Article_List * /*article*/, char *, int);
extern void ajoute_parsed_from (char *, char *);

#endif
