/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_inter.c : mode commande de flrn, boucle principale
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#define IN_FLRN_INTER_C
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "options.h"
#include "group.h"
#include "art_group.h"
#include "flrn_format.h"
#include "flrn_menus.h"
#include "flrn_pager.h"
#include "flrn_filter.h"
#include "flrn_tags.h"
#include "flrn_macros.h"
#include "flrn_command.h"
#include "flrn_files.h"
#include "flrn_shell.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "post.h"
#include "flrn_help.h"
#include "flrn_regexp.h"
#include "flrn_color.h"
#include "flrn_messages.h"
#include "flrn_xover.h"
#include "flrn_func.h"
#include "enc/enc_strings.h"
#ifdef USE_SLANG_LANGUAGE
#include "slang.h"
#include "slang_flrn.h"
#endif

static UNUSED char rcsid[]="$Id$";

/* On va définir ici des structures et des variables qui seront utilisées */
/* pour loop, et les fonctions qui y sont associés. Il faudrait en fait   */
/* que flrn_inter se limite à ses fonctions...				  */

/* Ces variables etaient auparavant locales à loop */
struct etat_var {
   int hors_struct; /* 1       : hors_limite 
		       2 et 3  : end_of_group ( => hors_limite )
		       4 et 5  : under_group  ( => hors_limite )
		       (7)     : groupe vide
		       8 et 11 : hors_newsgroup ( => end_of_groupe ) */
   int etat, num_message; 
   /* etat=0 : afficher, etat=1 : message, etat=3 : rien, etat=4 : commande */
   /* Le système num_message est probablement insuffisant pour des messages */
   /* explicites...							    */
   Newsgroup_List *Newsgroup_nouveau; /* Pour les changement de newsgroup */
   int num_futur_article;
   int next_cmd;
   Article_List *Article_nouveau; /* utilisé si num_futur_article == -1 */
} etat_loop, etat_save;

typedef struct Num_lists
{
   struct Num_lists *next;
   int numlst_flags; /* 0 : rien   1 : article       2 : num1
                        4 : _article  8 : article>   16 : big_thread(*) 
	               32 : num1-num2  64(flag) : selected */
#define NUMLST_ART 1
#define NUMLST_NUM 2
#define NUMLST_ASC 4
#define NUMLST_DES 8
#define NUMLST_THR 16
#define NUMLST_RNG 32
#define NUMLST_ALL 63
#define NUMLST_SEL 64
   union element elem1;
   int num2;
} Numeros_List;

/* DOC : le flag selected s'applique pour toute commande qui appelle 
   distribue_action. Dans le cas ou aucun numéro n'est précisé, ça équivaut
   à "1-".
   Les fonctions qui appellent distribue_action (avec un flag potentiel
   a 64) sont : 
       do_tag
       do_omet (donc put_flags)
       do_kill
       do_summary
       do_select
       do_save
       do_launch_pager
       do_pipe
       do_cancel
*/

Numeros_List Arg_do_funcs={NULL, 0, {0}, 0};
static int thread_view=0;

typedef int (*Action)(Article_List *article, void * flag);
int distribue_action(Numeros_List *num, Action action, Action special,
    void * flag, int flags_to_omit);
int thread_action(Article_List *article,int all, Action action, void *param, int flags, int flags_to_omit);
int gthread_action(Article_List *article,int all, Action action, void *param, int flags, int flags_to_omit);


Cmd_return une_commande;
#define MAX_CHAR_STRING 100
flrn_char Arg_str[MAX_CHAR_STRING];


/* Cette horrible structure sert a stocker une action et son argument en un */
/* seul pointeur...							    */
typedef struct Action_Beurk {
   Action action;
   void *flag;
   int flags_to_omit;
} Action_with_args;

struct file_and_int {
   FILE *file;
   int num;
};

/* ces deux trucs servent pour do_summary, et do_select */
static long min_kill_l, max_kill_l;

int parse_arg_string(flrn_char *str,int command, int annu16);
/* On prédéfinit ici les fonctions appelés par loop... A l'exception de */
/* get_command, elles DOIVENT être de la forme int do_* (int res)       */ 
#ifdef USE_SLANG_LANGUAGE
int get_command_command(int get_com, SLang_Name_Type **slang_fun);
int Execute_function_slang_command(int, SLang_Name_Type *slang_fun);
#else
int get_command_command(int get_com);
#endif
int do_deplace(int res); 
int do_goto(int res); /* renvoie change */
int do_unsubscribe(int res);
int do_omet(int res);
int do_kill(int res); /* tue avec les crossposts */
int do_zap_group(int res); /* la touche z... a revoir */
int do_help(int res);
int do_quit(int res); /* cette fonction est inutile */
int do_summary(int res); /* Doit faire à la fois r, t, T */
int do_save(int res);
int do_pipe(int res);
int do_launch_pager(int res);
int do_list(int res);
int do_post(int res);
int do_opt(int res);
int do_opt_menu(int res);
int do_neth(int res);
int do_get_father(int res);
int do_swap_grp(int res);
int do_art_msgid(int res);
int do_prem_grp(int res);
int do_goto_tag(int res);
int do_tag(int res);
int do_back(int res);
int do_cancel(int res);
int do_abonne(int res);
int do_remove_kill(int res);
int do_add_kill(int res);
int do_select(int res);
int do_keybindings(int);
int do_on_selected(int res);


/* Ces fonctions sont appelés par les do_* */
int change_group(Newsgroup_List **newgroup,int flags, flrn_char *gpe_tab);
int prochain_non_lu(int force_reste, Article_List **debut, int just_entered, int pas_courant);
int prochain_newsgroup();
void Get_option_line(flrn_char *argument);

static void apply_autocmd(int flag, char *name);

static int push_tag();

/* aff_opt_c : affiche simplement une liste de messages non lus vers stdout */
/* A la demande de Sbi, j'affiche aussi le nombre total de messages    	*/
/* non lus...								    */
int aff_opt_c(flrn_char *newsgroup, int with_abon) {
   int deb, res, nb_non_lus=0, rc;
   int to_build; /* sans intérêt ici, mais on doit l'utiliser pour un appel */
   flrn_char ligne[100];
   char *trad;
   
   Newsgroup_courant=Newsgroup_deb;
   while (Newsgroup_courant) {
      if (((with_abon) && 
		  (Newsgroup_courant->grp_flags & GROUP_UNSUBSCRIBED)) ||
          (newsgroup && (fl_strstr(truncate_group(Newsgroup_courant->name,0),newsgroup)==NULL))) {
         Newsgroup_courant=Newsgroup_courant->next;
	 continue;
      }
      res=NoArt_non_lus(Newsgroup_courant,0);
      if (res==-2) {
	 rc=conversion_to_terminal
	     (Newsgroup_courant->name,&trad,0,(size_t)(-1));
	 fprintf(stdout, "Mauvais newsgroup : %s\n", trad);
	 if (rc==0) free(trad);
      }
      if (res>0) {
        if (newsgroup) {
          deb=Newsgroup_courant->read?Newsgroup_courant->read->max[0]:1;
          cree_liste(deb,&to_build);
	  if ((res=Newsgroup_courant->not_read)) {
	    rc=conversion_to_terminal
		   (truncate_group(Newsgroup_courant->name,0),
		    &trad,0,(size_t)(-1));
	    if (res==1) 
		fprintf(stdout, "%s : 1 article non lu\n",trad);
            else fprintf(stdout, "%s : %d articles non lus\n",trad,res);
	    if (rc==0) free(trad);
	    Article_courant=Article_deb;
	    while (Article_courant) {
	       if (!(Article_courant->art_flags & FLAG_READ)) {
		  Prepare_summary_line(Article_courant,
			  NULL,0,ligne,99,80,0,0,0);
		  rc=conversion_to_terminal (ligne,&trad,0,(size_t)(-1));
	          fprintf(stdout,"%s\n",trad);
		  if (rc==0) free(trad);
	       }
	       Article_courant=Article_courant->next;
	    }
          }
	  if (Article_deb) {
	     detruit_liste(1);
	     Newsgroup_courant->Article_deb=NULL;
	     Newsgroup_courant->Article_exte_deb=NULL;
	     Newsgroup_courant->Hash_table=NULL;
	     Newsgroup_courant->Thread_deb=NULL;
	  }
	  to_build=0;
        } else {
	    rc=conversion_to_terminal
		   (truncate_group(Newsgroup_courant->name,0),
		    &trad,0,(size_t)(-1));
	    if (res==1) 
		fprintf(stdout, "%s : 1 article non lu\n",trad);
            else fprintf(stdout, "%s : %d articles non lus\n",trad,res);
	    if (rc==0) free(trad);
	}
        nb_non_lus+=res;
      }
      Newsgroup_courant=Newsgroup_courant->next;
   }
   if (newsgroup==NULL) {
      if (nb_non_lus==0) 
        fputs(_(Messages[MES_NOTHING_NEW]),stdout); 
      else
       fprintf(stdout, "  Il y a au total %d article%s non lu%s.",nb_non_lus,(nb_non_lus==1 ? "" : "s"), (nb_non_lus==1 ? "" : "s"));
      putc('\n',stdout);
   }
   return (nb_non_lus>0);
}

/* affiche un message
 * type = 0 => info, sinon, erreur */
static void Aff_message(int type, int num)
{
  switch (num) {
 /* Message d'information */
    case 1 : Aff_error(_(Messages[MES_NOTHING_NEW])); break;
    case 2 : Aff_error(_(Messages[MES_EOG])); break;
    case 3 : Aff_error(_(Messages[MES_NO_MES])); break;
    case 4 : 
    case 5 : Aff_error_fin(_(Messages[MES_OMIT]),0,-1); break;
    case 6 : Aff_error(_(Messages[MES_POST_SEND])); break;
    case 7 : Aff_error(_(Messages[MES_POST_CANCELED])); break;
    case 8 : Aff_error_fin(_(Messages[MES_ART_SAVED]),0,-1); break;
    case 9 : Aff_error_fin(_(Messages[MES_ABON]),0,-1); break;
    case 10 : Aff_error(_(Messages[MES_NOSEL_THREAD])); break;
    case 11 : Aff_error_fin(_(Messages[MES_ZAP]),0,-1);
	      break;
    case 12 : Aff_error(_(Messages[MES_NOTHER_GROUP])); break;
    case 13 : Aff_error_fin(_(Messages[MES_NO_XREF]),1,-1); break;
    case 14 : Aff_error_fin(_(Messages[MES_CONTINUE]),0,0); break;
    case 15 : Aff_error_fin(_(Messages[MES_TAG_SET]),0,-1); break;
    case 16 : Aff_error_fin(_(Messages[MES_CANCEL_CANCELED]),0,-1); break;
    case 17 : Aff_error(_(Messages[MES_CANCEL_DONE])); break;
    case 18 : 
    case 19 : Aff_error_fin(_(Messages[MES_OP_DONE]),0,-1); break;
    case 20 : Aff_error(_(Messages[MES_MAIL_SENT])); break;
    case 21 : Aff_error(_(Messages[MES_MAIL_POST])); break;
    case 22 : Aff_error_fin(_(Messages[MES_TEMP_READ]),0,-1); break;
    case 23 : Aff_error_fin(_(Messages[MES_FLAG_APPLIED]),0,-1); break;
    case 24 : Aff_error_fin(_(Messages[MES_MODE_THREAD]),0,-1); break;
    case 25 : Aff_error_fin(_(Messages[MES_MODE_NORMAL]),0,-1); break;
 /* Message d'erreur */
    case -1 : Aff_error(_(Messages[MES_NO_GROUP]));  /* non utilisé */
	       break;
    case -2 : Aff_error(_(Messages[MES_GROUP_EMPTY]));
	       break;
    case -3 : Aff_error(_(Messages[MES_NOT_IN_GROUP]));
	       break;
/*	        case -4 : Aff_error("Post refusé.");
	       break;   */
    case -5 : Aff_error_fin(_(Messages[MES_NEGATIVE_NUMBER]),1,-1); /* non utilisé */
	       break;
    case -6 : Aff_error(_(Messages[MES_SAVE_FAILED]));
	       break;
    case -7 : Aff_error(_(Messages[MES_UNK_GROUP]));
	       break;
    case -8 : Aff_error(_(Messages[MES_NO_FOUND_GROUP]));
	       break;
    case -9 : Aff_error_fin(_(Messages[MES_UNKNOWN_CMD]),1,-1);
	       break;
    case -10 : Aff_error_fin(_(Messages[MES_REGEXP_BUG]),1,-1); break;
    case -11 : Aff_error(_(Messages[MES_PIPE_BUG])); break;
    case -12 : Aff_error(_(Messages[MES_MES_NOTIN_GROUP]));
	       break;
    case -13 : Aff_error_fin(_(Messages[MES_BAD_TAG]),1,-1); break;
    case -14 : Aff_error_fin(_(Messages[MES_CANCEL_REFUSED]),1,-1); break;
    case -15 : Aff_error_fin(_(Messages[MES_EMPTY_HISTORY]),1,-1); break;
/*    case -16 : Aff_error("Vous ne pouvez pas poster ici."); break; */
/* ce message est idiot : rien n'empêche de faire un followup, sauf à */
/* la rigueur si le serveur refuse tout...			      */
    case -17 : Aff_error_fin(_(Messages[MES_NO_HEADER]),1,-1); break;
    case -18 : Aff_error_fin(_(Messages[MES_REFUSED_HEADER]),1,-1); break;
    case -19 : Aff_error_fin(_(Messages[MES_INVAL_FLAG]),1,-1); break;
    case -20 : Aff_error_fin(_(Messages[MES_MACRO_FORBID]),1,-1); break;
    case -21 : Aff_error_fin(_(Messages[MES_NO_MSGID]),1,-1); break;
    default : Aff_error(_(Messages[MES_FATAL]));
	       break;
  }

}

