/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_config.h : quelques define pour le programme
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_FLRN_CONFIG_H
#define FLRN_FLRN_CONFIG_H

#include "pathdef.h"

/* L'utilisateur peut modifier la valeurs qui suivent jusqu'au
   "-- ne rien modifier ensuite --" */

/********************************************************************/
/******** Options du GNKSA (Brevet du Bon Citoyen de Usenet) ********/
/********************************************************************/
/* Les options qui suivent (jusqu'à USE_GNKSA) sont destinées à     */
/* montrer que flrn satisfait toutes les recommandations, moyennant */
/* quelques options, du GNKSA (en français). Tout ceci devrait      */
/* passer à terme dans le config.h.in, pour le configure.           */
/* Pour l'instant, presque rien n'est défini par défaut 	    */
/* on peut définir tout à la fois en definissant USE_GKNSA          */
/********************************************************************/
/* NdDamien : les fonctions commentées ne sont pas implémentées :-) */

/* Cette option change la configuration par défaut pour afficher
 * les entêtes standards (From, Subject, Newsgroups, Followup-To,
 * Reply-To */
/* #undef GNKSA_DISPLAY_HEADERS */

/* Cette option change le nom des commandes standards pour avoir
 * une terminologie "GNKSA" (française) pour les posts et cancels */
/* #undef GNKSA_STANDARD_COMMANDS */

/* Cette option vérifie la gueule des posts :
   _ prévient si un large crosspost est fait.
   _ vérifie la tête du sujet et refuse de poster avec un sujet vide
   _ vérifie la tête du from
   _ vérifie la taille des lignes, de la signature
   _ refuse les articles vides, les articles de quotés 
   _ remplace les tabulations par des espaces */
/* #undef GNKSA_ANALYSE_POSTS */

/* Cette option met un champ Newsgroups conforme au GNKSA (il n'y est
 * pas par défaut car je n'aime pas cette feature */
/* #undef GNKSA_NEWSGROUPS_HEADER */

/* Cette option vérifie les references d'un article posté, en met le
 * maximum dans la limite de 998 caractères */
/* #undef GNKSA_REFERENCES_HEADER */

/* Cette option (qui devrait ne pas en être une) permet une meilleure
 * gestion des réponses (point numéro 9 du GNKSA). Elle sera par défaut
 * dès que possible, mais ces lignes permettent de m'en souvenir :-) */
/* #undef GNKSA_POINT_9 */

/* Le point 10e du GNKSA est difficile à suivre pour une interface
 * texte... je ne peux le faire correctement. Deja offrir l'option
 * de quote de la signature */
/* #undef GNKSA_QUOTE_SIGNATURE */

/* Cette option ajoute la fonction de "rewrapping" du texte quoté, et
 * propose un "rewrapping" du texte après édition */
/* #undef GNKSA_REWRAP_TEXT */

/* Cette option active les autres :-) */
#undef USE_GNKSA
#ifdef USE_GNKSA
  #define GNKSA_DISPLAY_HEADERS
  #define GNKSA_STANDARD_COMMANDS
  #define GNKSA_ANALYSE_POSTS
  #define GNKSA_NEWSGROUPS_HEADER
  #define GNKSA_REFERENCES_HEADER
  #define GNKSA_QUOTE_SIGNATURE
  #define GNKSA_REWRAP_TEXT
#endif

/* - fin des options du GNKSA - */

/* Cette option supprime l'éditeur interne de flrn... Elle pose problème
 * si l'utilisateur ne réussit pas à définir un bon éditeur. Bien entendu
 * l'option auto_edit est alors supprimée. (non implanté encore) */
/* #undef NO_INTERN_EDITOR */

/* Répertoire où chercher les fichiers de config chez l'utilisateur */
#define DEFAULT_DIR_FILE        ".flrn"

/* Nom du fichier de config normalement présent chez l'utilisateur */
#define DEFAULT_CONFIG_FILE     ".flrnrc"

/* Nom du fichier de config par défaut si l'utilisateur n'a pas de .flrnrc :
     flrn charge ce fichier dans le répertoire $datadir/flrn du Makefile 
     (par défaut /usr/local/share/flrn) */
#define DEFAULT_CONFIG_SYS_FILE "flrnrc"

/* Nom du fichier qui contient les messages lus par l'utilisateur 
   (équivalent du .newsrc) */
#define DEFAULT_FLNEWS_FILE     ".flnewsrc"

/* Nom du fichier temporaire utilisé pour les posts */
#define TMP_POST_FILE           ".article"

/* Nom du fichier temporaire utilisé pour obtenir la sortie standard
   d'un programme (lancé via \shin par exemple) */
#define TMP_PIPE_FILE		".pipe_flrn"

