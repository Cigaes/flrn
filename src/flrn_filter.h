/* liste chainée de regexp à matcher sur les headers */
#ifndef FLRN_FILTER_H
#define FLRN_FILTER_H

#include "flrn_lists.h"

typedef struct _flrn_condition {
  int flags;
#define FLRN_COND_REV 1
  int header_num;
  regex_t *condition;
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
  int newsgroup_regexp;  /* dit si l'on utilise une regexp ou une liste */
  union {
    regex_t *regexp;
    Flrn_liste *liste;
  } newsgroup_cond;
  long Article_deb_key; /* on reprend ce moyen rapide de validation */
  int  group_matched;
  flrn_filter * filter;
  struct _flrn_kill *next;
} flrn_kill;

#endif
