/* flrn v 0.1                                                           */
/*              art_group.h         25/11/97                            */
/*                                                                      */
/* Headers pour la gestion d'une liste d'article au sein du newsgroup.  */

#ifndef FLRN_ART_GROUP_H
#define FLRN_ART_GROUP_H

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/times.h>

typedef struct Flrn_known_headers {
   char *header;
   int header_len;
} Known_Headers;

extern const Known_Headers Headers[]; 

#define UNKNOWN_HEADER -2
#define NULL_HEADER -1
#define FROM_HEADER 0
#define REFERENCES_HEADER 1
#define SUBJECT_HEADER 2
#define DATE_HEADER 3
#define NEWSGROUPS_HEADER 4
#define FOLLOWUP_TO_HEADER 5
#define ORGANIZATION_HEADER 6
#define LINES_HEADER 7
#define SENDER_HEADER 8
#define REPLY_TO_HEADER 9
#define EXPIRES_HEADER 10
#define X_NEWSREADER_HEADER 11
#define TO_HEADER 12
#define CC_HEADER 13
#define BCC_HEADER 14
#define XREF_HEADER 15
#define MESSAGE_ID_HEADER 16
#define X_CENSORSHIP 17


#define NB_KNOWN_HEADERS 18

typedef struct Flrn_art_header
{
   char *k_headers[NB_KNOWN_HEADERS];
   int  k_headers_checked[NB_KNOWN_HEADERS];
   char *reponse_a;
   int  nb_lines, all_headers, reponse_a_checked;
   time_t date_gmt;
   /*
   int  avec_xover;
   */
} Article_Header;

struct Flrn_thread_list;
struct Flrn_hash_list;

typedef struct Flrn_art_list
{
   struct Flrn_art_list *next, *prev, *pere;
   struct Flrn_art_list *prem_fils, *frere_prev, *frere_suiv;
   int numero;
   int parent;


#define FLAG_READ 1
   /* Utilise en interne, il faut le mettre a 0 avant de l'utiliser */
#define FLAG_DISPLAYED 2
   /* utilise en interne par distribue
    * il faut le mettre a 0 apres l'avoir utilise */
#define FLAG_TAGGED 4
   /* Flag a utiliser avec distribue_action
    * utilise par le resume
    * Il doit en etat normal etre a 0 */
#define FLAG_ACTION 8
   /* Flag disant d'appliquer le kill_file */
#define FLAG_NEW   16
   /* Flag disant d'ignorer le message */
#define FLAG_KILLED   32
   int flag;  

   char *msgid;
   Article_Header *headers;
   struct Flrn_thread_list *thread;
} Article_List;

typedef struct Flrn_thread_list {
   int non_lu, number;
   struct Flrn_hash_list *premier_hash;
   struct Flrn_thread_list *next_thread;
} Thread_List;

typedef struct Flrn_hash_list {
   Thread_List *thread;
   Article_List *article;
   char *msgid;
   struct Flrn_hash_list *next_in_thread; /* truc dans le désordre */
   struct Flrn_hash_list *prev_hash;
} Hash_List;

extern Article_List *Article_courant;
extern Article_List *Article_deb;
/* entier incrémenté à chaque fois que la liste d'article est blastée */
extern long Article_deb_key;
extern Article_List *Article_exte_deb;
extern Article_List Article_bidon;
extern time_t Date_groupe;

#define HASH_SIZE 1024
extern Hash_List *(*Hash_table)[HASH_SIZE];
extern Thread_List *Thread_deb;


/* pour les autre headers */
typedef struct Flrn_header_list {
   struct Flrn_header_list *next;
   char *header;
   int num_af; /* utile lors de l'affichage */
} Header_List;

typedef struct Flrn_last_headers_cmd {
  Article_List *Article_vu;
  Header_List *headers;
} Last_headers_cmd;

extern Last_headers_cmd Last_head_cmd;

#endif
