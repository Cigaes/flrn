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

#endif