/* return 1 if 'q' was pressed */
int loop(flrn_char *opt) {
   int res=0, quit=0, ret;
   int to_build=0; /* il faut appeler cree_liste */
   int change, a_change=1;
   Numeros_List *fin_de_param=NULL;
   flrn_char Str_macro[MAX_CHAR_STRING];
#ifdef USE_SLANG_LANGUAGE
   SLang_Name_Type *slang_fun;
#endif
   
   Str_macro[0]=fl_static('\0');
   etat_loop.hors_struct=11;
   etat_loop.etat=etat_loop.num_message=0;
   etat_loop.Newsgroup_nouveau=NULL;
   etat_loop.next_cmd=-1;
   /* On cherche un newsgroup pour partir */
   Newsgroup_courant=NULL;
   Last_head_cmd.Article_vu=NULL;
   if (opt) {
      int save_opt_use_menus=Options.use_menus;
      Options.use_menus=0;
      res=change_group(&(etat_loop.Newsgroup_nouveau),2,opt);
      if (res<0) res=change_group(&(etat_loop.Newsgroup_nouveau),0,opt);
      if (res<0) res=change_group(&(etat_loop.Newsgroup_nouveau),3,opt);
      if (res<0) res=change_group(&(etat_loop.Newsgroup_nouveau),1,opt);
      Options.use_menus=save_opt_use_menus;
      Newsgroup_courant=etat_loop.Newsgroup_nouveau;
      if (Newsgroup_courant==NULL) {
      	etat_loop.etat=2; etat_loop.num_message=-8;
        Article_deb=&Article_bidon;
      } else {
	to_build=1;
	etat_loop.hors_struct=0;
      }
   }
   else {
     res=prochain_newsgroup(&(etat_loop.Newsgroup_nouveau));
     if (res<0) {
        etat_loop.etat=1; etat_loop.num_message=1;
        Article_deb=&Article_bidon;
     } else {
       to_build=1;
       etat_loop.hors_struct=0;
       Newsgroup_courant=etat_loop.Newsgroup_nouveau;
     }
     etat_loop.Newsgroup_nouveau=NULL;
   }
   if ((Newsgroup_courant==NULL) && (Options.quit_if_nothing)) 
      return 0;
	    
   /* Maintenant on cherche quelque chose à lire */
   
   while ((Newsgroup_deb) && (!quit)) {
      Aff_newsgroup_name(1);
      if (to_build) { 
	res = etat_loop.num_futur_article>0?etat_loop.num_futur_article:
	  (Newsgroup_courant->read?Newsgroup_courant->read->max[0]:1);
	res=cree_liste(res, &to_build);
	if (res==-2) {  
	   zap_newsgroup(Newsgroup_courant);
	   Newsgroup_courant=NULL;
	   Article_deb=&Article_bidon;
	   etat_loop.etat=2; etat_loop.num_message=-7;
	}
	etat_loop.hors_struct=(res<0)?11:0;
	if (res==0) {
	   Article_deb=&Article_bidon;
	   etat_loop.etat=2; etat_loop.num_message=-2;
	   etat_loop.hors_struct=7;
	}
      }
      Aff_newsgroup_courant();
      if (debug) fprintf(stderr, "Liste créée\n");
      if (etat_loop.num_futur_article==-1) {
	Article_courant=etat_loop.Article_nouveau;
	etat_loop.etat=0;
      } else {
	Article_courant=Article_deb;
      }
      if ((Newsgroup_courant) && (a_change)) {
          save_etat_loop();
	  apply_autocmd(AUTOCMD_ENTER,Newsgroup_courant->name);
	  restore_etat_loop();
      }
      a_change=0;
      if (Article_courant) {
	if (etat_loop.hors_struct==1) etat_loop.hors_struct=0;
	if (!(etat_loop.hors_struct & 8)) 
	{
	  if (etat_loop.num_futur_article==0)
           change=-prochain_non_lu(etat_loop.etat>0,&Article_courant,1,0);
	  else {
	    if (etat_loop.num_futur_article !=-1) {
	      Arg_do_funcs.elem1.number=etat_loop.num_futur_article;
	      Arg_do_funcs.num2=0;
	      Arg_do_funcs.numlst_flags=NUMLST_NUM;
	      if (Arg_do_funcs.next) Arg_do_funcs.next->numlst_flags=0;
	      do_deplace(FLCMD_VIEW);
	    }
	    change=0;
	  }
	}
	else change=0;
	  /* change=1 : il n'y a rien a lire */
	if ((etat_loop.Newsgroup_nouveau) && (change==1)) {
	   change=0; /* On veut rester sur ce newsgroup */
	   etat_loop.hors_struct|=3; /* Fin du conti */
	   etat_loop.etat=1; etat_loop.num_message=2;
	}
	if (change==2) { /* On doit reconstruire le groupe ??? */
	   change=1;
	   etat_loop.Newsgroup_nouveau=Newsgroup_courant;
	}
	while ((!change) && (!quit)) {
	   if ((etat_loop.hors_struct & 8) && (etat_loop.etat==0)) {
	      etat_loop.etat=2; etat_loop.num_message=-3;
	   }
	   if (debug) fprintf(stderr, "etat %d num_message %d\n", etat_loop.etat, etat_loop.num_message);
	   ret=0;
	   if (etat_loop.etat==0) { 
	      if (!thread_view) 
	         ret=Aff_article_courant(to_build); 
	      else ret=Aff_grand_thread(to_build);
	      push_tag();
	      etat_loop.hors_struct&=8;
	      if (etat_loop.hors_struct) etat_loop.hors_struct|=3;
			  /* ceci revient a ne garder qu'hors_newsgroup */
	   }
	   if ((etat_loop.etat==1) || (etat_loop.etat==2))
	     Aff_message(etat_loop.etat-1, etat_loop.num_message);
	   if (etat_loop.etat==4) ret=1; /* Commande deja tapee */
	   etat_loop.etat=0; etat_loop.num_message=0; 
	   etat_loop.num_futur_article=0;
	   etat_loop.Newsgroup_nouveau=NULL;
	   if (to_build) {
	     /* on finit la liste... */
	     cree_liste_end();
	     to_build=0;
	   }
	   if (ret<0) ret=0; /* Aff_article_courant a renvoyé une erreur */
	   if (etat_loop.next_cmd>=0) {
	     res=etat_loop.next_cmd;
	     etat_loop.next_cmd=-1; /* remis à jour ensuite */
	     fl_strncpy(Arg_str,Str_macro,99);
	     if (fin_de_param==NULL) Arg_do_funcs.numlst_flags=0; else
	     if (fin_de_param->next) fin_de_param->next->numlst_flags=0;
	   } else {
#ifdef USE_SLANG_LANGUAGE
	     res=get_command_command(ret-1,&slang_fun);
#else
	     res=get_command_command(ret-1);
#endif
	     if ((res>0) && (res & FLCMD_MACRO)) {
	        fl_strncpy(Str_macro,Arg_str,99);
	        fin_de_param=&Arg_do_funcs;
		if (fin_de_param->numlst_flags==0) fin_de_param=NULL;
		else
		  while ((fin_de_param->next) && 
			  (fin_de_param->next->numlst_flags))
		      fin_de_param=fin_de_param->next;
 	     }
	   }
	   if ((res >0) && (res & FLCMD_MACRO)) {
	     int num_macro= res ^FLCMD_MACRO;
	     res = Flcmd_macro[num_macro].cmd;
	     res = parse_arg_string(Flcmd_macro[num_macro].arg,res,0);
	     etat_loop.next_cmd = Flcmd_macro[num_macro].next_cmd;
	   }
	   if (res==-2) etat_loop.etat=3; else
	   if (res==FLCMD_UNDEF) 
	        { etat_loop.etat=2; etat_loop.num_message=-9; }
	   else  
#ifdef USE_SLANG_LANGUAGE
	   if (res>=NB_FLCMD) 
	       change=Execute_function_slang_command(res-NB_FLCMD, slang_fun);
	   else
#endif
	   {
	     if ((Flcmds[res].cmd_flags & CMD_NEED_GROUP) &&
		 (etat_loop.hors_struct & 8)) {
	       etat_loop.etat=2; etat_loop.num_message=-3; change=0;
	     } else
	       change=(*Flcmds[res].appel)(res);
	   }
	   quit=((res==FLCMD_QUIT) || (res==FLCMD_GQUT));
	   /* si on change de groupe VERS un article non existant, on ne */
	   /* change pas de groupe */
	   if ((change) && (etat_loop.num_futur_article!=0) &&
	       (etat_loop.num_futur_article<etat_loop.Newsgroup_nouveau->min)
	       && (etat_loop.num_futur_article!=-1)) {
	     change=0; etat_loop.hors_struct|=1;
	     etat_loop.etat=2; etat_loop.num_message=-12;
	  }
	}
      } else change=1;
      if (to_build) {
	/* on finit la liste... */
	cree_liste_end();
      }
      to_build=change;
      if ((change==1) && (etat_loop.Newsgroup_nouveau==NULL)) {
	etat_loop.num_futur_article=0;
	res=prochain_newsgroup(&(etat_loop.Newsgroup_nouveau));
	if (res==-1) {
	   to_build=0;
	   etat_loop.etat=1; etat_loop.num_message=1;
	   if (!(etat_loop.hors_struct & 8) && Article_courant) {
	     if (Newsgroup_courant->grp_flags & GROUP_UNSUBSCRIBED) {
	       detruit_liste(0);
	       Newsgroup_courant=NULL;
	       Article_deb=&Article_bidon;
	     } else
	     if (prochain_non_lu(0,&Article_courant,0,0)==0) {
	         etat_loop.etat=0;
	     } else etat_loop.hors_struct=3;
	   } 
	} else if (res==-2) {
	   to_build=0;
	   etat_loop.etat=2; etat_loop.num_message=-25;
	}
      }
      if ((Newsgroup_courant) && 
          (((to_build) && (Newsgroup_courant!=etat_loop.Newsgroup_nouveau))
	    || (quit))) {
          save_etat_loop();
	  apply_autocmd(AUTOCMD_LEAVE,Newsgroup_courant->name);
	  restore_etat_loop();
      }
      a_change=0;
      if (to_build==1) {
        if (Article_deb && (Article_deb!=&Article_bidon)) detruit_liste(0);
	if (Newsgroup_courant!=etat_loop.Newsgroup_nouveau) a_change=1;
	Newsgroup_courant=etat_loop.Newsgroup_nouveau;
      }
      if (Newsgroup_courant==NULL) etat_loop.hors_struct=11;
      change=0;
   } 
   if (Newsgroup_courant && Article_deb && (Article_deb!=&Article_bidon)) detruit_liste(0);
   return (res==FLCMD_QUIT);
}

void init_Flcmd_rev() {
  int i;
  struct key_entry key;
  /* l'initialisation à CMD_UNDEF est déjà faite */
  for (i=0;i<NB_FLCMD;i++) {
      if (Flcmds[i].key)
        add_cmd_for_slang_key(CONTEXT_COMMAND,i,Flcmds[i].key);
      if (Flcmds[i].key_nm)
        add_cmd_for_slang_key(CONTEXT_COMMAND,i,Flcmds[i].key_nm);
  }
  key.entry=ENTRY_SLANG_KEY;
  for (i=0;i<CMD_DEF_PLUS;i++) {
    key.value.slang_key=Cmd_Def_Plus[i].key;
#ifdef USE_SLANG_LANGUAGE
    Bind_command_new(&key,Cmd_Def_Plus[i].cmd,
	Cmd_Def_Plus[i].args,NULL,CONTEXT_COMMAND, Cmd_Def_Plus[i].add);
#else
    Bind_command_new(&key,Cmd_Def_Plus[i].cmd,
	Cmd_Def_Plus[i].args,CONTEXT_COMMAND, Cmd_Def_Plus[i].add);
#endif
  }
  return;
}

/* a supprimer dès que possible */
#if 0
int fonction_to_number(char *nom) {
  int i;
  for (i=0;i<NB_FLCMD;i++)
    if (strcmp(nom, Flcmds[i].nom)==0)
      return i;
  return -1;
}
#endif

/* FIXME : passer à l'éxecution d'une commande */
int call_func(int number, flrn_char *arg) {
  int res;
  *Arg_str='\0';
  Arg_do_funcs.numlst_flags=0;
  res=number;
  res=parse_arg_string(arg,res,0);
  return (*Flcmds[res].appel)(res);
}

void apply_autocmd(int flag, flrn_char *name) {
  autocmd_list_type *parcours=Options.user_autocmd;
  while (parcours) {
     if (flag & parcours->autocmd_flags) {
        if (fl_regexec(&(parcours->match),name,0,NULL,0)==0) {
	   call_func(parcours->cmd,parcours->arg);
	}
     }
     parcours=parcours->next;
  }
}

/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee */
/* -4 plus de macro disponible */
int Bind_command_explicite(flrn_char *nom, struct key_entry *key,
	flrn_char *arg, int add) {
  int i, res;
  Cmd_return commande;
#ifdef USE_SLANG_LANGUAGE
  commande.fun_slang=NULL;
#endif
  if (key==NULL) return -2;
  /* les commandes non rebindables... Je trouve ca moche */
  if (key->entry==ENTRY_SLANG_KEY) {
      int k=key->value.slang_key;
      for (i=0;i<NB_FLCMD;i++) 
        if (Flcmds[i].key_nm==k)
          return -3;
  }
  if (arg ==NULL) {
    if (fl_strcmp(nom, "undef")==0) {
	del_cmd_for_key(CONTEXT_COMMAND,key);
	/* FIXME : renvoyer mieux ? */
        return 0;
    }
  }
  commande.len_desc=0;
  res=Lit_cmd_explicite(nom, CONTEXT_COMMAND, -1, &commande);
  if (res==-1) return -1;
#ifdef USE_SLANG_LANGUAGE
  res=Bind_command_new(key, commande.cmd[CONTEXT_COMMAND], arg,
     commande.fun_slang, CONTEXT_COMMAND, add);
  if (commande.fun_slang) free(commande.fun_slang);
#else
  res=Bind_command_new(key, commande.cmd[CONTEXT_COMMAND], arg,
     CONTEXT_COMMAND, add);
#endif
  return (res<0 ? -4 : 0);
}


/* Decodage d'un numero pour le parsing */
/* Returne -1 (ou NULL) en cas d'echec */
/* flag = 0 : numéro, flag = 1 : article, flag = 2 : article, mais peut changer */
static union element Decode_numero(flrn_char *str, union element defaut,
	int *flag) {
   flrn_char *ptr=str;
   int trouve_inf=0, nombre=0;
   union element res;
   Article_List *parcours=NULL;

   if (*ptr==fl_static(',')) ptr++;
   if (*ptr==fl_static('\0')) return defaut;
   if (*ptr==fl_static('<')) { 
       ptr++; trouve_inf=1; if (*ptr==fl_static(',')) ptr++; 
   }
   if (*ptr==fl_static('\0')) { 
       res=defaut; 
       /* Dans le cas de trouve_inf, on prend parcours=Article_courant */
       if (*flag) parcours=res.article; else 
           parcours=Article_courant;
   } else 
       if (*ptr==fl_static('.')) 
       { 
	   if (*flag) res.article=Article_courant; 
	   else { res.number=0; parcours=Article_courant; }
	   ptr++;
       } 
   else {
     nombre=fl_strtol(ptr, &ptr, 10);
     if (nombre) Recherche_article (nombre, &parcours, 0); else
         parcours=Article_courant;
     if (*flag==0) res.number=nombre; else 
         res.article=parcours;
   }
   if (*ptr==fl_static(',')) ptr++;
   if ((!trouve_inf) && (*ptr==fl_static('<'))) { 
       ptr++; trouve_inf=1; if (*ptr==fl_static(',')) ptr++; 
   }
   if (*ptr!='\0') {
      if (*flag==0) res.number=-1; else res.article=NULL;
      return res; /* On doit etre au bout */
   }
   if (trouve_inf) {
      if (parcours==NULL) {
         if (*flag==0) res.number=-1; else res.article=NULL;
	 return res;
      }
      if (*flag) 
         res.article=root_of_thread(parcours,1); else {
	   parcours=root_of_thread(parcours,0);
	   if (parcours) res.number=parcours->numero; else res.number=-1;
	 }
  } else if ((*flag==2) && (nombre)) {
     /* On a juste trouve un nombre, on le garde */
     *flag=0;
     res.number=nombre;
  }
  return res;
}


/* Parsing des numeros d'articles
 * Arg_do_funcs.flag est a 0 s'il n'y a rien a parser...
 * On arrete des qu'il y a un problème.
 * On a deux règles spéciales : 0 est l'article courant,
 * 1 le premier article du groupe */
/* flags est du type cmd_flags */
static void Parse_nums_article(flrn_char *str, flrn_char **sortie, int flags) {
   flrn_char *ptr=str, *ptr2=NULL, *ptrsign;
   int reussi=1, flag;
   flrn_char save_char=fl_static('\0');
   union element defaut;
   Numeros_List *courant=&Arg_do_funcs;
   /* on conserve le debut de la liste. Pour les commandes explicites */
   while (courant->numlst_flags != 0) courant = courant->next;

   while (ptr && reussi) {
     ptr2=fl_strchr(ptr,fl_static(','));
     if (ptr2!=NULL) *ptr2=fl_static('\0'); else
        if (flags & CMD_LAST_IS_STR) {
	   courant->numlst_flags=0;
	   if (sortie) *sortie=ptr;
	   return;
	}
     if (*ptr==fl_static('\0')) {
	courant->numlst_flags=0;
     } else {
       ptrsign=fl_strchr(ptr,fl_static('-'));
       if (ptrsign!=NULL) 
       { 
         flag=0;
         *ptrsign=fl_static('\0');
         courant->numlst_flags=NUMLST_RNG;
	 defaut.number=1;
         courant->elem1=Decode_numero(ptr,defaut,&flag);
         if (courant->elem1.number==-1) reussi=0;
         *ptrsign='-';
	 defaut.number=(Newsgroup_courant ? Newsgroup_courant->max : 1);
         defaut=Decode_numero(ptrsign+1,defaut,&flag);
	 courant->num2=defaut.number;
         if (courant->num2==-1) reussi=0;
       } else { /* ce n'est pas '-' */
         flag=1;
         ptrsign=fl_strchr(ptr,fl_static('_'));
         if (ptrsign!=NULL) { 
	     save_char=fl_static('_'); courant->numlst_flags=NUMLST_ASC; 
	     *ptrsign=fl_static(','); 
	 } else {
  	     ptrsign=fl_strchr(ptr,fl_static('>'));
  	     if (ptrsign!=NULL) { 
		 save_char=fl_static('>'); courant->numlst_flags=NUMLST_DES;
		 *ptrsign=fl_static(','); 
	     } else {
	       ptrsign=strchr(ptr,'*');
	       if (ptrsign!=NULL)  {
		   save_char=fl_static('*'); courant->numlst_flags=NUMLST_THR; 
		   *ptrsign=fl_static(','); 
	       }
	       else { courant->numlst_flags=NUMLST_ART; flag=2; }
	     }
	   }
	 defaut.article=Article_courant;
         courant->elem1=Decode_numero(ptr, defaut,&flag);
         if (flag) { if (courant->elem1.article==NULL) reussi=0; }  else 
	 {
	    courant->numlst_flags=NUMLST_NUM;
	    if (courant->elem1.number==-1) reussi=0; 
	 }
         if (ptrsign!=NULL) *ptrsign=save_char;
       }
     }
     if ((flag==0) && (Article_courant && (courant->elem1.number==0))) 
        courant->elem1.number=Article_courant->numero;
     if ((flag==0) && (Article_deb && (courant->elem1.number==1))) {
	if ((Newsgroup_courant) &&
		  (Newsgroup_courant->grp_flags & GROUP_NOT_EXHAUSTED)) {
	    int ff=2;
	    cree_liste(1,&ff);
	}
        courant->elem1.number=Article_deb->numero;
     }
     if (Article_courant && (courant->num2==0)) 
        courant->num2=Article_courant->numero;
     if (Article_deb && (courant->num2==1)) 
        courant->num2=Article_deb->numero;
     if (reussi) {
        if (ptr2) { *ptr2=fl_static(','); ptr=ptr2+1; } else ptr=NULL;
        if (courant->next) courant=courant->next; else
	{ courant->next=safe_calloc(1,sizeof(Numeros_List));
	  courant=courant->next;
	}
	courant->numlst_flags=0;
     }
   }
   if (!reussi) {
     if (ptr2) *ptr2=fl_static(',');
     courant->numlst_flags=0;
   }
   if (sortie) *sortie=ptr;
}


/* appelé par get_command_command et d'autres */
/* annu16=1 : annuler le flag 16 si macro */
int parse_arg_string(flrn_char *str,int command, int annu16)
{
   int flag;
   int cmd;
   if (str) while (*str==fl_static(' ')) str++;
   if ((str==NULL) || (str[0]==fl_static('\0'))) return command;
   cmd=command;
   if (cmd<0) return cmd;
   if (cmd & FLCMD_MACRO) {
     cmd = Flcmd_macro[cmd ^ FLCMD_MACRO].cmd;
   }
#ifdef USE_SLANG_LANGUAGE
   if (cmd>=NB_FLCMD)
      flag=cmd-NB_FLCMD;
   else
#endif
      flag=Flcmds[cmd].cmd_flags & CMD_PARSE_FILTER;
   /* un truc quand même : le flag 16 n'a pas de valeur si macro et appele */
   /* depuis get_command_command */
   if ((annu16) && (command & FLCMD_MACRO)) 
       flag=flag & (CMD_TAKE_ARTICLES|CMD_TAKE_STRING);
   if ((flag & (CMD_TAKE_ARTICLES|CMD_TAKE_STRING))==0) return command;
   if (flag & CMD_TAKE_ARTICLES) Parse_nums_article(str, &str, flag);
   if (str) fl_strncpy(Arg_str, str, MAX_CHAR_STRING-1);
   return command;
}

