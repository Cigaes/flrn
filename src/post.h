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

#define is_modifiable(i) (((i==0) || (i>NEWSGROUPS_HEADER)) && (i!=TO_HEADER) && (i!=SENDER_HEADER))

/* Les fonctions */

extern void Get_user_address(char * /*str*/);
extern int cancel_message (Article_List * /*origine*/);
extern int post_message (Article_List * /*origine*/, char * /*name_file*/,
    int /*flag*/);

#endif
