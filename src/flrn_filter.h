/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_filter.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* liste chain�e de regexp � matcher sur les headers */
#ifndef FLRN_FILTER_H
#define FLRN_FILTER_H

#include <stdio.h>

#include "art_group.h"
#include "flrn_lists.h"
#include "flrn_command.h"

typedef struct _flrn_condition {
  int cond_flags;
#define FLRN_COND_REV 1
#define FLRN_COND_STRING 2
#define FLRN_COND_U_HEAD 4
  union {
    int header_num;
    flrn_char *header_str;
  } header_ns;
  union {
    regex_t *regex;
    flrn_char *string;
  } condition;
  struct _flrn_condition *next;
} flrn_condition;

/* liste chain�e d'actions */
typedef struct _flrn_action {
  Cmd_return *command;
  struct _flrn_action *next;
} flrn_action;

/* ben la totale */
typedef struct _flrn_filter {
  long cond_mask;  /* masque sur les flag de l'article */
  long cond_res;
  time_t date_before;
  time_t date_after;
  flrn_condition *condition;
  int fltr_flag; /* est-ce flag ou action ? */
  union {
    long art_flag;
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
  			/* associ� ici � group_matched */
  flrn_filter * filter;
  struct _flrn_kill *next;
} flrn_kill;

/* Les fonctions */

extern int check_article(Article_List *, flrn_filter *, int);
extern int check_article_list(Article_List *, flrn_filter *, int ,int, int);
extern flrn_filter *new_filter(void);
extern int parse_filter(flrn_char *, flrn_filter *);
extern int parse_filter_flags(flrn_char *, flrn_filter *);
extern int parse_filter_action(flrn_char *, flrn_filter *);
extern void free_filter(flrn_filter *);
extern int parse_kill_file(FILE *);
extern void apply_kill(int);
extern void check_kill_article(Article_List *, int );
extern void check_kill_article_in_list(Article_List *, int, int, int );
extern int add_to_main_list(flrn_char *);
extern int remove_from_main_list(flrn_char *);
extern void free_kill();
extern int in_main_list(flrn_char *);
extern int parse_flags(flrn_char *, int *, int *);

#endif