/* Prend la chaine argument pour les appels qui en ont besoin en mode cbreak */
/* On ajoute key dans le cas ou isdigit(key) ou key=='<' */
/* Renvoie -1 si annulation */
static int get_str_arg(int res, Cmd_return *cmd, int tosave) {
   int col, ret;
   flrn_char cmd_line[MAX_CHAR_STRING];
   flrn_char *str=cmd_line;
   char affline[MAX_CHAR_STRING], *affptr=&(affline[0]);
   flrn_char *beg=cmd->before;

   *(str++)=fl_static('(');
   if (beg) {
      fl_strncpy(str,beg,MAX_CHAR_STRING-7-strlen(Flcmds[res].nom));
      str[MAX_CHAR_STRING-7-strlen(Flcmds[res].nom)]=fl_static('\0');
      str+=fl_strlen(str);
      *(str++)=fl_static('\\');
   }
   if (res<NB_FLCMD) fl_strcpy(str,Flcmds[res].nom);
       else fl_strcpy(str,fl_static("macro"));
   fl_strcat(str,fl_static(") : "));
   col=Aff_fin(cmd_line);
   str=cmd_line;
   str[0]=(flrn_char)0;
   affline[0]='\0';
   if (Flcmds[res].comp) {
     struct key_entry key,*keyp;
     int ret2;
     keyp=NULL;
     key.entry=ENTRY_ERROR_KEY;
     do {
	conversion_to_terminal(str,&affptr,MAX_CHAR_STRING,(size_t)(-1));
	Cursor_gotorc(Screen_Rows2-1,col); Screen_erase_eol();
	Screen_write_string(affptr);
	if ((ret=magic_flrn_getline(str,MAX_CHAR_STRING,
			affline,MAX_CHAR_STRING,Screen_Rows2-1,col,
			"\011",0,keyp,NULL))<0) return -1;
	if (ret>0) {
	    ret2=Comp_general_command(str, MAX_CHAR_STRING,col,
	       Flcmds[res].comp, Aff_ligne_comp_cmd, &key);
	    if (ret2>0) keyp=&key; else keyp=NULL;
	    if (ret2<0) ret2=0;
	}
     } while (ret!=0);
     Free_key_entry(&key);
   } else {
     ret=flrn_getline(str, MAX_CHAR_STRING, affline,
	     MAX_CHAR_STRING, Screen_Rows2-1, col);
     if (ret<0) return -1;
   }
   if ((tosave) && (*str)) {
      cmd->after=str;
      save_command(cmd);
   }
   if (Flcmds[res].cmd_flags & CMD_TAKE_ARTICLES) 
     Parse_nums_article(str,&str,Flcmds[res].cmd_flags & CMD_PARSE_FILTER);
   if (str) fl_strcpy(Arg_str, str); else Arg_str[0]=fl_static('\0');
   return 0;
}


/* Prend une commande pour loop... Renvoie le code de la commande frappe */
/* Renvoie -1 si commande non défini				         */
/*         -2 si rien							 */
/*         -3 si l'état est déjà défini...				 */
/*         -4 si on renvoie en fait une fonction slang                   */
int get_command_command(int get_com
#ifdef USE_SLANG_LANGUAGE
                        , SLang_Name_Type **slang_fun
#endif
) {
   int res, res2;
   struct key_entry key;
#ifdef USE_SLANG_LANGUAGE
   int a;
#endif

   key.entry=ENTRY_ERROR_KEY;
   Arg_do_funcs.numlst_flags=0;
   Arg_str[0]=fl_static('\0');

   if (get_com==-1) {
     num_help_line=thread_view;
     Aff_help_line(Screen_Rows-1);
     Aff_fin_utf8(_("Ã€ vous : "));
     Attend_touche(&key);
     if (KeyBoard_Quit) return -1;
     res=get_command(&key,CONTEXT_COMMAND,-1,&une_commande);
   } else res=get_com;
   if (res<0) {
      if ((res==-1) && (une_commande.cmd_ret_flags & CMD_RET_KEEP_DESC)) {
	  save_command(&une_commande);
      }
      if (une_commande.before) free(une_commande.before);
      if (une_commande.after) free(une_commande.after);
      return res;
   }
   /* res = 0 */
#ifdef USE_SLANG_LANGUAGE
   if (une_commande.fun_slang) {
      if (debug) { fprintf(stderr,"slang parse: %s\n",une_commande.fun_slang); }
      *slang_fun = Parse_fun_slang(une_commande.fun_slang, &a);
      free(une_commande.fun_slang);
      une_commande.fun_slang=NULL;
      if (*slang_fun==NULL) {
         if (une_commande.cmd_ret_flags & CMD_RET_KEEP_DESC) {
	    save_command(&une_commande);
         }
         if (une_commande.before) free(une_commande.before);
	 if (une_commande.after) free(une_commande.after);
	 return -1;
      }
      res2=NB_FLCMD+a; /* pour signifier une fonction slang ,
                    on pourra éventuellement penser à modifier ça, ensuite */
      une_commande.cmd_ret_flags &= ~CMD_RET_MAYBE_AFTER;
   } else
#endif
   res2=une_commande.cmd[CONTEXT_COMMAND];
   if (une_commande.before) 
      Parse_nums_article(une_commande.before,NULL,0);
   if (une_commande.after) {
      save_command(&une_commande);
      une_commande.cmd_ret_flags &= ~CMD_RET_KEEP_DESC;
      res2=parse_arg_string(une_commande.after,res2,1);
      free(une_commande.after);
   }
#ifdef USE_SLANG_LANGUAGE
   if ((res2!=FLCMD_UNDEF) && (res2 & FLCMD_MACRO)) {
       int res3,res4;
       res4 = res2 ^ FLCMD_MACRO;
       res3 = Flcmd_macro[res4].cmd;
       if (res3>=NB_FLCMD) {
            /* fonction SLANG */
	    *slang_fun = Parse_fun_slang(Flcmd_macro[res4].fun_slang, &a);
            if ((Flcmd_macro[res4].arg==NULL) && 
		 (((res3-NB_FLCMD) & 1) 
	      || ( ((!Options.forum_mode) && ((res3-NB_FLCMD) & 8))
              || ((Options.forum_mode) && ((res3-NB_FLCMD) & 4)) ))) {
		res=get_str_arg(res2,&une_commande,1);
		if (res==-1) res2=-2;
            }
       }
   }
   else
#endif
   if ((res2!=FLCMD_UNDEF) && ((res2 & FLCMD_MACRO)==0)) {
   /* Testons si on a besoin d'un (ou plusieurs) parametres */
     if ((une_commande.cmd_ret_flags & CMD_RET_MAYBE_AFTER) && 
         ( (Flcmds[res2].cmd_flags & (Options.forum_mode ? CMD_STR_FORUM :
				  CMD_STR_NOFORUM)) )) {
       res=get_str_arg(res2,&une_commande,1);
       if (res==-1) res2=-2;
     }
   }
   if (une_commande.before) free(une_commande.before);
   return res2;
}

static int tag_article(Article_List *art, void * flag)
{art->art_flags |= *(int *)flag; return 0;}
static int untag_article(Article_List *art, void * flag)
{art->art_flags &= *(int *)flag; return 0;}

/* do_deplace : deplacement */
int do_deplace(int res) {
   Article_List *parcours, *parcours2;
   int parc_eq_cour=0, peut_changer, ret=0;

   peut_changer=(etat_loop.hors_struct & 2) && (!Options.space_is_return);

   if (etat_loop.hors_struct & 8) {
     if (!peut_changer) {
       etat_loop.etat=2;
       etat_loop.num_message=-3;
     }
     return (peut_changer);
   }
   if ((Article_courant==&Article_bidon) && (!peut_changer)) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
   }
   parcours=Article_courant;
   /* dans le cas ou on est hors limite et num1=Article_courant->numero */
   /* on reaffiche Article_courant, a condition que ce ne soit ni - ni '\n' */
   /* ni un ' ' (sauf cas particuliers) */
   if (Article_courant && ((Arg_do_funcs.numlst_flags | NUMLST_SEL)==64)) {
      parc_eq_cour=1;
      if ((etat_loop.hors_struct & 1) && (res!=FLCMD_SPACE) &&
	  ((res!=FLCMD_PREC) || !(etat_loop.hors_struct & 4)) &&
	  ((res!=FLCMD_SUIV) || !(etat_loop.hors_struct & 2)))
      { etat_loop.hors_struct=0; 
        etat_loop.etat=0; etat_loop.num_message=0; 
        return 0; 
      }
   } else {
   /* on cheche Arg_do_funcs.num1 */
     if (Arg_do_funcs.numlst_flags & (NUMLST_RNG|NUMLST_NUM)) {
        ret=Recherche_article(Arg_do_funcs.elem1.number, &parcours, 
	   (res==FLCMD_PREC ? -1 : (res==FLCMD_SUIV ? 1 : 0)));
        if (ret==-2) {
          etat_loop.hors_struct=1;
          etat_loop.etat=1; etat_loop.num_message=3; return 0;
        }
     } else {
        parcours=Arg_do_funcs.elem1.article;
	ret=0;
     }
   }
   
   /* Je vois pas comment on peut éviter un case */
   parcours2=NULL;
   switch (res) {
     case FLCMD_PREC : if (ret!=-1) {
     			   if (parcours->numero==-1) {
			      if (parcours->parent>0)
			         parcours=parcours->pere; else
			         parcours=parcours->prem_fils;
			   } else
			   do {
			     parcours=parcours->prev;
			   } while(parcours && (parcours->art_flags 
				       & FLAG_KILLED));
			 }
			 break;
     case FLCMD_SUIV : if (ret!=-1) {
     			 if (parcours->numero==-1) {
			    if (parcours->prem_fils && 
			         (parcours->prem_fils->numero>0))
			       parcours=parcours->prem_fils; else
			       parcours=parcours->pere;
			    break;
			 }
			 parcours2=parcours->next; 
			 while(parcours2 && (parcours2->art_flags
				     & FLAG_KILLED)) {
			   parcours=parcours2;
			   parcours2=parcours2->next;
			 }
			 if ((parcours2==NULL) && (parcours->numero>0)) {
			     if (parcours->numero<Newsgroup_courant->max)
			       /* On peut avoir un article juste posté que */
			       /* Newnews ne detectera pas faute date...   */
			      parcours2=ajoute_message_par_num(
				  parcours->numero+1,
				  Newsgroup_courant->max);
			     if (parcours2==NULL) {
			       /* cherchons de nouveau messages */
			       ret=cherche_newnews();
			       if (ret==-2) { /* Alors pas bo */
				 etat_loop.Newsgroup_nouveau=Newsgroup_courant;
				 return 1;
			       } else if (ret>0) {parcours2=parcours->next;
				 while(parcours2 &&
				     (parcours2->art_flags &
				         FLAG_KILLED))
				   parcours2=parcours2->next;
			       }
			     }
			 }
			 parcours=parcours2;
		       }
		       break;
     case FLCMD_UP : parcours2=parcours->frere_prev; 
     		     if ((parcours2==NULL) && (Options.with_cousins))
		       parcours2=cousin_prev(parcours);
		     break;
     case FLCMD_LEFT : if (parcours->pere) parcours->pere->prem_fils=parcours; 
		  parcours2=parcours->pere; break;
     case FLCMD_RIGHT : parcours2=parcours->prem_fils; break;
     case FLCMD_DOWN : parcours2=parcours->frere_suiv; 
     		       if ((parcours2==NULL) && (Options.with_cousins))
		         parcours2=cousin_next(parcours);
		       break;
     case FLCMD_SPACE : ret=-prochain_non_lu(0, &parcours,0,1);
			if (ret==2) { 
			  etat_loop.Newsgroup_nouveau=Newsgroup_courant;
			  return 1;
		        } else if (ret==1) {
			  if (peut_changer) return 1; else
			  parcours=NULL;
			}
			break;
   }
   if ((res==FLCMD_UP) || (res==FLCMD_DOWN) || (res==FLCMD_LEFT) ||
       (res==FLCMD_RIGHT)) 
      if ((parcours2) /* || (Options.inexistant_arrow) */) parcours=parcours2;
   if (parcours==NULL) {
      etat_loop.hors_struct=1;
      if (parc_eq_cour) {
        if (res==FLCMD_PREC) etat_loop.hors_struct=5; else
	if ((res==FLCMD_SUIV) || (res==FLCMD_SPACE)) etat_loop.hors_struct=3;
      }
      etat_loop.etat=1; 
      etat_loop.num_message=(etat_loop.hors_struct & 2 ? 2 : 3);
   } else {
      Article_courant=parcours;
      etat_loop.etat=0;
   }
   return 0;
}

/* TODO : déplacer ce qui suit dans flrn_tags.c ? */
static int my_goto_tag (Flrn_Tag *tag) {
  int ret;
  union element ret_article;
  Newsgroup_List *ret_group;

  ret=goto_tag(tag,&ret_article,&ret_group);
  etat_loop.etat = (ret>=0 ? 0 : 1);
  if (ret<0) etat_loop.hors_struct|=1;
  if (ret>0) etat_loop.Newsgroup_nouveau=ret_group;
  switch (ret) {
     case 0 : Article_courant=ret_article.article;
              return 0;
     case 1 : etat_loop.Article_nouveau=ret_article.article;
     	      etat_loop.num_futur_article=-1;
              return 1;
     case 2 : etat_loop.num_futur_article=ret_article.number;
     	      return 1;
     case -1 : etat_loop.num_message=3;
	       return 0;
     case -2 : etat_loop.num_message=-8;
	       return 0;
     default : etat_loop.num_message=-13;
               return 0;
  }
}

int do_goto_tag(int res) {
  int ret;
  struct key_entry key;
  if (Arg_str[0]==fl_static('\0')) {
      memset(&key,0,sizeof(struct key_entry)); /* caractère 0 dans slang */
  } else parse_key_string(Arg_str,&key);
  ret=my_goto_tag(get_special_tags(&key));
  Free_key_entry (&key);
  return (ret > 0 ? 1 : 0);
}

static int my_tag_void(Article_List *article,void *vtag) {
  return put_tag(article, (Flrn_Tag *) vtag);
}

int do_tag (int res) {
  Flrn_Tag *tag;
  struct key_entry key;
  Numeros_List *courant=&Arg_do_funcs;
  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  if (Arg_str[0]==fl_static('\0')) {
      memset(&key,0,sizeof(struct key_entry)); /* caractère 0 dans slang */
  } else parse_key_string(Arg_str,&key);
  tag = add_special_tags(&key,NULL); /* on crée un tag "vide" */
  distribue_action(courant,my_tag_void,NULL,(void *)tag, 0);
  etat_loop.etat=1; etat_loop.num_message=15;
  return 0;
}

static int push_tag() {
  int res;
  if (tags_ptr>=0) {
    if (tags[tags_ptr].article==Article_courant) 
      return 0;
    /* On laisse le tag si ils sont bien egaux */
    if ((tags[tags_ptr].article_deb_key==-1) &&
        (tags[tags_ptr].newsgroup_name==Newsgroup_courant->name) &&
        (tags[tags_ptr].numero==Article_courant->numero)) {
        tags[tags_ptr].article_deb_key=Newsgroup_courant->article_deb_key;
        tags[tags_ptr].article=Article_courant;
        return 0;
    }
  }
  /* Le push est du à l'affichage d'un nouveau truc. Logiquement, on */
  /* reprend sur max_tags_ptr... */
  max_tags_ptr++;
  max_tags_ptr%=MAX_TAGS;
  tags_ptr=max_tags_ptr;
  res=put_tag(Article_courant,&(tags[tags_ptr]));
  return res;
}

int do_next(int res) {
  if (tags_ptr!=max_tags_ptr) {
    tags_ptr++;
    tags_ptr%=MAX_TAGS;
  }
  return my_goto_tag(&(tags[tags_ptr]));
}

int do_back(int res) {
  if ((etat_loop.hors_struct & 1)==0) { 
    tags_ptr += MAX_TAGS-1;
    tags_ptr %= MAX_TAGS;
    if ((tags_ptr==max_tags_ptr) || (tags[tags_ptr].article_deb_key==0)) {
      tags_ptr++;
      tags_ptr %= MAX_TAGS;
    }
  }
  return my_goto_tag(&(tags[tags_ptr]));
}

static int hist_menu_summary(void *item, flrn_char **line) {
  Flrn_Tag *tag = &tags[((int)(long)item)-1];
  Newsgroup_List *group;
  *line=safe_calloc(2*Screen_Cols,sizeof(flrn_char));
  if (is_tag_valid(tag,&group)==2) {
    Prepare_summary_line(tag->article,NULL,0,*line,2*Screen_Cols,
	    Screen_Cols-2,1,1,1);
  }
  return 1;
}

static struct format_elem_menu fmt_hist_menu_e [] =
    { { 0, 0, -1, 2, 0, 1 } };
