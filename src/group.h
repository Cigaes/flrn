/* flrn v 0.1                                                           */
/*              group.h             24/11/97                            */
/*                                                                      */
/* Headers pour la gestion des newsgroups.                              */

#ifndef FLRN_GROUP_H
#define FLRN_GROUP_H

#include "flrn_config.h"

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
   Range_List *read;	/* Les articles lus */
   long article_deb_key;
   Article_List *Article_deb, *Article_exte_deb;
} Newsgroup_List;


extern Newsgroup_List *Newsgroup_courant;
extern Newsgroup_List *Newsgroup_deb;

extern time_t Last_check;

#endif
