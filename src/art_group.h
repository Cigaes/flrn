/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *	art_group.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_ART_GROUP_H
#define FLRN_ART_GROUP_H

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/times.h>

#include "enc/enc_base.h"

typedef struct Flrn_known_headers {
   char *header;
   int header_len;
} Known_Headers;

extern const Known_Headers Headers[]; 

#define UNKNOWN_HEADER -2
#define NULL_HEADER -1
#define NEWSGROUPS_HEADER 0
#define FOLLOWUP_TO_HEADER 1
#define DATE_HEADER 2
#define LINES_HEADER 3
#define EXPIRES_HEADER 4
#define XREF_HEADER 5
#define X_TRACE_HEADER 6

#define NB_UTF8_HEADERS 7

#define FROM_HEADER 7
#define SUBJECT_HEADER 8
#define ORGANIZATION_HEADER 9
#define SENDER_HEADER 10
#define REPLY_TO_HEADER 11
#define X_NEWSREADER_HEADER 12
#define TO_HEADER 13
#define CC_HEADER 14
#define BCC_HEADER 15
#define X_CENSORSHIP 16

#define NB_DECODED_HEADERS 17

#define REFERENCES_HEADER 0
#define MESSAGE_ID_HEADER 1

#define NB_RAW_HEADERS 2
#define NB_KNOWN_HEADERS (NB_DECODED_HEADERS+NB_RAW_HEADERS)

typedef struct Flrn_art_header
{
   flrn_char *k_headers[NB_DECODED_HEADERS];
   char *k_raw_headers[NB_RAW_HEADERS];
   int  k_headers_checked[NB_KNOWN_HEADERS];
   flrn_char *reponse_a;
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


/* flags externes : sont directement liés à l'utilisateur */
#define FLAG_READ		0x0001
#define FLAG_KILLED		0x0002
#define FLAG_IMPORTANT		0x0004
#define FLAG_IS_SELECTED	0x0008
#define FLAG_TOREAD		0x0010
#define FLAG_WILL_BE_OMITTED	0x0020
#define FLAG_NEW   		0x0040
/* flags internes : pour certaines commandes */
/* A mettre à 0 avant de l'utiliser : */
   /* utilisé par thread_action et gthread_action */
#define FLAG_INTERNE1		0x0100
/* A mettre à 0 après l'avoir utilisé : */
   /* utilise par parent_action */
#define FLAG_INTERNE2		0x0200
   /* Flag a utiliser avec distribue_action
    * utilise par le resume
    * Il doit en etat normal etre a 0 */
   /* Ce flag est aussi utilise pour la selection */
#define FLAG_SUMMARY		0x0400
   /* flag utilisé en kill-file */
   /* en état normal à 0 	*/
#define FLAG_TMP_KILL		0x0800
   int art_flags;

   char *msgid;
   Article_Header *headers;
   struct Flrn_thread_list *thread;
} Article_List;

typedef struct Flrn_thread_list {
   int non_lu, number, thr_flags;
#define FLAG_THREAD_SELECTED 1
#define FLAG_THREAD_READ 2
#define FLAG_THREAD_UNREAD 4
#define FLAG_THREAD_IMPORTANT 8
#define FLAG_THREAD_BESELECTED 16
/* doit être remis à 0 en FIN d'opération */
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
   flrn_char *header_head;
   flrn_char *header_body;
   int num_af; /* utile lors de l'affichage */
} Header_List;

typedef struct Flrn_last_headers_cmd {
  Article_List *Article_vu;
  Header_List *headers;
} Last_headers_cmd;

extern Last_headers_cmd Last_head_cmd;

/* Les fonctions */

extern int cree_liens(void);
extern Article_Header *cree_header(Article_List * /*article*/,
    int /*rech_pere*/, int /*others*/, int);
extern void ajoute_reponse_a(Article_List * /*article*/);
extern Article_List *ajoute_message(char * /*msgid*/, int * /*should_retry*/);
extern Article_List *ajoute_message_par_num(int , int);
extern void detruit_liste(int);
extern void libere_liste(void);
extern Article_List *next_in_thread(Article_List * /*start*/,
    long , int * /*level*/,
    int /*deb*/, int /*fin*/, int /*set*/, int);
extern Article_List *root_of_thread(Article_List * /*article*/, int );
extern void article_read(Article_List * /*article*/);
extern int Recherche_article (int /*num*/, Article_List ** /*retour*/,
    int );
extern Article_Header *new_header(void);
extern int Est_proprietaire(Article_List * /*article*/, int);
extern void apply_kill_file(int, int);
extern Article_List *cousin_prev(Article_List *article);
extern Article_List *cousin_next(Article_List *article);
extern void free_one_article(Article_List *, int);

struct Group_List;

extern void recherche_article_par_msgid(Article_List **, struct Group_List **,
                                                char *);
extern flrn_char *get_one_header(Article_List *, struct Group_List *, 
	flrn_char *, int *);

#endif