struct format_menu fmt_hist_menu = { 1, &(fmt_hist_menu_e[0]) };
int do_hist_menu(int res) {
  int i;
  flrn_char *buf,*ptr;
  Liste_Menu *menu=NULL, *courant=NULL, *start=NULL;
  int valeur;
  int j, dup, bla;
  const char *special;
  flrn_char *trad;
  int rc;

  if (max_tags_ptr==-1) {
    etat_loop.etat=2; etat_loop.num_message=-15; 
    return 0;
  }
  bla=(max_tags_ptr+1)%MAX_TAGS;
  for (i=max_tags_ptr; tags[i].article_deb_key;
      i= (i+MAX_TAGS-1)%MAX_TAGS) {
    dup=0;
    for (j=max_tags_ptr; j!=i; j= (j+MAX_TAGS-1)%MAX_TAGS) {
      if (((tags[i].article_deb_key == -1) &&
           (tags[i].newsgroup_name == tags[j].newsgroup_name) &&
	   (tags[i].numero == tags[j].numero)) ||
	  ((tags[i].article_deb_key != -1) &&
	   (tags[i].article_deb_key == tags[j].article_deb_key) &&
	   (tags[i].article == tags[j].article))) {
	dup=1;
	break;
      }
    }
    if (dup) { if (i==bla) break; else continue; }
    buf=safe_malloc((fl_strlen(tags[i].newsgroup_name)+20)*sizeof(flrn_char));
    fl_strcpy(buf,tags[i].newsgroup_name);
    ptr=buf+fl_strlen(buf);
    if (tags[i].numero <0) {
	fl_strcat(ptr,fl_static(":?"));
    } else
        fl_snprintf(ptr,20,":%ld",tags[i].numero);
    courant=ajoute_menu(courant,&fmt_hist_menu,
	   &buf,(void *)(long)(i+1));
    /* buf sera directement utilisé, donc ne pas le libérer */
    if (tags[i].article==Article_courant) start=courant;
    if (!menu) menu=courant;
    if (i==bla) break;
  }
  if (!menu) {
    etat_loop.etat=2; etat_loop.num_message=-15; 
    return 0;
  }
  num_help_line=5;
  special=_("Historique. <entrÃ©e> pour choisir, q pour quitter...");
  rc=conversion_from_utf8(special,&trad,0,(size_t)(-1));
  valeur = (int)(long) 
      Menu_simple(menu, start, hist_menu_summary, NULL, 
	      trad);
  if (rc==0) free(trad);
  if (!valeur) {
    etat_loop.etat=3;
    return 0;
  }
  valeur --;
  Libere_menu(menu);
  tags_ptr=valeur;
  return my_goto_tag(&(tags[valeur]));
}

/* Aller dans un autre groupe */
int do_goto (int res) {
   int ret=-2;
 
   etat_loop.num_futur_article=0;
   if (Options.prefixe_groupe) {
      ret=change_group(&(etat_loop.Newsgroup_nouveau), (res==FLCMD_GGTO)|2,
                    Arg_str);
   }
   if (ret==-2) {
       ret=change_group(&(etat_loop.Newsgroup_nouveau), (res==FLCMD_GGTO),
                           Arg_str);
   }
   if ((ret==0) && (Arg_do_funcs.numlst_flags & (NUMLST_RNG|NUMLST_NUM)))
   	etat_loop.num_futur_article= Arg_do_funcs.elem1.number;
   if (ret>=0) return 1; else 
     if (ret==-1)  etat_loop.etat=3; else
   { etat_loop.etat=2; etat_loop.num_message=-8; 
     etat_loop.hors_struct|=1; }
   return 0;
}


/* do_unsubscribe : pour l'instant, j'ignore les arguments... J'aimerais */
/* bien pouvoir me desabonner a un ensemble de groupes...		 */
int do_unsubscribe(int res) {
   flrn_char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,2,str);
   if (ret==-2) ret=change_group(&newsgroup,0,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (ret==-1) {
      etat_loop.etat=3;
      return 0;
   }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   newsgroup->grp_flags |= GROUP_UNSUBSCRIBED;
   return (newsgroup==Newsgroup_courant);
}

/* do_abonne : pour l'instant, j'ignore les arguments...      J'aimerais */
/* bien pouvoir m'abonner a un ensemble de groupes...		 */
int do_abonne(int res) {
   flrn_char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,3,str);
   if (ret==-2) ret=change_group(&newsgroup,1,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (ret==-1) {
      etat_loop.etat=3;
      return 0;
   }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   newsgroup->grp_flags &= ~GROUP_UNSUBSCRIBED;
   if (Options.auto_kill) {
     add_to_main_list(newsgroup->name);
     newsgroup->grp_flags|=GROUP_IN_MAIN_LIST_FLAG;
   }
   if (newsgroup==Newsgroup_courant) Aff_newsgroup_name(0);
   etat_loop.etat=1; etat_loop.num_message=9;
   return 0;
}

int do_add_kill(int res) {
  flrn_char *str=Arg_str;
  int ret;
  Newsgroup_List *newsgroup=Newsgroup_courant;

  ret=change_group(&newsgroup,3,str);
  if (ret==-2) ret=change_group(&newsgroup,1,str);
  if (ret==-2) 
  { etat_loop.etat=2; etat_loop.num_message=-8;
    etat_loop.hors_struct|=1; return 0; }
   if (ret==-1) {
      etat_loop.etat=3;
      return 0;
   }
  if (newsgroup==NULL) {
     etat_loop.etat=2; etat_loop.num_message=-3;
     return 0;
  }
  add_to_main_list(newsgroup->name);
  newsgroup->grp_flags|=GROUP_IN_MAIN_LIST_FLAG;
  if (newsgroup==Newsgroup_courant) Aff_newsgroup_name(0);
  etat_loop.etat=1; etat_loop.num_message=18;
  return 0;
}

int do_remove_kill(int res) {
   flrn_char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,3,str);
   if (ret==-2) ret=change_group(&newsgroup,1,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (ret==-1) {
      etat_loop.etat=3;
      return 0;
   }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   remove_from_main_list(newsgroup->name);
   newsgroup->grp_flags&=~GROUP_IN_MAIN_LIST_FLAG;
   if (newsgroup==Newsgroup_courant) Aff_newsgroup_name(0);
   etat_loop.etat=1; etat_loop.num_message=19;
   return 0;
}

/* do_prem_grp : pour l'instant, place le groupe courant en première position */
/* c'est un peu léger pour ordonner le .flnewsrc, mais bon... */
int do_prem_grp (int res) {
   if (Newsgroup_courant->prev) {
     Newsgroup_courant->prev->next=Newsgroup_courant->next;
     Newsgroup_courant->next=Newsgroup_deb;
     Newsgroup_deb->prev=Newsgroup_courant;
     Newsgroup_deb=Newsgroup_courant;
     Newsgroup_deb->prev=NULL;
   }
   return 0;
}


/* des hack crades :-( */
int grand_distribue(Article_List *article, void * beurk) {
  Action_with_args *le_machin = (Action_with_args *) beurk;
  return thread_action(article,0,le_machin->action,le_machin->flag,0,le_machin->flags_to_omit);
}
int thread_distribue(Article_List *article, void * beurk) {
  Action_with_args *le_machin = (Action_with_args *) beurk;
  return thread_action(article,1,le_machin->action,le_machin->flag,0,le_machin->flags_to_omit);
}
int gthread_distribue(Article_List *article, void * beurk) {
  Action_with_args *le_machin = (Action_with_args *) beurk;
  return gthread_action(article,0,le_machin->action,le_machin->flag,0,le_machin->flags_to_omit);
}

static int omet_article(Article_List *article, void * toto)
{  if (article->art_flags & FLAG_KILLED) return 0;
   if ((article->numero>0) && (article->art_flags & FLAG_READ) &&
      (Newsgroup_courant->not_read!=-1)) {
        Newsgroup_courant->not_read++;
	article->thread->non_lu++;
      }
      article->art_flags &= ~FLAG_READ; return 0; }

/* Contrairement à article_read, on ne s'occupe pas des crossposts */
static int mark_article_read(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if ((article->numero>0) && (!(article->art_flags & FLAG_READ)) &&
      (Newsgroup_courant->not_read>0)) {
        Newsgroup_courant->not_read--;
	article->thread->non_lu--;
      }
   article->art_flags |= FLAG_READ; 
   if (article->art_flags & FLAG_IMPORTANT) {
      Newsgroup_courant->important--;
      article->art_flags &= ~FLAG_IMPORTANT;
   }
   article->art_flags |= flag; /* Si flag est FLAG_KILLED */
   return 0; 
}
static int mark_article_important(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if (article->art_flags & FLAG_READ) return 0;
   if (!(article->art_flags & FLAG_IMPORTANT)) Newsgroup_courant->important++;
   article->art_flags |= flag; 
   return 0; 
}
static int mark_article_unimportant(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if (article->art_flags & FLAG_IMPORTANT) Newsgroup_courant->important--;
   article->art_flags &= flag; 
   return 0; 
}

static void transforme_selection() {
   Article_List *parcours=Article_deb;

   while (parcours) {
      if (parcours->art_flags & FLAG_IS_SELECTED) 
              parcours->art_flags&= ~FLAG_IS_SELECTED;
      if (parcours->art_flags & FLAG_SUMMARY) {
          parcours->art_flags|=FLAG_IS_SELECTED;
	  parcours->art_flags&=~FLAG_SUMMARY;
      }
      parcours=parcours->next;
   }
   parcours=Article_exte_deb;
   while (parcours) {
      if (parcours->art_flags & FLAG_IS_SELECTED) 
              parcours->art_flags&= ~FLAG_IS_SELECTED;
      if (parcours->art_flags & FLAG_SUMMARY) {
          parcours->art_flags|=FLAG_IS_SELECTED;
	  parcours->art_flags&=~FLAG_SUMMARY;
      }
      parcours=parcours->next;
   }
}

int do_omet(int res) { 
  Numeros_List *courant=&Arg_do_funcs;
  Action_with_args beurk;
  int flag, col, use_summary=0;
  int toset;
  flrn_char *name=Arg_str;
  int use_argstr=0, ret=0;
  
  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  beurk.flags_to_omit = 0;
  if ((res==FLCMD_OMET) || (res==FLCMD_GOMT)) {
     beurk.action=omet_article;
     beurk.flag=NULL;
  } else {
     while ((*name) && (isblank(*name))) name++;
     if (name[0]==fl_static('\0')) {
       char *affchar;
       use_argstr=1;
       name=safe_malloc(40*sizeof(flrn_char));
       affchar=safe_malloc(40*sizeof(flrn_char));
       col=Aff_fin(fl_static("Flag: "));
       /* FIXME : complétion de flags ? */
       ret=flrn_getline(name, 39, affchar, 39,Screen_Rows2-1,col);
       free(affchar);
       if (ret<0) {
          free(name);
	  etat_loop.etat=3;
	  return 0;
       }
     }
     ret=parse_flags(name,&toset,&flag);
     if (ret<0) {
        if (use_argstr) free(name);
        etat_loop.etat=2; etat_loop.num_message=-19;
        return 0;
     }
     beurk.flag=&flag;
     if (toset) 
        beurk.action=tag_article;
     else 
        beurk.action=untag_article;
     switch (flag) {
        case FLAG_READ :
	    if (toset) {
     		beurk.action=mark_article_read;
	    } else {
     		beurk.action=omet_article;
     		beurk.flag=NULL;
		res=FLCMD_OMET;
	    }
	    break;
	case FLAG_KILLED :
	    if (toset) 
     		beurk.action=mark_article_read;
	    break;
	case FLAG_IMPORTANT :
	    if (toset) 
	        beurk.action=mark_article_important;
	    else beurk.action=mark_article_unimportant;
	    break;
     }
     if (toset==0) flag=~flag;
  }
  if ((flag==FLAG_IS_SELECTED) && (toset) && 
	  (courant->numlst_flags & NUMLST_SEL)) {
      flag=FLAG_SUMMARY;
      use_summary=1;
  }
  if (res==FLCMD_GOMT)
      distribue_action(courant,grand_distribue,NULL,&beurk, 0);
  else
      distribue_action(courant,beurk.action,NULL,beurk.flag, 0);
  if (use_summary) transforme_selection();
  Aff_not_read_newsgroup_courant();
  if (use_argstr) free(name);
  etat_loop.etat=1; etat_loop.num_message=(res==FLCMD_OMET ?
  				4 : (res==FLCMD_GOMT ? 5 : 23));
  return 0;
}

/* La, j'ai un doute sur la semantique
 * KILL tue des threads ? */
static int kill_article(Article_List *article, void * toto)
{ article_read(article); return 0; }
static int temp_kill_article(Article_List *article, void * toto)
{ article_read(article); 
  article->art_flags|=FLAG_WILL_BE_OMITTED;
  return 0; }
int do_kill(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  Action_with_args beurk;
  
  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  beurk.action=kill_article;
  beurk.flag=NULL;
  beurk.flags_to_omit = 0;
  if (res==FLCMD_KILL)
      distribue_action(courant,grand_distribue,NULL,&beurk, 0);
  else if (res==FLCMD_PKIL)
      distribue_action(courant,kill_article,NULL,NULL, 0);
  else if (res==FLCMD_ART_TO_RETURN) 
      distribue_action(courant,temp_kill_article,NULL,NULL, 0);
  else
  {
      beurk.flags_to_omit = FLAG_TOREAD;
      distribue_action(courant,thread_distribue,NULL,&beurk, 0);
  }
  /* Est-ce un hack trop crade ? */
  courant->numlst_flags=0;
  Aff_not_read_newsgroup_courant();
  if (res==FLCMD_ART_TO_RETURN) {
     etat_loop.etat=1;
     etat_loop.num_message=22;
     return 0;
  }
  if (Article_courant->art_flags & FLAG_READ) {
     etat_loop.hors_struct&=~2; /* Ceci sert à empêcher un changement */
     do_deplace(FLCMD_SPACE);
  }
  else etat_loop.etat=3;
  return 0;
}

/* Bon, on marque tous les articles lus, c'est pas optimal
 * Mais ca permet d'avoir un message avant de quitter le groupe
 * si l'on veut */
int do_zap_group(int res) {
  Numeros_List blah;
  int flag=FLAG_READ;
  Thread_List *parcours=Thread_deb;

  blah.next=NULL; blah.numlst_flags=NUMLST_RNG; blah.elem1.number=1;
  blah.num2=Newsgroup_courant->max;
  distribue_action(&blah,mark_article_read,NULL, &flag, FLAG_TOREAD);
  Recherche_article(Newsgroup_courant->max,&Article_courant,-1);
  if (!Options.zap_change_group) {
    etat_loop.hors_struct|=3; /* Fin du conti */
    etat_loop.etat=1; etat_loop.num_message=11;
  }
  while (parcours) { parcours->non_lu=0; parcours=parcours->next_thread; }
  Aff_not_read_newsgroup_courant();
  return (Options.zap_change_group)?1:0;
}

/* do_help : lance aide() */
int do_help(int res) { 
  etat_loop.etat=3;
  Aide();
  return 0; 
}

/* do_quit : ne fait rien */
int do_quit(int res) { 
  return 0; 
}

static int tag_and_minmax_article(Article_List *article, void *blah)
{
   article->art_flags|=FLAG_TMP_KILL;
   if ((article->numero<min_kill_l) || (min_kill_l==-1))
     min_kill_l=article->numero;
   if (article->numero>max_kill_l) max_kill_l=article->numero;
   return 0;
}

static int check_and_tag_article(Article_List *article, void *blah)
{
  flrn_filter *arg = (flrn_filter *) blah;
  if (check_article(article,arg,1)<=0)
  { tag_article(article,& arg->action.art_flag); return 1; }
  else return 0;
}


/* do_summary Il faut toujours appeler Aff_summary
 * car Aff_summary retire le FLAG_SUMMARY
 * La, on a des comportements par défaut non triviaux, et qui
 * en plus ont le mauvais gout de dependre de la commande */
static int Do_aff_summary_line(Article_List *art, int *row,
    flrn_char *previous_subject, int level, Liste_Menu **courant) {
  return Aff_summary_line(art,row,previous_subject,level);
}

static struct format_elem_menu fmt_summ_menu_e [] =
    { { 4, 4, 1, 2, 0, 7 }, { 0, 0, -1, 4, 0, 1 } };
struct format_menu fmt_summ_menu = { 2, &(fmt_summ_menu_e[0]) };
static int Do_menu_summary_line(Article_List *art, int *row,
    flrn_char *previous_subject, int level, Liste_Menu **courant) {
  flrn_char *lelem[2];
  lelem[1]=safe_calloc(2*Screen_Cols,sizeof(flrn_char));
  lelem[0]=((art->art_flags & FLAG_READ) ? fl_static("") : fl_static("*"));
  Prepare_summary_line(art,previous_subject,level, lelem[1],2*Screen_Cols,
	  Screen_Cols-2,4,0,0);
  *courant=ajoute_menu(*courant,&fmt_summ_menu,&(lelem[0]),art);
  return 0;
}

