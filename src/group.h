/* flrn v 0.1                                                           */
/*              group.h             24/11/97                            */
/*                                                                      */
/* Headers pour la gestion des newsgroups.                              */

#ifndef FLRN_GROUP_H
#define FLRN_GROUP_H

#include <sys/types.h>
#include <regex.h>
#include "flrn_config.h"
#include "flrn_menus.h"
#include "art_group.h"

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
   unsigned int flags;
#define GROUP_UNSUBSCRIBED              0x001
#define GROUP_NEW_GROUP_FLAG            0x002
#define GROUP_IN_MAIN_LIST_FLAG		0x004
#define GROUP_READONLY_FLAG		0x008
#define GROUP_READONLY_TESTED		0x010

   char name[MAX_NEWSGROUP_LEN + 1];

   char *description;   /* Description du newsgroup (quand c'est defini :( ) */
   int min;             /* Le numero de l'article minimum disponible */
   int max;		/* Le max du newsgroup */
   int not_read, important;	/* nombre d'articles à lire... */
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
extern void free_groups(int /*save_flnewsrc*/);
extern void new_groups(int /*opt_c*/);
extern Newsgroup_List *cherche_newsgroup(char * /*name*/, int /*exact*/, int);
extern Newsgroup_List *cherche_newsgroup_re (char * /*name*/, 
    regex_t /*reg*/, int );
extern Liste_Menu *menu_newsgroup_re (char * /*name*/, regex_t /*reg*/,
    int /*avec_reg*/);
extern void zap_newsgroup(Newsgroup_List * /*group*/);
extern int NoArt_non_lus(Newsgroup_List * /*group*/, int);
extern int cherche_newnews(void);
extern void add_read_article(Newsgroup_List * /*group*/, int /*numero*/);
extern char *truncate_group (char *, int);
extern void test_readonly(Newsgroup_List *);
extern void zap_group_non_courant (Newsgroup_List *);
extern void Ligne_carac_du_groupe (void *, char *, int );
extern void get_group_description(Newsgroup_List *);

#endif
