#ifndef FLRN_TAGS_H
#define FLRN_TAGS_H

#include "art_group.h"

typedef struct {
  long article_deb_key;
  Article_List *article;
  long numero;
  char *newsgroup_name; /*[MAX_NEWSGROUP_LEN + 1];*/
} Flrn_Tag;

#define NUM_SPECIAL_TAGS 256
#define MAX_TAGS 256+NUM_SPECIAL_TAGS

extern Flrn_Tag tags[MAX_TAGS];
extern int max_tags_ptr,tags_ptr;

extern void save_history(void);
extern void load_history(void);

#endif