static int flags_summ_article (void *letruc, flrn_char **lachaine) {
   Article_List *larticle=letruc;
   flrn_char *buf;
   *lachaine=buf=safe_calloc(80,sizeof(flrn_char));
   fl_strcpy(buf,((larticle->art_flags & FLAG_READ) ? 
	       fl_static("  ") : fl_static("un")));
   fl_strcat(buf,fl_static("read"));
   fl_strcat(buf,((larticle->art_flags & FLAG_KILLED) ? 
	       fl_static("   ") : fl_static(" un")));
   fl_strcat(buf,fl_static("killed"));
   fl_strcat(buf,((larticle->art_flags & FLAG_IS_SELECTED) ? 
	       fl_static("   ") : fl_static(" un")));
   fl_strcat(buf,fl_static("selected"));
   fl_strcat(buf,((larticle->art_flags & FLAG_IMPORTANT) ?
	       fl_static("   ") : fl_static(" un")));
   fl_strcat(buf,fl_static("interesting"));
   fl_strcat(buf,((larticle->art_flags & FLAG_TOREAD) ? 
	       fl_static("   ") : fl_static(" un")));
   fl_strcat(buf,fl_static("toread"));
   if (larticle->headers==NULL) return 1;
   fl_strcat(buf,fl_static("  Lines: "));
   buf=buf+58;
   if (larticle->headers->nb_lines) {
     fl_snprintf(buf,20,"%i",larticle->headers->nb_lines);
   } else {
      if (larticle->headers->k_headers[LINES_HEADER]) {
        fl_strncpy(buf,larticle->headers->k_headers[LINES_HEADER],20);
      } else fl_strcpy(buf,"?");
   }
   return 1;
}

static int tag_article_summ (Liste_Menu *courant, char *arg) {
   Article_List *larticle=courant->lobjet;
   int ret, toset, flag;
   int flag_base=larticle->art_flags;

   if ((arg==NULL) || ((ret=parse_flags(arg,&toset,&flag))<0)) 
       return 0;
        /* FIXME : reprendre put_flag, et/ou
         *         demander à rentrer un autre flag */
        /* TODO : après réflexion ce truc est à chier : le parse
	 *         du flag est fait, fait et refait autant de fois que
	 *         demandé. C'est trop ! */
   if (toset==0) ret=~flag;
   switch (flag) {
   	case FLAG_READ :
	    if (toset) 
	    	mark_article_read(larticle, &flag);
	    else omet_article(larticle, &ret);
	    break;
	case FLAG_KILLED :
	    if (toset) 
	        mark_article_read(larticle, &flag);
	    else untag_article(larticle, &ret);
	    break;
	case FLAG_IMPORTANT :
	    if (toset) 
	        mark_article_important(larticle,&flag);
	    else mark_article_unimportant(larticle,&ret);
	    break;
	default :
	    if (toset) tag_article(larticle,&flag);
	       else untag_article(larticle,&ret);
	    break;
   }
   if (flag_base!=larticle->art_flags) {
       change_menu_line(courant,0,(larticle->art_flags & FLAG_READ ? 
		   fl_static(" ") : fl_static("*")));
   }
   return (flag_base!=larticle->art_flags);
}

static int summary_menu (Liste_Menu *debut_menu, Liste_Menu **courant,
	flrn_char **name, int *tofree, Cmd_return *la_commande) {
   int ret,cmd,rc;
   Action_on_menu action=tag_article_summ;
   flrn_char *cst_char=la_commande->after;

   *name=NULL;
   *tofree=0;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) return -1; else
   cmd=la_commande->cmd[CONTEXT_COMMAND];
   switch (cmd) {
      case FLCMD_KILL :
      case FLCMD_GKIL :
      case FLCMD_PKIL : cst_char=fl_static("read");
      			break;
      case FLCMD_OMET :
      case FLCMD_GOMT : cst_char=fl_static("unread");
      			break;
      case FLCMD_PUT_FLAG : break;
      default : rc=conversion_from_utf8(_(Messages[MES_UNKNOWN_CMD]),
			name,0,(size_t)(-1));
		*tofree=(rc==0);
		return 0;
   }
   ret=distribue_action_in_menu(la_commande->before, cst_char,
   		debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   return ret;
}

static Article_List * raw_Do_summary (int deb, int fin, int thread,
    int action(Article_List *, int *, flrn_char *, int, Liste_Menu **)) {
  Article_List *parcours;
  Article_List *parcours2;
  flrn_char *previous_subject=NULL;
  int level=1;
  int row=0;
  Liste_Menu *courant=NULL, *menu=NULL, *start=NULL;

  /* On DOIT effacer l'écran pour les commandes summary de base... */
  Cursor_gotorc(1,0); 
  Screen_erase_eos();
  num_help_line=6;
  Aff_help_line(Screen_Rows-1);
  Cursor_gotorc(1,0); 
  /* find first article */
  parcours=Article_deb;
  while (parcours && (parcours->numero<deb)) parcours=parcours->next;
  while (parcours && !(parcours->art_flags & FLAG_SUMMARY))
    parcours=parcours->next;
  if (parcours && (thread || !Options.ordered_summary)) {
    parcours=root_of_thread(parcours,1);
    if (!(parcours->art_flags & FLAG_SUMMARY))
       parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	   FLAG_SUMMARY,1);
  }
  parcours2=parcours;

  while (parcours && (!fin || (parcours->numero<=fin))) {
    if ((*action)(parcours,&row,previous_subject,level,&courant)) {
      if(menu) Libere_menu(menu); /* ne doit jamais arriver */
      return NULL;
    }
    if ((courant) && (!menu)) menu=courant;
    if ((courant) && (!start) && (parcours==Article_courant)) start=courant;
    if ((!Options.duplicate_subject) && (parcours->headers))
      previous_subject=parcours->headers->k_headers[SUBJECT_HEADER];
    parcours->art_flags &= ~FLAG_SUMMARY;
    if (thread || !Options.ordered_summary) {
      parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	  FLAG_SUMMARY,1);
    }
    if (!parcours || !(parcours->art_flags & FLAG_SUMMARY)) {
      while(parcours2 && (!(parcours2->art_flags & FLAG_SUMMARY)))
	parcours2=parcours2->next;
      parcours=parcours2; level=1;
      if (parcours && (thread || !Options.ordered_summary)) {
        parcours=root_of_thread(parcours,1);
        if (!(parcours->art_flags & FLAG_SUMMARY))
          parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	     FLAG_SUMMARY,1);
      }
    }
  }
  /* on jouait avec un menu */
  if(menu) {
    int rc;
    const char *special;
    flrn_char *trad;
    special=_("RÃ©sumÃ© (q pour quitter).");
    rc=conversion_from_utf8(special,&trad,0,(size_t)(-1));
    parcours=Menu_simple(menu, start, flags_summ_article, summary_menu, 
	    trad);
    if (rc==0) free(trad);
    Libere_menu(menu);
    return parcours;
  }
  num_help_line=thread_view;
  Aff_help_line(Screen_Rows-1);
  return NULL;
}

/* Gestion de la commande 'r' */
/* on affiche les messages taggues entre deb et fin */
/* deb=fin=0 signifie qu'on a fait r entree */
static void Aff_summary (int deb, int fin, int thread) {
  raw_Do_summary(deb,fin,thread,Do_aff_summary_line);
}

Article_List * Menu_summary (int deb, int fin, int thread) {
  return raw_Do_summary(deb,fin,thread,Do_menu_summary_line);
}

/* pour thread_menu */
static int tag_thread_in (Liste_Menu *courant, flrn_char *arg) {
   Thread_List *thr=(Thread_List *)(courant->lobjet);
   int ret, toset, flag;
   int oldflags=thr->thr_flags;
   
   ret=parse_flags(arg,&toset,&flag);
   if (ret<0) return 0;
   switch (flag) {
      case FLAG_KILLED :
      case FLAG_READ : if (toset) {
       			  if (thr->thr_flags & FLAG_THREAD_UNREAD) 
      				thr->thr_flags&=~FLAG_THREAD_UNREAD;
			  else thr->thr_flags|=FLAG_THREAD_READ;
		       } else {
       			  if (thr->thr_flags & FLAG_THREAD_READ) 
      				thr->thr_flags&=~FLAG_THREAD_READ;
			  else thr->thr_flags|=FLAG_THREAD_UNREAD;
		       }
		       break;
      case FLAG_IMPORTANT : 
      		       if (toset) 
			  thr->thr_flags|=FLAG_THREAD_IMPORTANT;
		       else
      			  thr->thr_flags&=~FLAG_THREAD_IMPORTANT;
		       break;
       case FLAG_IS_SELECTED :
      		       if (toset) 
			  thr->thr_flags|=FLAG_THREAD_BESELECTED;
		       else
      			  thr->thr_flags&=~FLAG_THREAD_BESELECTED;
		       break;
   }
   if (oldflags!=thr->thr_flags) { 
       change_menu_line(courant,0,
	       (thr->thr_flags & FLAG_THREAD_READ ? fl_static("-") :
		(thr->thr_flags & FLAG_THREAD_UNREAD ? fl_static("+") : 
		 (thr->thr_flags & FLAG_THREAD_IMPORTANT ? fl_static("I") :
		  (thr->thr_flags & FLAG_THREAD_BESELECTED ? fl_static("s")
		   : fl_static(" "))))));
       return 1;
   }
   return 0;
}

static int thread_menu (Liste_Menu *debut_menu, Liste_Menu **courant, flrn_char **name, int *tofree, Cmd_return *la_commande) {
   int ret,cmd,rc;
   Action_on_menu action=tag_thread_in;
   flrn_char *cst_char=la_commande->after;

   *tofree=0;
   *name=NULL;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) return -1; else
   cmd=la_commande->cmd[CONTEXT_COMMAND];

   switch (cmd) {
      case FLCMD_KILL :
      case FLCMD_GKIL :
      case FLCMD_PKIL : cst_char=fl_static("read");
      			break;
      case FLCMD_OMET :
      case FLCMD_GOMT : cst_char=fl_static("unread");
			break;
      case FLCMD_PUT_FLAG : break;
      default : rc=conversion_from_utf8(_(Messages[MES_UNKNOWN_CMD]),
			name,0,(size_t)(-1));
		*tofree=(rc==0);
		return 0;
   }
   ret=distribue_action_in_menu(la_commande->before, cst_char,
   		debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   return ret;
}

static struct format_elem_menu fmt_thread_menu_e [] =
    { { 4, 4, 1, 2, 0, 7 }, { 0, 0, 5, 4, 0, 1 },
      { 0, 0, 5, 9, 0, 1 }, { 0, 0, -1, 13, 0, 7} };
struct format_menu fmt_thread_menu = { 4, &(fmt_thread_menu_e[0]) };
static struct format_elem_menu fmt_thread2_menu_e [] =
    { { 4, 4, 1, 2, 0, 7 }, { 0, 0, -1, 13, 0, 7} };
struct format_menu fmt_thread2_menu = { 2, &(fmt_thread2_menu_e[0]) };
static int Menu_selector (Thread_List **retour) {
   Thread_List *parcours=Thread_deb;
   Hash_List *hash_parc;
   Article_List *art;
   Liste_Menu *courant=NULL, *menu=NULL, *start=NULL;
   int non_lu, lu, impor, selec, flag=FLAG_IMPORTANT;
   flrn_char *lelem[4];
   struct format_menu *fmtm;

   if (Screen_Cols>17) {
       fmtm=&fmt_thread_menu;
       lelem[1]=safe_malloc(6*sizeof(flrn_char));
       lelem[2]=safe_malloc(6*sizeof(flrn_char));
   } else fmtm=&fmt_thread2_menu;
   lelem[0]=fl_static("");
   for (;parcours;parcours=parcours->next_thread) {
      parcours->thr_flags&=~FLAG_THREAD_READ;
      parcours->thr_flags&=~FLAG_THREAD_UNREAD;
      parcours->thr_flags&=~FLAG_THREAD_IMPORTANT;
      parcours->thr_flags&=~FLAG_THREAD_BESELECTED;
      if (!(parcours->thr_flags & FLAG_THREAD_SELECTED)) continue;
      hash_parc=parcours->premier_hash;
      while ((hash_parc) && ((hash_parc->article==NULL) 
                         || (hash_parc->article->numero<=0)))
         hash_parc=hash_parc->next_in_thread;
      if (hash_parc==NULL) continue;
      art=root_of_thread(hash_parc->article,0);
      if ((art->headers==NULL) ||
          (art->headers->k_headers_checked[SUBJECT_HEADER] == 0))
        cree_header(art,0,0,0);
      if (art->headers==NULL)  continue;
      if (fmtm==&fmt_thread_menu) {
        if (parcours->number>9999) 
	    fl_strcpy(lelem[1],fl_static(" ***"));
	else
            fl_snprintf(lelem[1],6,fl_static("%4d"),parcours->number);
        if (parcours->non_lu>9999) 
	    fl_strcpy(lelem[2],fl_static(" ***"));
	else
            fl_snprintf(lelem[2],5,fl_static("%4d"),parcours->non_lu);
	lelem[3]=art->headers->k_headers[SUBJECT_HEADER];
      } else lelem[1]=art->headers->k_headers[SUBJECT_HEADER];
      courant=ajoute_menu(courant,fmtm,&(lelem[0]),(void *)parcours);
      if (Article_courant->thread==parcours) start=courant;
      if (!menu) menu=courant;
   }
   if (fmtm==&fmt_thread_menu) {
       free(lelem[1]);
       free(lelem[2]);
   }
    
   if (menu) {
      int rc;
      flrn_char *trad;
      const char *special;
      special=_("<total> <non lus>. q pour quitter.");
      rc=conversion_from_utf8(special, &trad, 0, (size_t)(-1));
      num_help_line=7;
      (*retour)=(Thread_List *)Menu_simple(menu,start,NULL,thread_menu,
					   trad);
      if (rc==0) free(trad);
      Libere_menu(menu);
   } else return -1;
   parcours=Thread_deb;
   for (;parcours;parcours=parcours->next_thread) {
       if (parcours->thr_flags &
	       (FLAG_THREAD_UNREAD | FLAG_THREAD_READ |
		FLAG_THREAD_IMPORTANT | FLAG_THREAD_BESELECTED)) {
         non_lu=parcours->thr_flags & FLAG_THREAD_UNREAD;
         lu=parcours->thr_flags & FLAG_THREAD_READ;
	 impor=parcours->thr_flags & FLAG_THREAD_IMPORTANT;
	 selec=parcours->thr_flags & FLAG_THREAD_BESELECTED;
         for (hash_parc=parcours->premier_hash;hash_parc;
	      hash_parc=hash_parc->next_in_thread) 
	    if (hash_parc->article) {
	       if (non_lu) omet_article(hash_parc->article,NULL);
	       else if ((lu) && ((hash_parc->article->numero>0) &&
	                (!(hash_parc->article->art_flags && FLAG_READ))))
	           kill_article(hash_parc->article,NULL);
	/* On évite de faire des requetes superflues et lourdes */
	       if (impor)
	          mark_article_important(hash_parc->article,(void *)(&flag));
	       if (selec)
	          hash_parc->article->art_flags|=FLAG_IS_SELECTED;
	    }
       }
   }
   return 0;
}


static int default_thread(Article_List *article, void *flag)
{
  Numeros_List blah;
  blah.next=NULL;
  blah.numlst_flags=NUMLST_DES;
  blah.elem1.article=article;
  distribue_action(&blah,(Action) tag_and_minmax_article,NULL,flag, 0);
  return 1;
}
static int default_gthread(Article_List *article, void *flag)
{ return default_thread(root_of_thread(article,1), flag);
}
static int default_summary(Article_List *article, void *flag)
{
  Numeros_List blah;
  Article_List *parcours, *parcours2;
  int count=Screen_Rows - 2* Options.skip_line - 3;
  blah.next=NULL;
  blah.numlst_flags=NUMLST_NUM;
  parcours=article;
  parcours2=article;
  if (check_and_tag_article(parcours,flag)) count --;
  while ( count != 0) {
    if ((count) && (parcours->prev)) {
      parcours=parcours->prev;
      if (check_and_tag_article(parcours,flag)) count --;
    }
    if ((count) && (parcours2->next)) {
      parcours2=parcours2->next;
      if (check_and_tag_article(parcours2,flag)) count --;
    }
    if (!parcours->prev && !parcours2->next) break;
  }
  return 0;
}

int do_summary(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  int result;
  Action defact=NULL;
  Action act=NULL;
  flrn_filter *filt;
  Article_List *ret;
  flrn_char *buf=Arg_str;

  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  min_kill_l=max_kill_l=-1;
  filt=new_filter();
  filt->action.art_flag=FLAG_SUMMARY;
  act=tag_and_minmax_article;
  if (Arg_str && *Arg_str) {
    while(*buf==fl_static(' ')) buf++;
    if (parse_filter_flags(buf,filt))
      parse_filter(buf,filt);
  }
  switch(res) {
    case FLCMD_MENUTHRE:
    case FLCMD_THRE: act=defact=default_thread;
		     break;
    case FLCMD_MENUGTHR:
    case FLCMD_GTHR: act=defact=default_gthread;
		     break;
    default : defact=default_summary;
  }
  result = distribue_action(courant, act, defact, (void *)filt, 0);
  if (filt) 
     result=check_article_list(Article_deb,filt,3,min_kill_l,max_kill_l);
  else result=1;
  /* La suite n'est pas necessaire en cas de default_summary, mais tant pis */
  if (result) {
     ret=Article_deb;
     while ((ret) && (ret->numero<=max_kill_l)) {
       if (ret->art_flags & FLAG_TMP_KILL) {
          ret->art_flags |= FLAG_SUMMARY;
          ret->art_flags &= ~FLAG_TMP_KILL;
       }
       ret=ret->next;
     }
  }
  free_filter(filt);
  ret=NULL;
  switch(res) {
    case FLCMD_MENUTHRE:
    case FLCMD_MENUSUMM:
    case FLCMD_MENUGTHR:
    case FLCMD_MENUSUMMS:
      ret=Menu_summary(0,0,((res==FLCMD_MENUTHRE)||(res==FLCMD_MENUGTHR))?1:0);
      break;
    default:
      Aff_summary(0,0,((res==FLCMD_THRE)||(res==FLCMD_GTHR))?1:0);
  }
  etat_loop.etat=3;
  if(ret) {
    Article_courant=ret;
    etat_loop.etat=0;
  }
  return 0;
}

