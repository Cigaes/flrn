#ifndef FLRN_FLRN_CONFIG_H
#define FLRN_FLRN_CONFIG_H

#include "pathdef.h"

#define MAX_PATH_LEN 	1024
#define DEFAULT_DIR_FILE	".flrn"
/* Répertoire où chercher les fichiers de config */
#define DEFAULT_CONFIG_FILE	".flrnrc"
#define DEFAULT_CONFIG_SYS_FILE "flrnrc" /* si pas de .flrnrc, on ouvre celui-la */
#define DEFAULT_FLNEWS_FILE	".flnewsrc"
#define TMP_POST_FILE           ".article"
  /* en fait fichier temporaire utilisé partout */
#define TMP_REJECT_POST_FILE    "dead.article"
#define DEFAULT_HELP_DIRECTORY	PATH_HELP_DIRECTORY /* defined in pathdef.h */
#define DEFAULT_HELP_FILES	"Help_"
#define DEFAULT_NNTP_PORT 119
#define CHECK_MAIL	1
#ifdef CHECK_MAIL
/* peut-être à mettre dans le configure... */
#define DEFAULT_MAIL_PATH "/var/spool/mail" 
#endif
#define MAX_BUF_SIZE    8192  /* taille maximum de lecture brute */
#define MAX_READ_SIZE   2048  /* taille maximum de lecture raffinee */
#define MAX_REGEXP_SIZE 1024  /* taille max d'une regexp */

#define MAILER_CMD SENDMAIL " -U -t"
/* #define DOMAIN "ens.fr" */
/* #define DEFAULT_HOST "naoned" */ /* maintenant, ça se fait avec configure */
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
#define NB_TCP_CMD 14


/* Touches de Commande */
#define NB_FLCMD 64
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

/* completions */
#include "flrn_comp.h"

extern int options_comp(char * /*option*/, int /*len*/, Liste_Chaine *);
extern int keybindings_comp(char *, int, Liste_Chaine *);
extern int flags_comp(char *, int, Liste_Chaine *);


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
   { "save", 's', 0, 15|CMD_NEED_GROUP, &do_save, NULL },
#define FLCMD_SAVE 20
   { "SAVE", 'S', 0, 15|CMD_NEED_GROUP, &do_save, NULL },
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
   { "summ-search" , 0, 0 ,14|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_SUMM_SEARCH 50
   { "menu-summary" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUSUMM 51
   { "menu-replies" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUTHRE 52
   { "menu-thread" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUGTHR 53
   { "menu-search" , '/', 0 ,14|CMD_NEED_GROUP, &do_summary, NULL },
#define FLCMD_MENUSUMMS 54
   { "save-options", 0, 0, 1, &do_save, NULL },
#define FLCMD_SAVE_OPT 55
   { "add-kill", 0, 0, 5, &do_add_kill, NULL },
#define FLCMD_ADD_KILL 56
   { "remove-kill", 0, 0, 5, &do_remove_kill, NULL },
#define FLCMD_REMOVE_KILL 57
   { "pipe-header", 0, 0, 15|CMD_NEED_GROUP, &do_pipe, NULL },
#define FLCMD_PIPE_HEADER 58
   { "select", 0, 0, 14|CMD_NEED_GROUP, &do_select, NULL },
#define FLCMD_SELECT 59
   { "art-to-return", 'x', 0, 2|CMD_NEED_GROUP, &do_kill, NULL },
#define FLCMD_ART_TO_RETURN 60
   { "keybindings", 0, 0, 17, &do_keybindings, &keybindings_comp },
#define FLCMD_KEYBINDINGS 61
   { "put-flag", 0, 0, 15|CMD_NEED_GROUP, &do_omet, NULL },
#define FLCMD_PUT_FLAG 62
   { "on-selected", 0, 0, 15|CMD_NEED_GROUP, &do_on_selected, NULL },
#define FLCMD_ON_SELECTED 63
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
