/* flrn v 0.1                                                           */
/*              post.h              25/11/97                            */
/*                                                                      */
/* Headers pour post.c (si !).						*/

#ifndef FLRN_POST_H
#define FLRN_POST_H

#include "flrn_string.h"
#include "art_group.h"

#define MAX_SUJET_LEN		70
#define MAX_NB_REF		6

typedef struct Flrn_post_headers {
   char *k_header[NB_KNOWN_HEADERS];
   Header_List *autres;
} Post_Headers;

#define is_modifiable(i) ((i>NEWSGROUPS_HEADER) && (i!=TO_HEADER))

#endif