int do_select(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  Hash_List *hash_parc;
  int result;
  Action act=NULL;
  flrn_filter *filt;
  flrn_char *buf=Arg_str;
  Thread_List *parcours=Thread_deb, *retour=NULL;
  Article_List *art_ret;

  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  min_kill_l=max_kill_l=-1;
  filt=new_filter();
  act=tag_and_minmax_article;
  if (Arg_str && *Arg_str) {
    while(*buf==fl_static(' ')) buf++;
    if (parse_filter_flags(buf,filt))
      parse_filter(buf,filt);
  }
  result = distribue_action(courant, act, NULL, NULL, 0);
  if (filt)
     result=check_article_list(Article_deb,filt,3,min_kill_l,max_kill_l);
  else result=1;
  if (result) {
     art_ret=Article_deb;
     while ((art_ret) && (art_ret->numero<=max_kill_l)) {
       if (art_ret->art_flags & FLAG_TMP_KILL) {
          art_ret->thread->thr_flags|=FLAG_THREAD_SELECTED;
          art_ret->art_flags &= ~FLAG_TMP_KILL;
       }
       art_ret=art_ret->next;
     }
  }
  free_filter(filt);
  art_ret=NULL;
  result=Menu_selector(&retour);
  if (result==-1) {
     etat_loop.hors_struct|=1;
     etat_loop.etat=1;
     etat_loop.num_message=10;
     return 0;
  }
  while (parcours) {
    parcours->thr_flags &= ~FLAG_THREAD_SELECTED;
    parcours=parcours->next_thread;
  }
  etat_loop.etat=0;
  /* Est-ce un hack trop crade ? */
  courant->numlst_flags=0;
  Aff_not_read_newsgroup_courant();
  if (retour) {
      for (hash_parc=retour->premier_hash; hash_parc;
              hash_parc=hash_parc->next_in_thread) {
	 if (!hash_parc->article) continue;
	 if ((art_ret==NULL) || (!(hash_parc->article->art_flags & FLAG_READ))) 
	     art_ret=hash_parc->article; else continue;
	 if ((art_ret) && (!(art_ret->art_flags & FLAG_READ))) break;
      }
      if (art_ret) {
         Article_courant=root_of_thread(art_ret,1);
	 if ((!(art_ret->art_flags & FLAG_READ)) && 
	       ((Article_courant->art_flags & FLAG_READ) ||
		(Article_courant->numero<0))) {
	    Article_List *myarticle=Article_courant;
	    while (myarticle && (myarticle->numero<0)) {
	       myarticle=myarticle->prem_fils;
	       while (myarticle && (myarticle->frere_prev))
	           myarticle=myarticle->frere_prev;
	    }
	    if (myarticle) myarticle=next_in_thread(myarticle,FLAG_READ,NULL,
	    				0,0,0,1);
	    if (myarticle) Article_courant=myarticle;
	 }
      }
  } else
  if (Article_courant->art_flags & FLAG_READ) {
     etat_loop.hors_struct&=~2;
     do_deplace(FLCMD_SPACE);
  }
  else etat_loop.etat=3;
  return 0;
}

static int Sauve_article(Article_List *a_sauver, void *fichier, int flag);

/* do_save : on va essayer de voir... */
int do_save(int res) { 
  FILE *fichier;
  flrn_char *name=Arg_str;
  char *filenom;
  int ret, use_argstr=0, col, rc;
  struct stat status;
  Numeros_List *courant=&Arg_do_funcs;

  int Sauve_article_do_save(Article_List *a_sauver, void *vfichier) {
      return Sauve_article(a_sauver, vfichier, 0);
  }

  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  while ((*name) && (fl_isblank(*name))) name++;
  if (name[0]==fl_static('\0')) {
    char *affname;
    use_argstr=1;
    name=safe_malloc(MAX_PATH_LEN*sizeof(flrn_char));
    affname=safe_malloc(MAX_PATH_LEN);
    fl_strcpy(name, Newsgroup_courant->name);
    name[0]=fl_toupper(name[0]);
    conversion_to_terminal(name,&affname,MAX_PATH_LEN,(size_t)(-1));
    col=Aff_fin_utf8(_("Sauver dans : "));
    Screen_write_string(affname);
    if ((ret=flrn_getline(name, MAX_PATH_LEN, affname,
		    MAX_PATH_LEN,Screen_Rows2-1,col)<0)) {
      free(name);
      free(affname);
      etat_loop.etat=3;
      return 0;
    }
    free(affname);
  }
  if (name[0]!=fl_static('/') && 
	  (Options.savepath[0]!=fl_static('\0'))) {
    /* on n'a pas un path complet */
    flrn_char *fullname=
	safe_malloc((fl_strlen(name)+fl_strlen(Options.savepath)+2)
		*sizeof(flrn_char));
    fl_strcpy(fullname,Options.savepath);
    fl_strcat(fullname,"/");
    fl_strcat(fullname,name);
    if (use_argstr) free(name);
    use_argstr=1;
    name=fullname;
  }
  rc=conversion_to_file(name,&filenom,0,(size_t)(-1));
  if (stat(filenom,&status)==0) {
      struct key_entry key;
      int k;
      const char *special;
      key.entry=ENTRY_ERROR_KEY;
      if (res==FLCMD_SAVE_OPT) 
	  special=_("Ã‰craser le fichier ? ");
      else special=_("Ajouter au folder ? ");
      Aff_fin_utf8(special);
      Attend_touche(&key);
      if (key.entry==ENTRY_SLANG_KEY) k=key.value.slang_key;
      else k=(int)'n';
      if ((KeyBoard_Quit) || (strchr(_("yYoO"), k)==NULL))  {
	if (rc==0) free(filenom);
        etat_loop.etat=3;
        return 0;
      }
  }
  if (res==FLCMD_SAVE_OPT)
    fichier=fopen(filenom,"w");
  else fichier=fopen(filenom,"a");
  if (rc==0) free(filenom);
  if (use_argstr) free(name);
  if (fichier==NULL) {
    etat_loop.etat=2; etat_loop.num_message=-6;
    return 0;
  }
  if (res==FLCMD_SAVE_OPT) {
    dump_flrnrc(fichier);
    fclose(fichier);
    etat_loop.etat=1; etat_loop.num_message=14; /* la flemme */
    return 0;
  }
  if (res==FLCMD_SAVE)
    distribue_action(courant,Sauve_article_do_save,NULL ,fichier, 0);
  else {
    Action_with_args beurk;

    beurk.action=Sauve_article_do_save;
    beurk.flag=fichier;
    beurk.flags_to_omit = 0;
    distribue_action(courant,grand_distribue, NULL, &beurk, 0);
  }
  fclose(fichier);
  etat_loop.etat=1; etat_loop.num_message=8;
  return 0;
}

/* do_launch_pager : voisin de do_save */
int do_launch_pager(int res) { 
  FILE *fichier;
  Numeros_List *courant=&Arg_do_funcs;
  int fd;
  char name[MAX_PATH_LEN];

  int Sauve_article_do_launch_pager(Article_List *a_sauver, void *vfichier) {
      return Sauve_article(a_sauver, vfichier, 1);
  }

  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  fd=Pipe_Msg_Start(1,0,NULL,name);
  fichier=fdopen(fd,"w");
  if (fichier==NULL) {
    etat_loop.etat=2; etat_loop.num_message=-6;
    if (fd >=0)
      Pipe_Msg_Stop(fd);
    return 0;
  }
  distribue_action(courant,Sauve_article_do_launch_pager,NULL,fichier, 0);

  if (fd>0) fclose(fichier);
  Pipe_Msg_Stop(fd);
#ifdef USE_MKSTEMP
  unlink(name);
#endif
  etat_loop.etat=3;
  return 0;
}

/* lit le fichier temporaire créé par un filtre */
/* appelée par do_pipe */
int display_filter_file(flrn_char *cmd, int flag, char *name) {
  FILE *file;
  flrn_char prettycmd[MAX_CHAR_STRING];
  file=fopen(name,"r");
  if (file == NULL) {
    etat_loop.etat=2; etat_loop.num_message=-11;
    return 0;
  }
  if (flag) fl_strcpy(prettycmd,fl_static("! "));
  else fl_strcpy(prettycmd,fl_static("| "));
  fl_strncat(prettycmd,cmd,MAX_CHAR_STRING-10);
  fl_strcat(prettycmd," : ");
  Aff_file(file,NULL,NULL,NULL); /* c'est tres moche */
  fclose(file);
  num_help_line=15;
  Aff_help_line(Screen_Rows-1);
  Aff_fin(prettycmd);
  Attend_touche(NULL);
  num_help_line=thread_view;
  Aff_help_line(Screen_Rows-1);
  etat_loop.etat=3;
  etat_loop.hors_struct|=1;
  return 0;
}

static int Sauve_header_article (Article_List *a_sauver, void *file_and_int);

/* do_pipe : on va essayer de voir... */
int do_pipe(int res) { 
  FILE *fichier;
  flrn_char *name=Arg_str;
  char filename[MAX_PATH_LEN];
  int ret, use_argstr=0, col, num_known_header=0;
  Numeros_List *courant=&Arg_do_funcs;
  int fd;

  int Sauve_article_do_pipe(Article_List *a_sauver, void *vfichier) {
      return Sauve_article(a_sauver, vfichier, 2);
  }

  if ((res!=FLCMD_SHELLIN) && (res!=FLCMD_SHELL) && (Article_courant==&Article_bidon)) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  while ((*name) && (fl_isblank(*name))) name++;
  if (res==FLCMD_PIPE_HEADER) {
    if (name[0]==fl_static('\0')) {
	etat_loop.etat=2; etat_loop.num_message=-17; return 0;
    }
    for (num_known_header=0; num_known_header<NB_KNOWN_HEADERS;
	 num_known_header++) {
	if (fl_strncasecmp(name,
		    fl_static_tran(Headers[num_known_header].header),
			Headers[num_known_header].header_len)==0)
	    break;
    }
    if (num_known_header==NB_KNOWN_HEADERS) {
	etat_loop.etat=2; etat_loop.num_message=-18; return 0;
    }
    name+=Headers[num_known_header].header_len;
    while ((*name) && (fl_isblank(*name))) name++;
  }

  if (name[0]==fl_static('\0')) {
    char *affname;
    const char *special;
    use_argstr=1;
    name=safe_malloc(MAX_PATH_LEN*sizeof(flrn_char));
    affname=safe_malloc(MAX_PATH_LEN);
    name[0]=fl_static('\0');
    affname[0]='\0';
    special=_("Piper dans : ");
    col=Aff_fin_utf8(special);
    if ((ret=flrn_getline(name, MAX_PATH_LEN, affname, MAX_PATH_LEN,
		    Screen_Rows2-1,col)<0)) {
      free(name);
      free(affname);
      etat_loop.etat=3;
      return 0;
    }
    free(affname);
  }
  fd=Pipe_Msg_Start((res!=FLCMD_SHELLIN) && (res!=FLCMD_SHELL), 
      (res == FLCMD_FILTER ) || (res == FLCMD_GFILTER)
      || (res == FLCMD_SHELLIN), name, filename);
  if (fd<0) {
    if (use_argstr) free(name);
    etat_loop.etat=2; etat_loop.num_message=-11;
    return 0;
  }
  fichier=fdopen(fd,"w");
  if (fichier==NULL) {
    if (use_argstr) free(name);
    etat_loop.etat=2; etat_loop.num_message=-11;
    return 0;
  }
  if ((res==FLCMD_PIPE) || (res==FLCMD_FILTER))
    distribue_action(courant,Sauve_article_do_pipe,NULL ,fichier, 0);
  else if (res==FLCMD_PIPE_HEADER) {
    struct file_and_int beurk2;
    beurk2.file=fichier;
    beurk2.num=num_known_header;
    distribue_action(courant,Sauve_header_article,NULL ,&beurk2, 0);
  }
  else {
    Action_with_args beurk;
    beurk.action=Sauve_article_do_pipe;
    beurk.flag=fichier;
    beurk.flags_to_omit = 0;
    if ((res != FLCMD_SHELL) && (res != FLCMD_SHELLIN))
      distribue_action(courant,grand_distribue, NULL, &beurk, 0);
  }
  if (fd>0) fclose(fichier);
  Pipe_Msg_Stop(fd);
  if ((res == FLCMD_FILTER ) || (res == FLCMD_GFILTER)
      || (res == FLCMD_SHELLIN))
    display_filter_file(name, res == FLCMD_SHELLIN, filename);
#ifdef USE_MKSTEMP
  unlink(filename);
#endif
  etat_loop.etat=1; etat_loop.num_message=14;
  if (use_argstr) free(name);
  return 0;
}

/* do_post : lance post */
int do_post(int res) { 
  Article_List *origine;
  flrn_char *str=Arg_str;
  int ret;

  if ((res==FLCMD_ANSW) || (res==FLCMD_MAIL) || (res==FLCMD_SUPERSEDES)) {
     origine=Article_courant;
     if (Arg_do_funcs.numlst_flags & (NUMLST_RNG|NUMLST_NUM))
       ret=Recherche_article(Arg_do_funcs.elem1.number,&origine,0);
     else if (Arg_do_funcs.numlst_flags & NUMLST_ALL) {
       origine=Arg_do_funcs.elem1.article;
       ret=(origine ? 0 : -2);
     }
     else ret=(origine ? 0 : -2);
     if (ret<0) {
       etat_loop.etat=1; etat_loop.num_message=3; return 0;
     } 
  } else origine=NULL;
  /* Provisoirement, je ne m'occupe pas de la chaine de caractère */
  ret=post_message(origine, str, (res==FLCMD_MAIL ? 1 : 
      					(res==FLCMD_SUPERSEDES ? -1 : 0))); 
  if (ret==3) { etat_loop.etat=1; etat_loop.num_message=21; } else
  if (ret==2) { etat_loop.etat=1; etat_loop.num_message=20; } else
  if (ret==1) { etat_loop.etat=1; etat_loop.num_message=6; } else
  if (ret==0) { etat_loop.etat=1; etat_loop.num_message=7; } else
    etat_loop.etat=3;
  return 0; 
}


/* do_cancel : cancel (SI !) */
static int cancel_article(Article_List *article, void *toto) { 
   static int confirm;
   int ret;
   if (article==NULL) {
     confirm=0;
     return 0;
   }
   ret=cancel_message(article,confirm); 
   if (ret==2) confirm=1;
   return ret;
}
int do_cancel(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  int ret;

  if (Article_courant==&Article_bidon) {
      etat_loop.etat=1; etat_loop.num_message=3; 
      etat_loop.hors_struct=7; return 0;
  }
  cancel_article(NULL,NULL);
  ret=distribue_action(courant,cancel_article,NULL,NULL, 0);
  if (ret>=0) {
     etat_loop.etat=1; etat_loop.num_message=(ret==0 ? 16 : 17);
  } else {
    etat_loop.etat=2; etat_loop.num_message=-14;
  }
  return 0;
}


/* do_opt */
int do_opt(int res) { 
  Get_option_line(Arg_str);
  etat_loop.etat=3;
  return 0; 
}

int do_opt_menu(int res) {
  int sav_color_opt=Options.color;
  menu_config_variables();
  if (Options.color!=sav_color_opt) {
       Init_couleurs();
       Screen_touch_lines (0, Screen_Rows2-1);
  }
  Screen_Rows=Screen_Rows2-Options.help_line;
  etat_loop.etat=3;
  etat_loop.hors_struct|=1;
  return 0;
}

int do_neth(int res) {  
/* TODO : remettre un nom correct à cette fonction */
  thread_view=1-thread_view;
  if (etat_loop.hors_struct & 8) {
     etat_loop.etat=1;
     etat_loop.num_message=(thread_view ? 24 : 25);
  } else etat_loop.etat=0;
  return 0;
}


/* Affiche la liste des newsgroup */
/* res=FLCMD_GLIS -> liste tout */
int do_list(int res) {
   flrn_char *gpe=Arg_str;
   Newsgroup_List *retour=NULL;
   int ret;

   ret=Liste_groupe((res==FLCMD_GLIS)?2:0, gpe, &retour);
   if (ret<0) { etat_loop.etat=2; etat_loop.num_message=-10; }
   else if (retour) {
      etat_loop.Newsgroup_nouveau=retour;
      etat_loop.num_futur_article=0;
      return 1;
   }
   else etat_loop.etat=3;
   etat_loop.hors_struct |= 1;
   return 0;
}

static int Sauve_header_article(Article_List *a_sauver, void *file_and_int) {
  struct file_and_int *truc = (struct file_and_int *) file_and_int;
  
  if (a_sauver==NULL) return 0;
  if ((a_sauver->headers==NULL) &&
      (cree_header(a_sauver,0,0,0)==NULL)) return 0;
  if (truc->num<NB_DECODED_HEADERS) {
     char *bla;
     int rc;
     if (a_sauver->headers->k_headers[truc->num]==NULL) return 0;
     rc=conversion_to_file(a_sauver->headers->k_headers[truc->num],
	     &bla,0,(size_t)(-1));
     fprintf(truc->file,"%s\n",bla);
     if (rc==0) free (bla);
     return 0;
  }
  if (a_sauver->headers->k_raw_headers[truc->num-NB_DECODED_HEADERS])
    fprintf(truc->file,"%s\n",
	    a_sauver->headers->k_raw_headers[truc->num-NB_DECODED_HEADERS]);
  return 0;
}

