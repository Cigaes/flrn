#ifndef FLRN_INTER_H
#define FLRN_INTER_H

#include "art_group.h"
#include "flrn_config.h"

typedef struct command_desc {
   char *nom;
   int  key;
   int  key_nm;
   int  flags; /* 1 : chaine de caractère a prendre
   		  2 : possibilites d'articles       
                  4 : demande une chaine dans l'ancien mode 
                  8 : demande une chaine dans le nouveau mode aussi
		  16: demande à avoir un groupe valide */
#define CMD_NEED_GROUP 16
   int (*appel)(int);
} Flcmd;

typedef struct {
  long article_deb_key;
  Article_List *article;
  long numero;
  char *newsgroup_name; /*[MAX_NEWSGROUP_LEN + 1];*/
} Flrn_Tag;
#endif

