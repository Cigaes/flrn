/* liste chainée de regexp à matcher sur les headers */
#ifndef FLRN_FILTER_H
#define FLRN_FILTER_H

#include "flrn_lists.h"

typedef struct _flrn_condition {
  int flags;
#define FLRN_COND_REV 1
#define FLRN_COND_STRING 2
  int header_num;
  union {
    regex_t *regex;
    char *string;
  } condition;
  struct _flrn_condition *next;
} flrn_condition;

/* liste chainée d'actions */
typedef struct _flrn_action {
  long command_num;
  char *arg;
  struct _flrn_action *next;
} flrn_action;

/* ben la totale */
typedef struct _flrn_filter {
  long cond_mask;  /* masque sur les flag de l'article */
  long cond_res;
  flrn_condition *condition;
  int flag; /* est-ce flag ou action ? */
  union {
    long flag;
    flrn_action *action;
  } action;
} flrn_filter;

/* les kill-files */
typedef struct _flrn_kill {
  /* dit si l'on utilise une regexp ou une liste */
  unsigned int newsgroup_regexp :1;
  /* dit si le groupe correspondait au dernier check*/
  unsigned int group_matched    :1;
  union {
    regex_t *regexp;
    Flrn_liste *liste;
  } newsgroup_cond;
  long Article_deb_key; /* on reprend ce moyen rapide de validation */
  			/* associé ici à group_matched */
  flrn_filter * filter;
  struct _flrn_kill *next;
} flrn_kill;

#endif