/* Sauve l'article a_sauver dans fichier */
/* flag : 0 : sauve   1 : pager    2 : pipe */
static int Sauve_article(Article_List *a_sauver, void *vfichier, int flag) {
  time_t date;
  FILE *fichier = (FILE *) vfichier;
  flrn_char *buf, *buf2, saved_char;
  int rc;
  char *trad;

  if (Options.use_mailbox) {
    if ((a_sauver->headers) && (a_sauver->headers->k_headers[FROM_HEADER])
          && (a_sauver->headers->date_gmt)) {
      buf=fl_strchr(a_sauver->headers->k_headers[FROM_HEADER],fl_static('<'));
      if (buf) {
        buf++;
	buf2=fl_strchr(buf,fl_static('>'));
        if (buf2) {
	   saved_char=*buf2;
	   *buf2=fl_static('\0');
	} else saved_char=fl_static('\0');
      } else {
         buf=a_sauver->headers->k_headers[FROM_HEADER];
	 buf2=fl_strchr(buf,fl_static('@'));
	 if (buf2) *buf2=fl_static('\0');
	 buf=fl_strrchr(buf,fl_static(' ')); 
	 if (buf==NULL) buf=a_sauver->headers->k_headers[FROM_HEADER];
	 saved_char=fl_static('\0');
	 if (buf2) { 
	   *buf2=fl_static('@');
	   buf2=strchr(buf2,fl_static(' '));
	   if (buf2) {
	      saved_char=*buf2;
	      *buf2=fl_static('\0');
	   }
	 }
      }
      rc=conversion_to_file(buf,&trad,0,(size_t)(-1));
      fprintf(fichier, "From %s %s", trad,
	      ctime(&(a_sauver->headers->date_gmt)));
      if (rc==0) free(trad);
      if (saved_char) *buf2=saved_char;
    } else {
      date=time(NULL);
      fprintf(fichier, "From %s%s %s", flrn_user->pw_name,"@localhost",
        ctime(&date)); 
    }
    rc=conversion_to_file(Newsgroup_courant->name,&trad,0,(size_t)(-1));
    fprintf(fichier, "Article: %d of %s\n", a_sauver->numero, trad);
    if (rc==0) free(trad);
  } else {
      rc=conversion_to_file(Newsgroup_courant->name,&trad,0,(size_t)(-1));
      fprintf(fichier, "Newsgroup : %s     No : %d\n", trad, a_sauver->numero);
      if (rc==0) free(trad);
  }
  Copy_article(fichier,  a_sauver, 1, NULL, 1,
	  (flag!=0));
  fprintf(fichier,"\n");
  return 0;
}


/* Change de newsgroup pour récuperer le même article */
/* On va essayer de faire quelque chose d'assez complet */
typedef struct {
   Newsgroup_List *gpe;
   long numero;
} Flrn_Reference_swap;

static struct format_elem_menu fmt_swp_menu_e [] =
    { { 0, 0, -1, 2, 0, 7 } };
struct format_menu fmt_swp_menu = { 1, &(fmt_swp_menu_e[0]) };
static int art_swap_grp(flrn_char *xref, 
	flrn_char *arg, Newsgroup_List *exclu) {
  int nostr=0, order=-1, best_order=-1;
  long numero;
  regex_t reg;
  flrn_char *newsgroup, *num, *tmp_name, *dummy;
  Newsgroup_List *mygroup=NULL;
  int avec_un_menu=Options.use_menus;
  Liste_Menu *lemenu=NULL, *courant=NULL;
  Flrn_Reference_swap *objet;

  if ((arg==NULL) || (*arg==fl_static('\0'))) nostr=1;
  if ((!nostr) && (Options.use_regexp)) {
    if(fl_regcomp(&reg,arg,REG_EXTENDED)) {etat_loop.etat=2;
    	etat_loop.num_message=-10; return 0;} }
  /* Bon, faut que je parse le Xref... ça a déjà été fait dans article_read */
  newsgroup=fl_strtok_r(xref,fl_static(" "),&dummy);
  while ((newsgroup=fl_strtok_r(NULL,fl_static(" :"),&dummy))) {
    num=fl_strtok_r(NULL, fl_static(": "),&dummy);
    if (!num) { etat_loop.etat=1; etat_loop.num_message=3; 
    		if ((!nostr) && (Options.use_regexp))
		  regfree(&reg);
		return 0;}
    numero=fl_strtol(num,NULL,10);
    if ((exclu) && (!avec_un_menu) && (fl_strcmp(exclu->name,newsgroup)==0))
      continue;
    if (!avec_un_menu) tmp_name=truncate_group(newsgroup,0); else
       tmp_name=newsgroup;
    if ((nostr) || (((!Options.use_regexp) && 
                       ((order=calcul_order(tmp_name, Arg_str))!=-1))
      || ((Options.use_regexp) && 
             ((order=calcul_order_re(tmp_name,&reg))!=-1))))
    {   
      mygroup=Newsgroup_deb;
      while (mygroup && (strcmp(mygroup->name, newsgroup))) 
	mygroup=mygroup->next;
      if (mygroup==NULL) {
         mygroup=cherche_newsgroup(newsgroup,1,0);
	 if (mygroup==NULL) continue;
      }
      if (avec_un_menu) {
         objet=safe_malloc(sizeof(Flrn_Reference_swap));
	 objet->gpe=mygroup;
	 objet->numero=numero;
         *(num-1)=fl_static(':');
         courant=ajoute_menu_ordre(&lemenu,&fmt_swp_menu,
		 &newsgroup,objet,-1+nostr,order);
	 *(num-1)=fl_static('\0');
      } else {
         if ((nostr) || (best_order==-1) || (best_order>order)) {
           etat_loop.Newsgroup_nouveau=mygroup;
           etat_loop.num_futur_article=numero;
	   best_order=order;
	 }
	 if (nostr) return 1;
      }
    } 
  }
  if ((!nostr) && (Options.use_regexp)) regfree(&reg);
  if ((!avec_un_menu) && (best_order!=-1)) return 1;
  if ((avec_un_menu) && (lemenu)) {
     if (lemenu->suiv==NULL) {
	 objet=(Flrn_Reference_swap *)(lemenu->lobjet);
         mygroup=objet->gpe;
	 numero=objet->numero;
	 free(lemenu->lobjet);
	 if (exclu==mygroup) {
	    etat_loop.etat=1; etat_loop.num_message=12;
	    return 0;
	 }
	 if (mygroup==Newsgroup_courant) {
	    Article_List *nouveau=Article_courant;
	    Recherche_article(numero,&nouveau,0);
	    if (nouveau) Article_courant=nouveau; /* sur ! */
    	    etat_loop.etat=0;
	    return 0;
	 } else {
    	    etat_loop.Newsgroup_nouveau=mygroup;
	    etat_loop.num_futur_article=numero;
	    return 1;
	 }
     } else {
	 int rc;
	 const char *special;
	 flrn_char *trad;
         num_help_line=8;
	 special=_("Quel groupe ?");
	 rc=conversion_from_utf8(special, &trad, 0 ,(size_t)(-1));
         objet=(Flrn_Reference_swap *)Menu_simple
	     (lemenu,NULL,NULL,NULL, trad);
	 if (rc==0) free(trad);
	 if ((objet==NULL) || (objet->gpe==Newsgroup_courant)) {
	     if ((objet) && (Article_courant) && 
	         (objet->numero!=Article_courant->numero)) {
		Article_List *nouveau=Article_courant;
		Recherche_article(objet->numero,&nouveau,0);
		if (nouveau) {
		   Article_courant=nouveau; 
		   etat_loop.etat=0;
		} else {
		   etat_loop.etat=2;
		   etat_loop.num_message=-12;
		   etat_loop.hors_struct|=1;
		}
	     } else etat_loop.etat=(objet ? 0 : 3);
	     for (courant=lemenu;courant;courant=courant->suiv) 
	        free(courant->lobjet);
             Libere_menu(lemenu);
	     return 0;
	 } else {
	     etat_loop.Newsgroup_nouveau=objet->gpe;
	     etat_loop.num_futur_article=objet->numero;
	     for (courant=lemenu;courant;courant=courant->suiv) 
	        free(courant->lobjet);
             Libere_menu(lemenu);
	     return 1;
	 }
     }
  } else {
     etat_loop.etat=1; etat_loop.num_message=12;
     return 0;
  }
}

int do_swap_grp(int res) {
  int ret;
  Article_List *article_considere;
  flrn_char *gpe=Arg_str, *buf;

  if (Article_courant==&Article_bidon) {
     etat_loop.etat=1; etat_loop.num_message=3;
     etat_loop.hors_struct=7; return 0;
  }
  while ((*gpe) && (isblank(*gpe))) gpe++;
  article_considere=Article_courant;
  if (Arg_do_funcs.numlst_flags & (NUMLST_RNG|NUMLST_NUM)) 
    Recherche_article(Arg_do_funcs.elem1.number, &article_considere, 0); else
  if (Arg_do_funcs.numlst_flags & NUMLST_ALL)
      article_considere=Arg_do_funcs.elem1.article;
  if (article_considere==NULL) {
     etat_loop.etat=1; etat_loop.num_message=3; return 0;
  }

  if ((!article_considere->headers) ||
      (article_considere->headers->k_headers_checked[XREF_HEADER] == 0)) {
    cree_header(Article_courant,0,1,0);
    if (!article_considere->headers) {
      etat_loop.etat=1; etat_loop.num_message=3; return 0;
    }
  }
  if (!article_considere->headers->k_headers[XREF_HEADER]) {
    etat_loop.etat=1; etat_loop.num_message=13; return 0;
  }
  buf=safe_flstrdup(article_considere->headers->k_headers[XREF_HEADER]);
  ret=art_swap_grp(buf,gpe,(article_considere==Article_courant ?
			  	Newsgroup_courant : NULL));
  free(buf);
  return ret;
}

int do_art_msgid(int res) {
   Article_List article_temp;
   flrn_char *flgpe=Arg_str;
   int ret,rc;
   char *gpe;

   rc=conversion_to_utf8(flgpe,&gpe,0,(size_t)(-1));
   while ((*gpe) && (isblank(*gpe))) gpe++;
   article_temp.headers=NULL;
   if (*gpe) { /* TODO : peut-être, si *gpe est nul, demander un msgid */
     article_temp.numero=-1;
     article_temp.msgid=gpe;
     cree_header(&article_temp,0,0,1);
     if ((article_temp.headers==NULL) && (strncasecmp(gpe,"<news:",6)==0)) { 
        if (strncasecmp(gpe,"<news:",6)==0) {
           gpe+=5;
	   *gpe='<';
        }
        article_temp.msgid=gpe;
        cree_header(&article_temp,0,0,1);
     }
   }
   if (article_temp.headers==NULL) {
      if (rc==0) free(gpe);
      etat_loop.etat=2; etat_loop.num_message=-21;
      return 0;
   }
   if (article_temp.headers->k_headers[XREF_HEADER]==NULL) {
      /* cas de leafnode : on peut toujours faire une recherche dans les
       * groupes */
      Article_List *larticle;
      Newsgroup_List *legroup;
      recherche_article_par_msgid(&larticle,&legroup,article_temp.msgid);
      free_one_article(&article_temp,0);
      if (larticle) {
	 if (legroup==Newsgroup_courant) {
	    Article_courant=larticle;
    	    etat_loop.etat=0;
            if (rc==0) free(gpe);
	    return 0;
	 } else {
	   etat_loop.Newsgroup_nouveau=legroup;
	   etat_loop.num_futur_article=-1;
	   etat_loop.Article_nouveau=larticle;
           if (rc==0) free(gpe);
	   return 1;
	 }
      }
      etat_loop.etat=1; etat_loop.num_message=13; return 0;
   }
   ret=art_swap_grp(article_temp.headers->k_headers[XREF_HEADER],NULL,NULL);
   free_one_article(&article_temp,0);
   if (rc==0) free(gpe);
   return ret;
}

int do_keybindings(int res) {
   int i, ret, key=-1, rc;
   int context=-1;
   flrn_char *buf=Arg_str, *buf2;
   Liste_Menu *menu=NULL, *courant=NULL;
   struct format_menu *fmtm=NULL;
   flrn_assoc_key_cmd *pc;
   flrn_char **ligne;
   const char *special;
   flrn_char *trad;

   if (*buf) {
      while (fl_isblank(*buf)) buf++;
      buf2=fl_strchr(buf,fl_static(' '));
      if (buf2) *buf2=fl_static('\0');
      for (context=0; context<NUMBER_OF_CONTEXTS; context++) 
         if (fl_strcasecmp(buf,Noms_contextes[context])==0) break;
      if (context==NUMBER_OF_CONTEXTS) context=-1;
   }
   for (i=0; i<MAX_ASSOC_TAB; i++) {
       pc=Flcmd_rev[i];
       while (pc) {
          ret=aff_ligne_binding(&(pc->key),context,&ligne,&fmtm);
          if (ret<0) { pc=pc->next; continue; }
          courant=ajoute_menu(courant,fmtm,ligne,(void *)(long)i);
	  if (!menu) menu=courant;
	  pc=pc->next;
       }
   }
   num_help_line=9;
   if (context==-1) {
     special=_("Bindings de touche...");
     rc=conversion_from_utf8(special, &trad, 0, (size_t)(-1));
   } else {
     trad=Noms_contextes[context];
     rc=1;
   }
   Menu_simple(menu,menu,NULL,NULL,
	   trad);
   if (rc==0) free(trad);
   Libere_menu(menu);
   etat_loop.hors_struct|=1;
   etat_loop.etat=3+(key>=0);
   return 0;
}

int do_on_selected (int res) {
  flrn_char *name=Arg_str,*buf;
  int ret, res2;
  Numeros_List *courant=&Arg_do_funcs;
  struct key_entry key;

  key.entry=ENTRY_ERROR_KEY;
  if (Article_courant==&Article_bidon) {
     etat_loop.etat=1; etat_loop.num_message=3;
     etat_loop.hors_struct=7; return 0;
  }

  while ((*name) && (fl_isblank(*name))) name++;
  if (name[0]==fl_static('\0')) {
    const char *special;
    special=_("Commande : ");
    Aff_fin_utf8(special);
    Attend_touche(&key);
    if (KeyBoard_Quit) {
       etat_loop.etat=3;
       return 0;
    }
    ret=get_command(&key,CONTEXT_COMMAND,-1,&une_commande);
  } else { 
    size_t b=next_flch(name,0);
    if ((int)b<=0) b=0;
    une_commande.cmd_ret_flags &= ~CMD_RET_MAYBE_AFTER;
    if ((name[b]==fl_static('\0')) || (fl_isblank(name[b]))) {
	buf=name;
	une_commande.before=NULL;
	une_commande.len_desc=0;
	une_commande.description=NULL;
	buf+=parse_key_string(buf,&key);
	ret=Lit_cmd_key(&key,CONTEXT_COMMAND,-1,&une_commande);
	if (*buf) {
	    while (fl_isblank(*buf)) buf++;
	    if (*buf) une_commande.after=safe_flstrdup(buf);
	}
	else une_commande.after=NULL;
    } else {
	une_commande.len_desc=0;
	ret=Lit_cmd_explicite(name,CONTEXT_COMMAND,-1,&une_commande);
	buf=name;
	while ((*buf) && (!fl_isblank(*buf))) buf++;
	if (buf[0]) une_commande.after=safe_flstrdup(buf);
	else une_commande.after=NULL;
	une_commande.before=NULL;
    }
  }
  if (ret<0) {
     if (une_commande.before) free(une_commande.before);
     if (une_commande.after) free(une_commande.after);
     etat_loop.etat=2; etat_loop.num_message=-9;
     return 0;
  }
  Arg_str[0]=fl_static('\0');
  res2=une_commande.cmd[CONTEXT_COMMAND];
  if (une_commande.before) Parse_nums_article(une_commande.before,NULL,0);
  if (une_commande.after) {
     res2=parse_arg_string(une_commande.after,res2,1);
     free(une_commande.after);
  }
  if ((res2 & FLCMD_MACRO)==0) {
     if ((une_commande.cmd_ret_flags & CMD_RET_MAYBE_AFTER) && 
	 (Flcmds[res2].cmd_flags & (Options.forum_mode ? 
			CMD_STR_FORUM : CMD_STR_NOFORUM))) {
       ret=get_str_arg(res2,&une_commande,0);
       if (ret==-1) {
          if (une_commande.before) free(une_commande.before);
	  etat_loop.etat=3;
	  return 0;
       }
     }
  } else {
     if (une_commande.before) free(une_commande.before);
     etat_loop.etat=2;
     etat_loop.num_message=-20;
     return 0;
  }
  if (une_commande.before) free(une_commande.before);
  /* On ne teste pas CMD_NEED_GROUP, ça a déjà été testé :-) */
  do {
    courant->numlst_flags|=NUMLST_SEL;
    courant=courant->next;
  } while ((courant) && (courant->numlst_flags));
  return (*Flcmds[res2].appel)(res2);
}

