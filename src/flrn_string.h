/* flrn v 0.1                                                           */
/*              flrn_string.h          03/12/97                         */
/*                                                                      */
/* Headers pour flrn_string.c                                           */

#ifndef FLRN_FLRN_STRING_H
#define FLRN_FLRN_STRING_H

#include "flrn_config.h"

typedef struct flrn_lecture_list {
    struct flrn_lecture_list *prec,*suivant;
    char lu[MAX_READ_SIZE];
    int size;
} Lecture_List;

/* Les fonctions */

extern Lecture_List *alloue_chaine(void);
extern int free_chaine(Lecture_List * /*chaine*/);
extern int ajoute_char(Lecture_List ** /*chaine*/, int /*chr*/);
extern int enleve_char(Lecture_List ** /*chaine*/);
extern char get_char(Lecture_List * /*chaine*/, int /*n*/);
extern int str_cat (Lecture_List ** /*chaine1*/, char * /*chaine*/);
extern int str_ch_cat(Lecture_List ** /*chaine1*/, Lecture_List * /*chaine2*/,
    int /*place*/, char /*chr*/);
extern void cherche_char(Lecture_List ** /*chaine*/, int * /*place*/, char /*c*/);

#endif