/* Lieu où sauver un message qui a été refusé par le serveur */
#define TMP_REJECT_POST_FILE    "dead.article"

/* Défini à 1 si on veut que flrn vérifie la présence de nouveaux mails */
#define CHECK_MAIL	1
#ifdef CHECK_MAIL
/* Répertoire où flrn suppose que se trouve la mailbox de l'utilisateur. */
#define DEFAULT_MAIL_PATH "/var/spool/mail" 
#endif

/* Défini à 1 si on veut pouvoir utiliser les jeux de caractères */
#define WITH_CHARACTER_SETS 1

/* Options de lancement de sendmail (le lieu où trouver sendmail est 
   défini via configure dans config.h */
#define MAILER_CMD SENDMAIL " -U -t"


/* -- ne rien modifier ensuite, normalement -- */

#define MAX_PATH_LEN 	1024
  /* en fait fichier temporaire utilisé partout */
#define DEFAULT_HELP_DIRECTORY	PATH_HELP_DIRECTORY /* defined in pathdef.h */
#define DEFAULT_HELP_FILES	"Help_"
#define DEFAULT_NNTP_PORT 119
#define MAX_BUF_SIZE    8192  /* taille maximum de lecture brute */
#define MAX_READ_SIZE   2048  /* taille maximum de lecture raffinee */
#define MAX_REGEXP_SIZE 1024  /* taille max d'une regexp */

#define MAX_SERVER_LEN  128
#define MAX_NEWSGROUP_LEN 128

/* Commandes a envoyer au serveur */

#define CMD_QUIT 0
#define CMD_MODE_READER 1
#define CMD_NEWGROUPS 2
#define CMD_NEWNEWS 3
#define CMD_GROUP 4
#define CMD_STAT 5
#define CMD_HEAD 6
#define CMD_BODY 7
#define CMD_XHDR 8
#define CMD_XOVER 9
#define CMD_POST 10
#define CMD_LIST 11
#define CMD_DATE 12
#define CMD_ARTICLE 13
#define CMD_AUTHINFO 14
#define NB_TCP_CMD 15


/* Touches de Commande */
#define NB_FLCMD 65
#define FLCMD_UNDEF -1
#define FLCMD_RETURN FLCMD_SUIV
#define FLCMD_MACRO 1024
#define fl_key_explicite '\\'
#define fl_key_nocbreak '@'
#define fl_key_nm_opt ':'
#define fl_key_nm_help '?'

extern int do_deplace(int); 
extern int do_goto(int); /* renvoie change */
extern int do_unsubscribe(int);
extern int do_abonne(int);
extern int do_omet(int);
extern int do_kill(int); /* tue avec les crossposts */
extern int do_zap_group(int); /* la touche z... a revoir */
extern int do_help(int);
extern int do_quit(int); /* cette fonction est inutile */
extern int do_summary(int); /* Doit faire à la fois r, t, T */
extern int do_save(int);
extern int do_launch_pager(int);
extern int do_list(int);
extern int do_post(int);
extern int do_opt(int);
extern int do_opt_menu(int);
extern int do_neth(int);
/*extern int do_get_father(int); */
extern int do_swap_grp(int);
extern int do_prem_grp(int);
extern int do_pipe(int);
extern int do_tag(int);
extern int do_back(int);
extern int do_next(int);
extern int do_goto_tag(int);
extern int do_cancel(int);
extern int do_hist_menu(int);
extern int do_add_kill(int);
extern int do_remove_kill(int);
extern int do_pipe_header(int);
extern int do_select(int);
extern int do_keybindings(int);
extern int do_on_selected(int);
extern int do_art_msgid(int);

/* completions */
#include "flrn_comp.h"

extern int options_comp(char * /*option*/, int /*len*/, Liste_Chaine *);
extern int keybindings_comp(char *, int, Liste_Chaine *);
extern int flags_comp(char *, int, Liste_Chaine *);
extern int header_comp(char *, int, Liste_Chaine *);
extern int flag_header_comp(char *, int, Liste_Chaine *);


/* ATTENTION : MAX_FL_KEY DOIT ÊTRE UN BIT SEULEMENT */
#define MAX_FL_KEY 0x1000 

#include "flrn_inter.h"

#ifdef IN_FLRN_INTER_C

#include "flrn_slang.h"