void Get_option_line(flrn_char *argument)
{
  int res=0, use_arg=1, ret=0;
  flrn_char *buf=argument;
  int color=0, col;
  while (fl_isblank(*buf)) buf++;
  if (*buf==fl_static('\0')) {
    char *affbuf,*ptr;
    struct key_entry key, *k;
    const char *special;

    use_arg=0;
    key.entry=ENTRY_ERROR_KEY;
    k=NULL;
    buf = safe_malloc(MAX_READ_SIZE*sizeof(flrn_char));
    affbuf = safe_malloc(MAX_READ_SIZE);
    ptr=affbuf;
    *buf=fl_static('\0'); *affbuf='\0';
    special=_("Option : ");
    col=Aff_fin_utf8(special);
    do {
      if (res>0) Aff_ligne_comp_cmd(buf,strlen(buf),col);
      conversion_to_terminal(buf,&ptr,MAX_READ_SIZE,(size_t)(-1));
      if ((res=magic_flrn_getline(buf,MAX_READ_SIZE,
		      affbuf, MAX_READ_SIZE, Screen_Rows2-1,col,
	  "\011",0,k,NULL))<0) {
        free(buf);
	free(affbuf);
        return;
      }
      if (res>0)
        ret=Comp_general_command(buf,MAX_READ_SIZE,col,options_comp,Aff_ligne_comp_cmd,&key);
      if (ret<0) ret=0;
      if (ret>0) k=&key; else k=NULL;
    } while (res!=0);
    free(affbuf);
    Free_key_entry(&key);
  }
  /* hack pour reconstruir les couleurs au besoin */
  color=(fl_strstr(buf,"color"))?1:0;
  color=(fl_strstr(buf,"mono"))?1:color;
  parse_options_line(buf,1,NULL,NULL);
  if (color) {
       Init_couleurs();
       Screen_touch_lines (0, Screen_Rows2-1);
  }
  Screen_Rows=Screen_Rows2-Options.help_line;
  if (!use_arg) free(buf);
  return;
}

/* change de groupe */
/* les flags correspondent à abonné / tout (0/1) et */
/* avec prefixe_groupe/sans (2/0) */
/* Retourne 0 en cas de succes, -1 si aucun changement */
/* et -2 si on demande un groupe inexistant */
/* On retourne exceptionnelement 1 dans le cas ou la chaine demandee */
/* est vide. */
int change_group(Newsgroup_List **newgroup, int flags, flrn_char *gpe_tab)
{
   int ret;

   ret=get_group(newgroup, flags | (Options.use_menus ? 8 : 0) |
   				   (Options.use_regexp ? 4 : 0),
			gpe_tab);
   if (ret==-1) *newgroup=Newsgroup_courant;
   if (ret==-3) ret=1;
   if (ret==-4) ret=-2;
   return ret;
}


/* Prend le prochain article non lu. Renvoie : */
/* 0 : Ok, ou force_reste=1... */
/* -1 : pas de nouveaux articles */
/* si pas_courant=1, on ne renvoie pas l'article courant */
/* On modifie prochain_non_lu pour qu'il fasse appel a chercher_mewnews */
/* quand il n'y a rien de nouveau...					*/
/* Et eventuellement l'appel a ajoute_message_par_num si posts récents  */
/* Renvoie donc en plus : */
/* -2 : reconstruire le groupe... */
static int raw_prochain_non_lu(int force_reste, Article_List **debut, int just_entered,int important) {
   Article_List *myarticle=*debut;
   int flag_mask, flag_res;
   int res;

   flag_mask=(important ? (FLAG_IMPORTANT | FLAG_READ) : FLAG_READ);
   flag_res=(important ? FLAG_IMPORTANT : 0);
   /* on regarde si l'article courant est lu */
   if (myarticle && ((myarticle->art_flags & flag_mask)==flag_res))
     return 0; 

   /* On essaie d'abord de chercher dans la thread */
   if( Options.threaded_next) {
     myarticle=next_in_thread(myarticle,flag_mask,NULL,0,0,flag_res,1);
     if (myarticle) { *debut=myarticle; return 0; }
     myarticle=*debut;
   }
   /* On teste d'abord ce qu'on trouve après Article_courant */
   while (myarticle && ((myarticle->art_flags & flag_mask)!=flag_res))
      myarticle=myarticle->next;
   if (myarticle==NULL) { 
     /* Puis on repart de Article_deb */
     myarticle=Article_deb;
     while (myarticle && (myarticle!=*debut) && ((myarticle->art_flags &
		     flag_mask)!=flag_res))
        myarticle=myarticle->next;
     if (myarticle==NULL) myarticle=Article_deb;
   }
   if (myarticle && ((myarticle->art_flags & flag_mask)==flag_res)) {
     if (Options.threaded_next) {
       myarticle=root_of_thread(myarticle,1);
       if ((myarticle->art_flags & flag_mask)!=flag_res)
         myarticle=next_in_thread(myarticle,flag_mask,NULL,0,0,flag_res,0);
        /* En théorie, on est SUR de trouver quelque chose */
     }
     if (myarticle) { *debut=myarticle; return 0; } 
     if (debug) fprintf(stderr,"Euh.... un thread lu non lu ???\n");
     myarticle=Article_deb;
       /* test inutile... en théorie */
   }
   /* Si on a rien trouvé, un appel a cherche_newnews ne fait pas de mal */
   /* a moins qu'on vienne juste de rentrer dans le groupe... */
   if (!just_entered) {
     res=cherche_newnews();
     if (res==-2) return -2;
     if (res>=1) return raw_prochain_non_lu(force_reste, debut, 0, important);
   }

   /* On fixe Article_courant au dernier article dans tous les cas */
   while (myarticle->next) myarticle=myarticle->next;
   *debut=myarticle;
   if (force_reste) 
      return 0;

   return -1;
}

/* en fait juste un wrapper pour appliquer le kill-file */
int prochain_non_lu(int force_reste, Article_List **debut, int just_entered, int pas_courant) {
  int res=-1;
  int old_flag=0;
  Article_List *save=NULL;
  if (debut) save=*debut;
  if (pas_courant) {
     old_flag=save->art_flags;
     save->art_flags |= FLAG_READ;
     if (save->art_flags & FLAG_IMPORTANT) {
        save->art_flags &= ~FLAG_IMPORTANT;
	Newsgroup_courant->important--;
     }
  }
  if (Newsgroup_courant->important>0) 
    res=raw_prochain_non_lu(0,debut,1,1);
  if (res<0) {
     /* je ne veux pas partir tout de suite */
    if (debut) (*debut)=save;
    res=raw_prochain_non_lu(force_reste,debut,just_entered,0);
  }
  if (pas_courant) {
     save->art_flags=old_flag;
     if (save->art_flags & FLAG_IMPORTANT)
	Newsgroup_courant->important++;
  }
  if (((*debut)->art_flags & FLAG_READ) == 0) {
    check_kill_article(*debut,1); /* le kill_file_avec création des headers */
    if (((*debut)->art_flags & FLAG_READ) != 0)
      return prochain_non_lu(force_reste,debut,just_entered,pas_courant);
  }
  return res;
}

/* Prend le prochain newsgroup interessant. Renvoie : */
/* 0 : Ok... */
/* -1 : rien de nouveau */
/* -2 : erreur */
int prochain_newsgroup(Newsgroup_List **newgroup ) {
   Newsgroup_List *mygroup=Newsgroup_courant, *last_mygroup;
   int res;

   /* On teste d'abord strictement APRES Newsgroup_courant */
   if (mygroup) 
     mygroup=mygroup->next;
   while (mygroup) {
      res=0;
      if (!(mygroup->grp_flags & GROUP_UNSUBSCRIBED )) {
          res=NoArt_non_lus(mygroup,1);
	  if (res>0) break;
      }
      last_mygroup=mygroup; 
            /* Plutot indispensable si on fait un zap_newsgroup */
      mygroup=mygroup->next;  
      if (res==-2) zap_newsgroup(last_mygroup);
      if (res==-1) return -2;
   }

   if (mygroup) { *newgroup=mygroup; return 0; }

   /* Puis on prend AVANT Newsgroup_courant, en NE testant PAS */
   /* celui-ci */
   mygroup=Newsgroup_deb;
   while (mygroup!=Newsgroup_courant) {
      res=0;
      if (!(mygroup->grp_flags & GROUP_UNSUBSCRIBED)) {
          res=NoArt_non_lus(mygroup,1);
          if (res>0) break;
      }
      last_mygroup=mygroup; 
      mygroup=mygroup->next;  
      if (res==-2) zap_newsgroup(last_mygroup);
      if (res==-1) return -2;
   }

   if (mygroup!=Newsgroup_courant) { *newgroup=mygroup; return 0; }

   return -1;
}

/* Applique action sur tous les peres de l'article donne*/
int parents_action(Article_List *article,int all, Action action, void *param, int flags, int flags_to_omit) {
  Article_List *racine=article;
  int res=0, res2=0;
  while(racine->pere && !(racine->pere->art_flags & FLAG_INTERNE2)) {
    racine->art_flags |= FLAG_INTERNE2;
    racine=racine->pere;
  }
  do {
    if ((!(flags & NUMLST_SEL)) || (racine->art_flags & FLAG_IS_SELECTED)) {
        if(racine->art_flags & flags_to_omit)
	    res2 = 0;
	else
            res2=action(racine,param);
    }
    racine->art_flags &= ~FLAG_INTERNE2;
    if (res<res2) res=res2;
  } while((racine=next_in_thread(racine,FLAG_INTERNE2,NULL,0,0,
      FLAG_INTERNE2,0)));
  return res;
}
  

/* Applique action(flag) sur tous les articles du thread
 * On utilise en interne FLAG_INTERNE1
 * Attention a ne pas interferer
 * all indique qu'il faut d'abord remonter a la racine */
/* on applique a tous les articles non extérieurs */
/* flags & 64 : on se limite aux articles SELECTED */
int thread_action(Article_List *article,int all, Action action, void *param, int flags, int flags_to_omit) {
  Article_List *racine=article;
  Article_List *racine2=article;
  int res=0;
  int res2=0;
  int level=0;
  /* On remonte a la racine si besoin */
  if (all) 
    racine = root_of_thread(racine,1);
  racine2=racine;
  /* On retire le FLAG_INTERNE1 a tout ceux qui l'ont dans le thread
   * C'est oblige car l'etat par defaut n'est pas defini
   * On le change dans la boucle pour que next_in_thread ne cycle pas */
  racine->art_flags &= ~FLAG_INTERNE1;
  while((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
      FLAG_INTERNE1,0)))
    racine->art_flags &= ~FLAG_INTERNE1;
  racine=racine2;
  /* Et la on peut faire ce qu'il faut */
  do { if (racine->numero!=-1) 
         if ((!(flags & NUMLST_SEL)) || (racine->art_flags &
		     FLAG_IS_SELECTED)) {
	  if(racine->art_flags & flags_to_omit)
	   res2=0;
	  else
           res2=action(racine,param);
	 }
    if (res<res2) res=res2;
    racine->art_flags |= FLAG_INTERNE1;
  } while ((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
            0,0)));
  return res; 
}
/* gthread_action : Applique action(flag) sur tous les articles du BIG thread
 * On utilise en interne FLAG_INTERNE1
 * Attention a ne pas interferer
 * all indique qu'il faut d'abord remonter a la racine */
/* on applique a tous les articles non extérieurs */
/* flags & 64 : on se limite aux articles SELECTED */
int gthread_action(Article_List *article,int all, Action action, void *param, int flags, int flags_to_omit) {
  Article_List *racine=article;
  Article_List *racine2=article;
  int res=0;
  int res2=0;
  int level=0;
  /* On remonte a la racine */
  racine = root_of_thread(racine,1);
  racine2=racine;
  /* On retire le FLAG_INTERNE1 a tout ceux qui l'ont dans le thread
   * C'est oblige car l'etat par defaut n'est pas defini
   * On le change dans la boucle pour que next_in_thread ne cycle pas */
  racine->art_flags &= ~FLAG_INTERNE1;
  while((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
      FLAG_INTERNE1,1)))
    racine->art_flags &= ~FLAG_INTERNE1;
  racine=racine2;
  /* Et la on peut faire ce qu'il faut */
  do { if (racine->numero!=-1)
         if ((!(flags & NUMLST_SEL)) || 
		 (racine->art_flags & FLAG_IS_SELECTED)) {
	  if(racine->art_flags & flags_to_omit)
	   res2 = 0;
	  else
           res2=action(racine,param);
	 }
    if (res<res2) res=res2;
    racine->art_flags |= FLAG_INTERNE1;
  } while ((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
            0,1)));
  return res; 
}


/* Appelle action sur tous les articles de Numero_List */
int distribue_action(Numeros_List *num, Action action, Action special,
    void *param, int flags_to_omit) {
  Article_List *parcours=NULL;
  int res,result=0,res2;
  if (num->numlst_flags==0) {
    if (!Article_courant) return -1;
    if (Article_courant->art_flags & flags_to_omit) return 0;
    if (special) return special(Article_courant,param);
    return action(Article_courant,param);
  } else if (num->numlst_flags==NUMLST_SEL) {
      if (!Article_courant) return -1;
      /* Selected + aucun paramètre : on prend tous les articles du groupe */
      num->numlst_flags=(NUMLST_SEL|NUMLST_RNG); num->elem1.number=1;
      num->num2=Newsgroup_courant->max;
  }
  while (num) {
    if (num->numlst_flags==0) {return 0;}
    if (num->numlst_flags & (NUMLST_RNG|NUMLST_NUM)) 
       res=Recherche_article(num->elem1.number,&parcours,1);
    else {
       parcours=num->elem1.article;
       res=(parcours ? 0 : -2);
    }
    res2=0;
    switch (num->numlst_flags & (~NUMLST_SEL)) { 
      case NUMLST_ART: case NUMLST_NUM : if (num->numlst_flags & NUMLST_SEL) {
                           if ((res==0) && (!(parcours->art_flags &
					   FLAG_IS_SELECTED)))
			        res=-1;
		       }
                  if (res==0) {
		   if(parcours->art_flags & flags_to_omit)
		    res2=0;
		   else
		    res2=action(parcours,param);
		  }
	      break;
      case NUMLST_ASC: if (res==0) res2=parents_action(parcours,0,action,param,
		      num->numlst_flags, flags_to_omit);
		break;
      case NUMLST_DES: if (res==0) res2=thread_action(parcours,0,action,param,
			       num->numlst_flags, flags_to_omit);
		break;
      case NUMLST_THR: if (res==0) res2=gthread_action(parcours,0,action,param,
			       num->numlst_flags, flags_to_omit);
		break;
      case NUMLST_RNG: if ((res==0) || (res==-1)) {
		while(parcours) {
		  if (parcours->numero > num->num2) break;
		  if ((num->numlst_flags==NUMLST_RNG) ||
			  (parcours->art_flags & FLAG_IS_SELECTED)) {
		       if(parcours->art_flags & flags_to_omit)
			   res2 = 0;
		       else
		           res2=action(parcours,param);
		  }
		  if (res2<result) result=res2;
		  parcours=parcours->next;
		}
	      } 
	      break;
      default: return -1;
    }
    if (res2<result) result=res2;
    num=num->next;
  }
  return result;
}

void save_etat_loop() {
  memcpy(&etat_save,&etat_loop,sizeof(etat_loop));
}
void restore_etat_loop() {
  memcpy(&etat_loop,&etat_save,sizeof(etat_loop));
}

#ifdef USE_SLANG_LANGUAGE

int Push_article(Article_List *a_sauver, void *groupe) {
   Newsgroup_List *gr = (Newsgroup_List *)groupe;
   return Push_article_on_stack (a_sauver, gr);
}

struct slang_fun_with_type {
   SLang_Name_Type *slang_fun;
   int type_fun;
};

int Exec_function_on_article(Article_List *a_sauver, void *la_fun) 
{
  char *name=Arg_str;
  struct slang_fun_with_type *fonction=(struct slang_fun_with_type *)la_fun;
  SLang_start_arg_list ();
  Push_article_on_stack(a_sauver, Newsgroup_courant);
  if (fonction->type_fun & 1) 
    SLang_push_string(name);
  SLang_end_arg_list ();
  if (SLexecute_function(fonction->slang_fun)==-1) {
     SLang_restart (1);
     SLang_Error = 0;
  }
  /* TODO : faire attention à l'état de la pile dans ce genre de cas */
  return 0;
}

/* effect = 1 : changer de groupe */
int side_effect_of_slang_command (int rt, Newsgroup_List *new,
	 int num_futur_article, int etat, int hors_struct) {
    static int ret;

    if (rt==-100) return ret;
    if (rt==-50) {
	ret=0;
	return 0;
    }
    ret=rt;
    etat_loop.Newsgroup_nouveau=new;
    etat_loop.num_futur_article=0;
    etat_loop.etat=etat;
    etat_loop.hors_struct=hors_struct;
    return ret;
}

int Execute_function_slang_command(int type_fun, SLang_Name_Type *slang_fun)
{
  flrn_char *name=Arg_str;
  Numeros_List *courant=&Arg_do_funcs;
  char *argname;
  int rc;

  rc=conversion_to_file(name,&argname,0,(size_t)(-1));
  if (slang_fun==NULL) {
      etat_loop.etat=2;
      etat_loop.num_message=-9;
      return 0;
  }
  side_effect_of_slang_command(-50,NULL,0,0,0);
  if ((type_fun & 4)==0) {
    SLang_start_arg_list ();
    if (type_fun & 2) 
      distribue_action(courant, Push_article, NULL, Newsgroup_courant, 0);
    if (type_fun & 1) 
       SLang_push_string(argname);
    SLang_end_arg_list ();

    if (SLexecute_function(slang_fun)==-1) {
       SLang_restart (1);
       SLang_Error = 0;
    }
  /* TODO : faire attention à l'état de la pile dans ce genre de cas */
  } else {
    struct slang_fun_with_type fonction;
    fonction.slang_fun=slang_fun;
    fonction.type_fun=type_fun;
    distribue_action(courant, Exec_function_on_article, NULL, &fonction, 0);
  }
  if (rc==0) free(argname);
  return (side_effect_of_slang_command(-100,NULL,0,0,0));
}
#endif
