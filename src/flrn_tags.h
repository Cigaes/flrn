/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_tags.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_TAGS_H
#define FLRN_TAGS_H

#include "art_group.h"
#include "tty_keyboard.h"

typedef struct {
  long article_deb_key;
  Article_List *article;
  long numero;
  flrn_char *newsgroup_name; /*[MAX_NEWSGROUP_LEN + 1];*/
} Flrn_Tag;

typedef struct flrn_special_tag {
  struct key_entry key;
  Flrn_Tag tag;
  struct flrn_special_tag *next;
} Flrn_Special_Tag;

#define MAX_TAGS 256

extern Flrn_Tag tags[MAX_TAGS];
extern int max_tags_ptr,tags_ptr;
extern Flrn_Special_Tag *special_tags;

Flrn_Tag *add_special_tags(struct key_entry *, Flrn_Tag *);
Flrn_Tag *get_special_tags(struct key_entry *);
extern void correct_article_in_tags(Article_List *, Article_List *);

extern void save_history(void);
extern void load_history(void);

#endif