Flcmd Flcmds[NB_FLCMD] = {
   { "previous", 'p' , '-', 2, &do_deplace, NULL },
#define FLCMD_PREC 0
   { "next-article", '\n', '\r', 2, &do_deplace, NULL },
#define FLCMD_SUIV 1
   { "article", 'v', 0, 6, &do_deplace, NULL },
#define FLCMD_VIEW 2
   { "goto", 'g', 0, 31, &do_goto, NULL },
#define FLCMD_GOTO 3
   { "GOTO", 'G', 0, 31, &do_goto, NULL },
#define FLCMD_GGTO 4
   { "unsubscribe", 'u', 0, 5, &do_unsubscribe, NULL },
#define FLCMD_UNSU 5
   { "subscribe", 'a', 0, 5, &do_abonne, NULL },
#define FLCMD_ABON 6
   { "omit", 'o', 0, 6|CMD_NEED_GROUP, &do_omet, NULL },
#define FLCMD_OMET 7
   { "OMIT", 'O', 0, 6|CMD_NEED_GROUP, &do_omet, NULL },
#define FLCMD_GOMT 8
   { "zap" , 'z', 0 ,13|CMD_NEED_GROUP, &do_zap_group, NULL },
#define FLCMD_ZAP  9
   { "help" , 'h', fl_key_nm_help ,1, &do_help, NULL },
#define FLCMD_HELP 10
   { "quit" , 'q', 0 ,0, &do_quit, NULL },
#define FLCMD_QUIT 11
   { "QUIT" , 'Q', 0 ,0, &do_quit, NULL },
#define FLCMD_GQUT 12
   { "kill-thread" , 'J', 0 ,2|CMD_NEED_GROUP, &do_kill, NULL },
#define FLCMD_GKIL 13
   { "kill-replies" , 'K', 0 ,2|CMD_NEED_GROUP, &do_kill, NULL },
#define FLCMD_KILL 14
   { "kill" , 'k', 0 ,2|CMD_NEED_GROUP, &do_kill, NULL },
#define FLCMD_PKIL 15
   { "summary" , 'r', 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_SUMM 16
   { "post", 'm', 0, 5|CMD_NEED_GROUP, &do_post, NULL },
#define FLCMD_POST 17
   { "reply", 'R', 0, 7|CMD_NEED_GROUP, &do_post, NULL },
#define FLCMD_ANSW 18
   { "view", 'V', 0, 2|CMD_NEED_GROUP, &do_launch_pager, NULL },
#define FLCMD_PAGE 19
   { "save", 's', 0, 7|CMD_NEED_GROUP, &do_save, NULL },
#define FLCMD_SAVE 20
   { "SAVE", 'S', 0, 7|CMD_NEED_GROUP, &do_save, NULL },
#define FLCMD_GSAV 21
   { "list", 'l', 0, 13, &do_list, NULL },
#define FLCMD_LIST 22
   { "LIST", 'L', 0, 13, &do_list, NULL },
#define FLCMD_GLIS 23
   { "summ-replies", 't', 0, 6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_THRE 24
   { "summ-thread", 'T', 0, 6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_GTHR 25
   { "option", 0, fl_key_nm_opt, 1, &do_opt, &options_comp }, 
#define FLCMD_OPT 26
   { "up", FL_KEY_UP, 0, 2, &do_deplace, NULL },
#define FLCMD_UP 27
   { "down", FL_KEY_DOWN, 0, 2, &do_deplace, NULL },
#define FLCMD_DOWN 28
   { "left", FL_KEY_LEFT, 0, 2, &do_deplace, NULL },
#define FLCMD_LEFT 29
   { "right", FL_KEY_RIGHT, 0, 2, &do_deplace, NULL },
#define FLCMD_RIGHT 30
   { "next-unread", ' ', 0, 2, &do_deplace, NULL },
#define FLCMD_SPACE 31
   { "show-tree", 'N', 0, 2|CMD_NEED_GROUP, &do_neth, NULL },
#define FLCMD_NETH 32
   { "swap-grp", 0, 0, 15|CMD_NEED_GROUP, &do_swap_grp, NULL },
#define FLCMD_SWAP_GRP 33
   { "prem-grp", 0, 0, 13|CMD_NEED_GROUP, &do_prem_grp, NULL },
#define FLCMD_PREM_GRP 34
   { "pipe", '|', 0, 15|CMD_NEED_GROUP, &do_pipe, NULL },
#define FLCMD_PIPE 35 
   { "PIPE", 0, 0, 15|CMD_NEED_GROUP, &do_pipe, NULL },
#define FLCMD_GPIPE 36
   { "filter", '%', 0, 15|CMD_NEED_GROUP, &do_pipe, NULL },
#define FLCMD_FILTER 37 
   { "FILTER", 0, 0, 15|CMD_NEED_GROUP, &do_pipe, NULL },
#define FLCMD_GFILTER 38
   { "shell", 0, 0, 13, &do_pipe, NULL },
#define FLCMD_SHELL 39
   { "shin", '!', 0, 13, &do_pipe, NULL },
#define FLCMD_SHELLIN 40
   { "config", 0, 0, 1,&do_opt_menu, NULL },
#define FLCMD_OPTMENU 41
   { "mail-answer", 0, 0, 3|CMD_NEED_GROUP,&do_post, NULL },
#define FLCMD_MAIL 42
   { "tag", '"', 0, 2|CMD_NEED_GROUP,&do_tag, NULL },
#define FLCMD_TAG 43
   { "go-tag", '\'', 0, 2,&do_goto_tag, NULL },
#define FLCMD_GOTO_TAG 44
   { "cancel", 'e', 0, 2|CMD_NEED_GROUP, &do_cancel, NULL },
#define FLCMD_CANCEL 45
   { "hist-prev", 'B', 0, 2, &do_back, NULL },
#define FLCMD_HPREV 46
   { "hist-next", 'F', 0, 2, &do_next, NULL },
#define FLCMD_HSUIV 47
   { "history", 'H', 0, 2, &do_hist_menu, NULL },
#define FLCMD_HMENU 48
   { "supersedes", 0, 0, 3|CMD_NEED_GROUP, &do_post, NULL },
#define FLCMD_SUPERSEDES 49
   { "summ-search" , 0, 0 ,14|CMD_NEED_GROUP, &do_summary, &flag_header_comp },
#define FLCMD_SUMM_SEARCH 50
   { "menu-summary" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUSUMM 51
   { "menu-replies" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUTHRE 52
   { "menu-thread" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUGTHR 53
   { "menu-search" , '/', 0 ,14|CMD_NEED_GROUP, &do_summary, &flag_header_comp },
#define FLCMD_MENUSUMMS 54
   { "save-options", 0, 0, 1, &do_save, NULL },
#define FLCMD_SAVE_OPT 55
   { "add-kill", 0, 0, 5, &do_add_kill, NULL },
#define FLCMD_ADD_KILL 56
   { "remove-kill", 0, 0, 5, &do_remove_kill, NULL },
#define FLCMD_REMOVE_KILL 57
   { "pipe-header", 0, 0, 15|CMD_NEED_GROUP, &do_pipe, &header_comp },
#define FLCMD_PIPE_HEADER 58
   { "select", 0, 0, 14|CMD_NEED_GROUP, &do_select, &flag_header_comp },
#define FLCMD_SELECT 59
   { "art-to-return", 'x', 0, 2|CMD_NEED_GROUP, &do_kill, NULL },
#define FLCMD_ART_TO_RETURN 60
   { "keybindings", 0, 0, 17, &do_keybindings, &keybindings_comp },
#define FLCMD_KEYBINDINGS 61
   { "put-flag", 0, 0, 15|CMD_NEED_GROUP, &do_omet, &flags_comp },
#define FLCMD_PUT_FLAG 62
   { "on-selected", 0, 0, 15|CMD_NEED_GROUP, &do_on_selected, NULL },
#define FLCMD_ON_SELECTED 63
   { "art-msgid", 0, 0, 29, &do_art_msgid, NULL },
#define FLCMD_ART_MSGID 64
};

#define CMD_DEF_PLUS (sizeof(Cmd_Def_Plus)/sizeof(Cmd_Def_Plus[0]))
struct cmd_predef {
  int key;
  int cmd;
  char *args;
  int add;
} Cmd_Def_Plus[] = {
  { 'b', FLCMD_LEFT, NULL, 0 },
  { 'f', FLCMD_RIGHT, NULL, 0 },
  { 'n', FLCMD_SUIV, NULL, 0 },
  { 'P', FLCMD_HPREV, NULL, 0 },
  { '[', FLCMD_LEFT, NULL, 0 },
  { ']', FLCMD_RIGHT, NULL, 0 },
  { '(', FLCMD_UP, NULL, 0 },
  { ')', FLCMD_DOWN, NULL, 0 },
  { 2  , FLCMD_PIPE, "urlview", 0 },
  { '+', FLCMD_SELECT, "1-,unread", 0 },
};

#else

extern Flcmd Flcmds[NB_FLCMD];
/* On fait les defines suivant pour les menus */
#define FLCMD_PREC 0
#define FLCMD_SUIV 1
#define FLCMD_VIEW 2
#define FLCMD_QUIT 11
#define FLCMD_UP 27
#define FLCMD_DOWN 28
/* et ceci pour la commande List... si ca continue on va tout définir... */
#define FLCMD_ADD_KILL 56
#define FLCMD_REMOVE_KILL 57
#define FLCMD_UNSU 5
#define FLCMD_ABON 6
#define FLCMD_ZAP  9
#define FLCMD_GOTO 3
#define FLCMD_GGTO 4

#endif

#endif
