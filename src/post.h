/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      post.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_POST_H
#define FLRN_POST_H

#include "art_group.h"
#include "enc_base.h"

#define MAX_SUJET_LEN		70
#define MAX_NB_REF		6

typedef struct Flrn_post_headers {
   flrn_char *k_header[NB_DECODED_HEADERS];
   char *k_raw_header[NB_RAW_HEADERS];
   Header_List *autres;
} Post_Headers;

#define is_modifiable(i) (((i==FOLLOWUP_TO_HEADER) || (i==EXPIRES_HEADER) || (i>=NB_UTF8_HEADERS)) && (i!=TO_HEADER) && (i!=SENDER_HEADER) && (i!=SUBJECT_HEADER))

/* Les fonctions */

extern void Get_user_address(flrn_char * /*str*/);
extern int cancel_message (Article_List * /*origine*/, int /*confirm*/);
extern int post_message (Article_List * /*origine*/, flrn_char * /*name_file*/,
    int /*flag*/);

#endif
