#ifndef FLRN_FLRN_CONFIG_H
#define FLRN_FLRN_CONFIG_H

#include "pathdef.h"

#define MAX_PATH_LEN 	1024
#define DEFAULT_CONFIG_FILE	".flrnrc"
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

extern const char *tcp_command[];


/* Touches de Commande */
#define NB_FLCMD 58
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


#define MAX_FL_KEY 0x1000
/* le tableau touche -> commande */
extern int Flcmd_rev[MAX_FL_KEY];

#include "flrn_inter.h"

#ifdef IN_FLRN_INTER_C

#include "flrn_slang.h"

Flcmd Flcmds[NB_FLCMD] = {
   { "prec", 'p' , '-', 2, &do_deplace },
#define FLCMD_PREC 0
   { "suiv", '\r', '\n', 2, &do_deplace },
#define FLCMD_SUIV 1
   { "art", 'v', 0, 6, &do_deplace },
#define FLCMD_VIEW 2
   { "goto", 'g', 0, 15, &do_goto },
#define FLCMD_GOTO 3
   { "GOTO", 'G', 0, 15, &do_goto },
#define FLCMD_GGTO 4
   { "unsu", 'u', 0, 5|CMD_NEED_GROUP, &do_unsubscribe },
#define FLCMD_UNSU 5
   { "abon", 'a', 0, 5|CMD_NEED_GROUP, &do_abonne },
#define FLCMD_ABON 6
   { "omet", 'o', 0, 6|CMD_NEED_GROUP, &do_omet },
#define FLCMD_OMET 7
   { "OMET", 'O', 0, 6|CMD_NEED_GROUP, &do_omet },
#define FLCMD_GOMT 8
   { "zap" , 'z', 0 ,13|CMD_NEED_GROUP, &do_zap_group },
#define FLCMD_ZAP  9
   { "help" , 'h', fl_key_nm_help ,1, &do_help },
#define FLCMD_HELP 10
   { "quit" , 'q', 0 ,0, &do_quit },
#define FLCMD_QUIT 11
   { "QUIT" , 'Q', 0 ,0, &do_quit },
#define FLCMD_GQUT 12
   { "KILL_THR" , 'J', 0 ,2|CMD_NEED_GROUP, &do_kill },
#define FLCMD_GKIL 13
   { "KILL" , 'K', 0 ,2|CMD_NEED_GROUP, &do_kill },
#define FLCMD_KILL 14
   { "kill" , 'k', 0 ,2|CMD_NEED_GROUP, &do_kill },
#define FLCMD_PKIL 15
   { "summary" , 'r', 0 ,6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_SUMM 16
   { "post", 'm', 0, 5|CMD_NEED_GROUP, &do_post },
#define FLCMD_POST 17
   { "repond", 'R', 0, 7|CMD_NEED_GROUP, &do_post },
#define FLCMD_ANSW 18
   { "view", 'V', 0, 2|CMD_NEED_GROUP, &do_launch_pager },
#define FLCMD_PAGE 19
   { "save", 's', 0, 15|CMD_NEED_GROUP, &do_save },
#define FLCMD_SAVE 20
   { "SAVE", 'S', 0, 15|CMD_NEED_GROUP, &do_save },
#define FLCMD_GSAV 21
   { "list", 'l', 0, 13, &do_list },
#define FLCMD_LIST 22
   { "LIST", 'L', 0, 13, &do_list },
#define FLCMD_GLIS 23
   { "summ-fil", 't', 0, 6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_THRE 24
   { "summ-thr", 'T', 0, 6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_GTHR 25
   { "option", 0, fl_key_nm_opt, 1, &do_opt }, 
#define FLCMD_OPT 26
   { "up", FL_KEY_UP, 0, 2, &do_deplace },
#define FLCMD_UP 27
   { "down", FL_KEY_DOWN, 0, 2, &do_deplace },
#define FLCMD_DOWN 28
   { "left", FL_KEY_LEFT, 0, 2, &do_deplace },
#define FLCMD_LEFT 29
   { "right", FL_KEY_RIGHT, 0, 2, &do_deplace },
#define FLCMD_RIGHT 30
   { "next", ' ', 0, 2, &do_deplace },
#define FLCMD_SPACE 31
   { "nxt-thr", 'N', 0, 2|CMD_NEED_GROUP, &do_neth },
#define FLCMD_NETH 32
   { "swap-grp", 0, 0, 15|CMD_NEED_GROUP, &do_swap_grp },
#define FLCMD_SWAP_GRP 33
   { "prem-grp", 0, 0, 13|CMD_NEED_GROUP, &do_prem_grp },
#define FLCMD_PREM_GRP 34
   { "pipe", '|', 0, 15|CMD_NEED_GROUP, &do_pipe },
#define FLCMD_PIPE 35 
   { "PIPE", 0, 0, 15|CMD_NEED_GROUP, &do_pipe },
#define FLCMD_GPIPE 36
   { "filter", '%', 0, 15|CMD_NEED_GROUP, &do_pipe },
#define FLCMD_FILTER 37 
   { "FILTER", 0, 0, 15|CMD_NEED_GROUP, &do_pipe },
#define FLCMD_GFILTER 38
   { "shell", 0, 0, 13, &do_pipe },
#define FLCMD_SHELL 39
   { "shin", '!', 0, 13, &do_pipe },
#define FLCMD_SHELLIN 40
   { "config", 0, 0, 1,&do_opt_menu },
#define FLCMD_OPTMENU 41
   { "mail-ans", 0, 0, 3|CMD_NEED_GROUP,&do_post },
#define FLCMD_MAIL 42
   { "tag", '"', 0, 2|CMD_NEED_GROUP,&do_tag },
#define FLCMD_TAG 43
   { "gotag", '\'', 0, 2,&do_goto_tag },
#define FLCMD_GOTO_TAG 44
   { "cancel", 'e', 0, 2|CMD_NEED_GROUP, &do_cancel },
#define FLCMD_CANCEL 45
   { "hist-prev", 'B', 0, 2, &do_back },
#define FLCMD_HPREV 46
   { "hist-suiv", 'F', 0, 2, &do_next },
#define FLCMD_HSUIV 47
   { "hist-menu", 'H', 0, 2, &do_hist_menu },
#define FLCMD_HMENU 48
   { "supersedes", 0, 0, 3|CMD_NEED_GROUP, &do_post },
#define FLCMD_SUPERSEDES 49
   { "sum-search" , 0, 0 ,14|CMD_NEED_GROUP, &do_summary },
#define FLCMD_SUMM_SEARCH 50
   { "menu-summary" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_MENUSUMM 51
   { "menu-fil" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_MENUTHRE 52
   { "menu-thr" , 0, 0 ,6|CMD_NEED_GROUP, &do_summary },
#define FLCMD_MENUGTHR 53
   { "menu-search" , '/', 0 ,14|CMD_NEED_GROUP, &do_summary },
#define FLCMD_MENUSUMMS 54
   { "sav-opt", 0, 0, 1, &do_save },
#define FLCMD_SAVE_OPT 55
   { "add-kill", 0, 0, 5|CMD_NEED_GROUP, &do_add_kill },
#define FLCMD_ADD_KILL 56
   { "remove-kill", 0, 0, 5|CMD_NEED_GROUP, &do_remove_kill },
#define FLCMD_REMOVE_KILL 57
};

#define CMD_DEF_PLUS (sizeof(Cmd_Def_Plus)/sizeof(Cmd_Def_Plus[0]))
struct cmd_predef {
  int key;
  int cmd;
  char *args;
} Cmd_Def_Plus[] = {
  { 'b', FLCMD_LEFT, NULL },
  { 'f', FLCMD_RIGHT, NULL },
  { 'n', FLCMD_SUIV, NULL },
  { 'P', FLCMD_HPREV, NULL },
  { '[', FLCMD_LEFT, NULL },
  { ']', FLCMD_RIGHT, NULL },
  { '(', FLCMD_UP, NULL },
  { ')', FLCMD_DOWN, NULL },
  { 2  , FLCMD_PIPE, "urlview" },
};

#else

extern Flcmd Flcmds[NB_FLCMD];
/* On fait les defines suivant pour les menus */
#define FLCMD_PREC 0
#define FLCMD_SUIV 1
#define FLCMD_QUIT 11
#define FLCMD_UP 27
#define FLCMD_DOWN 28

#endif

#endif
