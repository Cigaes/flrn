/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      group.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_GROUP_H
#define FLRN_GROUP_H

#include <sys/types.h>
#include "flrn_config.h"
#include "flrn_menus.h"
#include "art_group.h"
#include "compatibility.h" /* pour les regexps */

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

/* (on pourra mettre ca en option par la suite)				*/

#define	RANGE_TABLE_SIZE 10
typedef struct Range_Lu
{
   struct Range_Lu *next;
   int min[RANGE_TABLE_SIZE], max[RANGE_TABLE_SIZE];
} Range_List;

typedef struct Group_List
{
   struct Group_List *next;
   struct Group_List *prev;
   unsigned int grp_flags;
#define GROUP_UNSUBSCRIBED              0x001
#define GROUP_NEW_GROUP_FLAG            0x002
#define GROUP_IN_MAIN_LIST_FLAG		0x004
#define GROUP_READONLY_FLAG		0x008
#define GROUP_MODERATED_FLAG            0x010
#define GROUP_MODE_TESTED		0x020
/* ce mode pourrait servir à gérer des groupes partiellement construits, *
 * dans le cas de l'option max_group_size */
#define GROUP_NOT_EXHAUSTED             0x040

   flrn_char name[MAX_NEWSGROUP_LEN + 1];

   flrn_char *description;   
       /* Description du newsgroup (quand c'est defini :( ) */
   int min;             /* Le numero de l'article minimum disponible */
   int max;		/* Le max du newsgroup */
   int not_read, virtual_in_not_read, important;	/* nombre d'articles à lire... */
   			/* -1 : indéfini */
   Range_List *read;	/* Les articles lus */
   long article_deb_key;
   Article_List *Article_deb, *Article_exte_deb;
   Hash_List *(*Hash_table)[HASH_SIZE];
   Thread_List *Thread_deb;
} Newsgroup_List;


extern Newsgroup_List *Newsgroup_courant;
extern Newsgroup_List *Newsgroup_deb;

extern time_t Last_check;

/* les fonctions */

extern void init_groups(void);
extern int check_last_mod(void);
extern void free_groups(int /*save_flnewsrc*/);
extern void new_groups(int /*opt_c*/);
extern Newsgroup_List *cherche_newsgroup(flrn_char * /*name*/, int /*exact*/,
	int);
extern Newsgroup_List *cherche_newsgroup_re (flrn_char * /*name*/, 
    regex_t /*reg*/, int );
extern Liste_Menu *menu_newsgroup_re (flrn_char * /*name*/, regex_t /*reg*/,
    int /*avec_reg*/);
extern void zap_newsgroup(Newsgroup_List * /*group*/);
extern int NoArt_non_lus(Newsgroup_List * /*group*/, int);
extern int cherche_newnews(void);
extern void add_read_article(Newsgroup_List * /*group*/, int /*numero*/);
extern flrn_char *truncate_group (flrn_char *, int);
extern void test_readonly(Newsgroup_List *);
extern void zap_group_non_courant (Newsgroup_List *);
extern int Ligne_carac_du_groupe (void *, flrn_char **);
extern void get_group_description(Newsgroup_List *);
extern int calcul_order(flrn_char *, void *);
extern int calcul_order_re(flrn_char *, void *);

extern int cherche_newsgroups_base (flrn_char *, regex_t, int, 
	         int (char *, int, int, void **),
		 int (flrn_char *, void *),
		 void **);
extern int cherche_newsgroups_in_list (flrn_char *, regex_t, int,
	         int (Newsgroup_List *, int, int, void **),
		 int (flrn_char *, void *),
		 void **);

#endif
