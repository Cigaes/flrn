/* flrn v 0.1                                                           */
/*              flrn_inter.c          27/11/97                          */
/*                                                                      */
/*  La boucle principale de flrn.                                       */
/*                                                                      */
/*                                                                      */

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

/* On va définir ici des structures et des variables qui seront utilisées */
/* pour loop, et les fonctions qui y sont associés. Il faudrait en fait   */
/* que flrn_inter se limite à ses fonctions...				  */

/* Ces variables etaient auparavant locales à loop */
struct etat_var {
   int hors_struct; /* 1       : hors_limite 
		       2 et 3  : end_of_group ( => hors_limite )
		       4 et 5  : under_group  ( => hors_limite )
		       8 et 11 : hors_newsgroup ( => end_of_groupe ) */
   int etat, num_message; 
   /* Le système num_message est probablement insuffisant pour des messages */
   /* explicites...							    */
   Newsgroup_List *Newsgroup_nouveau; /* Pour les changement de newsgroup */
   int num_futur_article;
   int next_cmd;
   Article_List *Article_nouveau; /* utilisé si num_futur_article == -1 */
} etat_loop, etat_save;

/* Ces variables correspondent simplement aux arguments des fonctions  */
/* Ici pour un numéro ou un article */
union element {
  int num;
  Article_List *article; 
};
typedef struct Num_lists
{
   struct Num_lists *next;
   int flags; /* 0 : rien   1 : article       2 : num1
                 4 : _article  8 : article>   16 : big_thread(*) 
		 32 : num1-num2  64(flag) : selected */
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
    void * flag);
int thread_action(Article_List *article,int all, Action action, void *param, int flags);
int gthread_action(Article_List *article,int all, Action action, void *param, int flags);


Cmd_return une_commande;
#define MAX_CHAR_STRING 100
char Arg_str[MAX_CHAR_STRING];

/* le tableau touche -> commande */
int *Flcmd_rev_command = &Flcmd_rev[CONTEXT_COMMAND][0];
/* pour les macros */

/* Cette horrible structure sert a stocker une action et son argument en un */
/* seul pointeur...							    */
typedef struct Action_Beurk {
   Action action;
   void *flag;
} Action_with_args;

struct file_and_int {
   FILE *file;
   int num;
};

int parse_arg_string(char *str,int command, int annu16);
/* On prédéfinit ici les fonctions appelés par loop... A l'exception de */
/* get_command, elles DOIVENT être de la forme int do_* (int res)       */ 
int get_command_command(int get_com);
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
int do_pipe_header(int res);
int do_select(int res);
int do_keybindings(int);
int do_on_selected(int res);


/* Ces fonctions sont appelés par les do_* */
int change_group(Newsgroup_List **newgroup,int flags, char *gpe_tab);
int prochain_non_lu(int force_reste, Article_List **debut, int just_entered, int pas_courant);
int prochain_newsgroup();
void Get_option_line(char *argument);
/* les trois fonctions suivantes sont inutilisées...
 * mais on les laisse tant que la bonne version 'est pas écrite */
void zap_thread(Article_List *article,int all, int flag, int zap);
#if 0
int next_thread(int flags);
#endif

static int push_tag();

/* aff_opt_c : affiche simplement une liste de messages non lus vers stdout */
/* A la demande de Sbi, j'affiche aussi le nombre total de messages    	*/
/* non lus...								    */
int aff_opt_c(char *newsgroup) {
   int deb, res, nb_non_lus=0;
   int to_build; /* sans intérêt ici, mais on doit l'utiliser pour un appel */
   char ligne[80];
   
   Newsgroup_courant=Newsgroup_deb;
   while (Newsgroup_courant) {
      if ((Newsgroup_courant->flags & GROUP_UNSUBSCRIBED) ||
          (newsgroup && (strstr(truncate_group(Newsgroup_courant->name,0),newsgroup)==NULL))) {
         Newsgroup_courant=Newsgroup_courant->next;
	 continue;
      }
      res=NoArt_non_lus(Newsgroup_courant,0);
      if (res==-2) 
	 fprintf(stdout, "Mauvais newsgroup : %s\n", Newsgroup_courant->name);
      if (res>0) {
        if (newsgroup) {
          deb=Newsgroup_courant->read?Newsgroup_courant->read->max[0]:1;
          cree_liste(deb,&to_build);
	  if ((res=Newsgroup_courant->not_read)) {
	    fprintf(stdout, "%s : %d article%s non lu%s\n", 
	      truncate_group(Newsgroup_courant->name,0),res, 
	         (res==1 ? "" : "s"), (res==1 ? "" : "s"));
	    Article_courant=Article_deb;
	    while (Article_courant) {
	       if (!(Article_courant->flag & FLAG_READ)) {
	          fprintf(stdout,"%s\n",Prepare_summary_line(Article_courant,NULL, 0, ligne, 79, 0)+1);
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
          fprintf(stdout, "%s : %d article%s non lu%s\n", 
            truncate_group(Newsgroup_courant->name,0),res, 
	      (res==1 ? "" : "s"), (res==1 ? "" : "s"));
	}
        nb_non_lus+=res;
      }
      Newsgroup_courant=Newsgroup_courant->next;
   }
   if (newsgroup==NULL) {
      if (nb_non_lus==0) 
        fputs(Messages[MES_NOTHING_NEW],stdout); 
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
    case 1 : Aff_error(Messages[MES_NOTHING_NEW]); break;
    case 2 : Aff_error(Messages[MES_EOG]); break;
    case 3 : Aff_error(Messages[MES_NO_MES]); break;
    case 4 : 
    case 5 : Aff_error_fin(Messages[MES_OMIT],0,-1); break;
    case 6 : Aff_error(Messages[MES_POST_SEND]); break;
    case 7 : Aff_error(Messages[MES_POST_CANCELED]); break;
    case 8 : Aff_error_fin(Messages[MES_ART_SAVED],0,-1); break;
    case 9 : Aff_error_fin(Messages[MES_ABON],0,-1); break;
    case 10 : Aff_error(Messages[MES_NOSEL_THREAD]); break;
    case 11 : Aff_error_fin(Messages[MES_ZAP],0,-1);
	      break;
    case 12 : Aff_error(Messages[MES_NOTHER_GROUP]); break;
    case 13 : Aff_error_fin(Messages[MES_NO_XREF],1,-1); break;
    case 14 : Aff_error_fin(Messages[MES_CONTINUE],0,0); break;
    case 15 : Aff_error_fin(Messages[MES_TAG_SET],0,-1); break;
    case 16 : Aff_error_fin(Messages[MES_CANCEL_CANCELED],0,-1); break;
    case 17 : Aff_error(Messages[MES_CANCEL_DONE]); break;
    case 18 : 
    case 19 : Aff_error_fin(Messages[MES_OP_DONE],0,-1); break;
    case 20 : Aff_error(Messages[MES_MAIL_SENT]); break;
    case 21 : Aff_error(Messages[MES_MAIL_POST]); break;
    case 22 : Aff_error_fin(Messages[MES_TEMP_READ],0,-1); break;
    case 23 : Aff_error_fin(Messages[MES_FLAG_APPLIED],0,-1); break;
    case 24 : Aff_error_fin(Messages[MES_MODE_THREAD],0,-1); break;
    case 25 : Aff_error_fin(Messages[MES_MODE_NORMAL],0,-1); break;
 /* Message d'erreur */
    case -1 : Aff_error(Messages[MES_NO_GROUP]);  /* non utilisé */
	       break;
    case -2 : Aff_error(Messages[MES_GROUP_EMPTY]);
	       break;
    case -3 : Aff_error(Messages[MES_NOT_IN_GROUP]);
	       break;
/*	        case -4 : Aff_error("Post refusé.");
	       break;   */
    case -5 : Aff_error_fin(Messages[MES_NEGATIVE_NUMBER],1,-1); /* non utilisé */
	       break;
    case -6 : Aff_error(Messages[MES_SAVE_FAILED]);
	       break;
    case -7 : Aff_error(Messages[MES_UNK_GROUP]);
	       break;
    case -8 : Aff_error(Messages[MES_NO_FOUND_GROUP]);
	       break;
    case -9 : Aff_error_fin(Messages[MES_UNKNOWN_CMD],1,-1);
	       break;
    case -10 : Aff_error_fin(Messages[MES_REGEXP_BUG],1,-1); break;
    case -11 : Aff_error(Messages[MES_PIPE_BUG]); break;
    case -12 : Aff_error(Messages[MES_MES_NOTIN_GROUP]);
	       break;
    case -13 : Aff_error_fin(Messages[MES_BAD_TAG],1,-1); break;
    case -14 : Aff_error_fin(Messages[MES_CANCEL_REFUSED],1,-1); break;
    case -15 : Aff_error_fin(Messages[MES_EMPTY_HISTORY],1,-1); break;
/*    case -16 : Aff_error("Vous ne pouvez pas poster ici."); break; */
/* ce message est idiot : rien n'empêche de faire un followup, sauf à */
/* la rigueur si le serveur refuse tout...			      */
    case -17 : Aff_error_fin(Messages[MES_NO_HEADER],1,-1); break;
    case -18 : Aff_error_fin(Messages[MES_REFUSED_HEADER],1,-1); break;
    case -19 : Aff_error_fin(Messages[MES_INVAL_FLAG],1,-1); break;
    case -20 : Aff_error_fin(Messages[MES_MACRO_FORBID],1,-1); break;
    case -21 : Aff_error_fin(Messages[MES_NO_MSGID],1,-1); break;
    default : Aff_error(Messages[MES_FATAL]);
	       break;
  }

}

/* return 1 if 'q' was pressed */
int loop(char *opt) {
   int res=0, quit=0, ret;
   int to_build=0; /* il faut appeler cree_liste */
   int change;
   Numeros_List *fin_de_param=NULL;
   char Str_macro[MAX_CHAR_STRING];
   
   Str_macro[0]='\0';
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
      if (Article_courant) {
	if (etat_loop.hors_struct==1) etat_loop.hors_struct=0;
	if (!(etat_loop.hors_struct & 8)) 
	{
	  if (etat_loop.num_futur_article==0)
           change=-prochain_non_lu(etat_loop.num_message==1,&Article_courant,1,0);
	  else {
	    if (etat_loop.num_futur_article !=-1) {
	      Arg_do_funcs.elem1.num=etat_loop.num_futur_article;
	      Arg_do_funcs.num2=0;
	      Arg_do_funcs.flags=2;
	      if (Arg_do_funcs.next) Arg_do_funcs.next->flags=0;
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
	     Aff_fin("Patientez..."); /* Pour le voir un peu quand même */
	     Screen_refresh();
	     cree_liste_end();
	     to_build=0;
	   }
	   if (ret<0) ret=0; /* Aff_article_courant a renvoyé une erreur */
	   if (etat_loop.next_cmd>=0) {
	     res=etat_loop.next_cmd;
	     etat_loop.next_cmd=-1; /* remis à jour ensuite */
	     strncpy(Arg_str,Str_macro,99);
	     if (fin_de_param==NULL) Arg_do_funcs.flags=0; else
	     if (fin_de_param->next) fin_de_param->next->flags=0;
	   } else {
	     res=get_command_command(ret-1);
	     if ((res>0) && (res & FLCMD_MACRO)) {
	        strncpy(Str_macro,Arg_str,99);
	        fin_de_param=&Arg_do_funcs;
		if (fin_de_param->flags==0) fin_de_param=NULL;
		else
		  while ((fin_de_param->next) && (fin_de_param->next->flags))
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
	   else  {
	     if ((Flcmds[res].flags & CMD_NEED_GROUP) &&
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
	Aff_fin("Patientez..."); /* Pour le voir un peu quand même */
	Screen_refresh();
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
	     if (Newsgroup_courant->flags & GROUP_UNSUBSCRIBED) {
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
	   etat_loop.etat=2; etat_loop.num_message=-20;
	}
      }
      if (to_build==1) {
        if (Article_deb && (Article_deb!=&Article_bidon)) detruit_liste(0);
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
  for (i=0;i<MAX_FL_KEY;i++) Flcmd_rev_command[i]=FLCMD_UNDEF;
  for (i=0;i<NB_FLCMD;i++) 
     Flcmd_rev_command[Flcmds[i].key]=Flcmd_rev_command[Flcmds[i].key_nm]=i;
  for (i=0;i<CMD_DEF_PLUS;i++) 
    Bind_command_new(Cmd_Def_Plus[i].key,Cmd_Def_Plus[i].cmd,
	Cmd_Def_Plus[i].args,CONTEXT_COMMAND, Cmd_Def_Plus[i].add);
  Flcmd_rev_command[0]=FLCMD_UNDEF;
  return;
}

int fonction_to_number(char *nom) {
  int i;
  for (i=0;i<NB_FLCMD;i++)
    if (strcmp(nom, Flcmds[i].nom)==0)
      return i;
  return -1;
}

int call_func(int number, char *arg) {
  int res;
  *Arg_str='\0';
  Arg_do_funcs.flags=0;
  res=number;
  res=parse_arg_string(arg,res,0);
  return (*Flcmds[res].appel)(res);
}

/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee */
/* -4 plus de macro disponible */
int Bind_command_explicite(char *nom, int key, char *arg, int add) {
  int i;
  if ((key<1)  || key >= MAX_FL_KEY)
    return -2;
  /* les commandes non rebindables... Je trouve ca moche */
  for (i=0;i<NB_FLCMD;i++) 
    if (Flcmds[i].key_nm==key)
      return -3;
  if (arg ==NULL) {
    if (strcmp(nom, "undef")==0) {
      Flcmd_rev_command[key]=FLCMD_UNDEF;
      return 0;
    }
  }
  for (i=0;i<NB_FLCMD;i++)
    if (strcmp(nom, Flcmds[i].nom)==0) {
      if (Bind_command_new(key,i,arg,CONTEXT_COMMAND, add)<0) return -4;
      return 0;
    }
  return -1;
}


/* Decodage d'un numero pour le parsing */
/* Returne -1 (ou NULL) en cas d'echec */
/* flag = 0 : numéro, flag = 1 : article, flag = 2 : article, mais peut changer */
static union element Decode_numero(char *str, union element defaut, int *flag) {
   char *ptr=str;
   int trouve_inf=0, nombre=0;
   union element res;
   Article_List *parcours=NULL;

   if (*ptr==',') ptr++;
   if (*ptr=='\0') return defaut;
   if (*ptr=='<') { ptr++; trouve_inf=1; if (*ptr==',') ptr++; }
   if (*ptr=='\0') { 
       res=defaut; 
       /* Dans le cas de trouve_inf, on prend parcours=Article_courant */
       if (*flag) parcours=res.article; else 
           parcours=Article_courant;
   } else 
       if (*ptr=='.') { if (*flag) res.article=Article_courant; else
                                  { res.num=0;  parcours=Article_courant; }
		        ptr++; } 
   else {
     nombre=strtol(ptr, &ptr, 10);
     if (nombre) Recherche_article (nombre, &parcours, 0); else
         parcours=Article_courant;
     if (*flag==0) res.num=nombre; else 
         res.article=parcours;
   }
   if (*ptr==',') ptr++;
   if ((!trouve_inf) && (*ptr=='<')) { ptr++; trouve_inf=1; 
     				       if (*ptr==',') ptr++; }
   if (*ptr!='\0') {
      if (*flag==0) res.num=-1; else res.article=NULL;
      return res; /* On doit etre au bout */
   }
   if (trouve_inf) {
      if (parcours==NULL) {
         if (*flag==0) res.num=-1; else res.article=NULL;
	 return res;
      }
      if (*flag) 
         res.article=root_of_thread(parcours,1); else {
	   parcours=root_of_thread(parcours,0);
	   if (parcours) res.num=parcours->numero; else res.num=-1;
	 }
  } else if ((*flag==2) && (nombre)) {
     /* On a juste trouve un nombre, on le garde */
     *flag=0;
     res.num=nombre;
  }
  return res;
}


/* Parsing des numeros d'articles
 * Arg_do_funcs.flag est a 0 s'il n'y a rien a parser...
 * On arrete des qu'il y a un problème.
 * On a deux règles spéciales : 0 est l'article courant,
 * 1 le premier article du groupe */
static void Parse_nums_article(char *str, char **sortie, int flags) {
   char *ptr=str, *ptr2=NULL, *ptrsign;
   int reussi=1, flag;
   char save_char='\0';
   union element defaut;
   Numeros_List *courant=&Arg_do_funcs;
   /* on conserve le debut de la liste. Pour les commandes explicites */
   while (courant->flags != 0) courant = courant->next;

   while (ptr && reussi) {
     ptr2=strchr(ptr,',');
     if (ptr2!=NULL) *ptr2='\0'; else
        if (flags & 16) {
	   courant->flags=0;
	   if (sortie) *sortie=ptr;
	   return;
	}
     if (*ptr=='\0') {
	courant->flags=0;
     } else {
       ptrsign=strchr(ptr,'-');
       if (ptrsign!=NULL) 
       { 
         flag=0;
         *ptrsign='\0';
         courant->flags=32;
	 defaut.num=1;
         courant->elem1=Decode_numero(ptr,defaut,&flag);
         if (courant->elem1.num==-1) reussi=0;
         *ptrsign='-';
	 defaut.num=(Newsgroup_courant ? Newsgroup_courant->max : 1);
         defaut=Decode_numero(ptrsign+1,defaut,&flag);
	 courant->num2=defaut.num;
         if (courant->num2==-1) reussi=0;
       } else { /* ce n'est pas '-' */
         flag=1;
         ptrsign=strchr(ptr,'_');
         if (ptrsign!=NULL) { save_char='_'; courant->flags=4; *ptrsign=','; }
           else {
  	     ptrsign=strchr(ptr,'>');
  	     if (ptrsign!=NULL) 
	       { save_char='>'; courant->flags=8; *ptrsign=','; } 
	     else {
	       ptrsign=strchr(ptr,'*');
	       if (ptrsign!=NULL)  
	         { save_char='*'; courant->flags=16; *ptrsign=','; }
	       else { courant->flags=1; flag=2; }
	     }
	   }
	 defaut.article=Article_courant;
         courant->elem1=Decode_numero(ptr, defaut,&flag);
         if (flag) { if (courant->elem1.article==NULL) reussi=0; }  else 
	 {
	    courant->flags=2;
	    if (courant->elem1.num==-1) reussi=0; 
	 }
         if (ptrsign!=NULL) *ptrsign=save_char;
       }
     }
     if ((flag==0) && (Article_courant && (courant->elem1.num==0))) 
        courant->elem1.num=Article_courant->numero;
     if ((flag==0) && (Article_deb && (courant->elem1.num==1))) 
        courant->elem1.num=Article_deb->numero;
     if (Article_courant && (courant->num2==0)) 
        courant->num2=Article_courant->numero;
     if (Article_deb && (courant->num2==1)) 
        courant->num2=Article_deb->numero;
     if (reussi) {
        if (ptr2) { *ptr2=','; ptr=ptr2+1; } else ptr=NULL;
        if (courant->next) courant=courant->next; else
	{ courant->next=safe_calloc(1,sizeof(Numeros_List));
	  courant=courant->next;
	}
	courant->flags=0;
     }
   }
   if (!reussi) {
     if (ptr2) *ptr2=',';
     courant->flags=0;
   }
   if (sortie) *sortie=ptr;
}


/* appelé par get_command_command et d'autres */
/* annu16=1 : annuler le flag 16 si macro */
int parse_arg_string(char *str,int command, int annu16)
{
   int flag;
   int cmd;
   if (str) while (*str==' ') str++;
   if ((str==NULL) || (str[0]=='\0')) return command;
   cmd=command;
   if (cmd<0) return cmd;
   if (cmd & FLCMD_MACRO) {
     cmd = Flcmd_macro[cmd ^ FLCMD_MACRO].cmd;
   }
   flag=Flcmds[cmd].flags & 19; /* c'est crade... */
   /* un truc quand même : le flag 16 n'a pas de valeur si macro et appele */
   /* depuis get_command_command */
   if ((annu16) && (command & FLCMD_MACRO)) flag=flag & 3;
   if (flag==0) return command;
   Parse_nums_article(str, &str, flag);
   if (str) strncpy(Arg_str, str, MAX_CHAR_STRING-1);
   return command;
}

/* Prend la chaine argument pour les appels qui en ont besoin en mode cbreak */
/* On ajoute key dans le cas ou isdigit(key) ou key=='<' */
/* Renvoie -1 si annulation */
static int get_str_arg(int res, char *beg) {
   int col, ret;
   char cmd_line[MAX_CHAR_STRING];
   char *str=cmd_line;

   *(str++)='(';
   if (beg) {
      strncpy(str,beg,MAX_CHAR_STRING-7-strlen(Flcmds[res].nom));
      str[MAX_CHAR_STRING-7-strlen(Flcmds[res].nom)]='\0';
      str+=strlen(str);
      *(str++)='\\';
   }
   strcpy(str,Flcmds[res].nom);
   strcat(str,") : ");
   col=Aff_fin(cmd_line);
   str=cmd_line;
   str[0]=0;
   if (Flcmds[res].comp) {
     int ret2;
     ret2=0;
     do {
        Aff_ligne_comp_cmd(str,strlen(str),col);
	if ((ret=magic_getline(str,MAX_CHAR_STRING,Screen_Rows-1,col,
			"\011",0,ret2))<0) return -1;
	ret2=0;
	if (ret>0) ret2=Comp_general_command(str, MAX_CHAR_STRING,col,
	   Flcmds[res].comp, Aff_ligne_comp_cmd);
	if (ret2<0) ret2=0;
     } while (ret!=0);
   } else {
     ret=getline(str, MAX_CHAR_STRING, Screen_Rows-1, col);
     if (ret<0) return -1;
   }
   if (Flcmds[res].flags & 2) 
     Parse_nums_article(str,&str,Flcmds[res].flags & 19);
   if (str) strcpy(Arg_str, str); else Arg_str[0]='\0';
   return 0;
}


/* Prend une commande pour loop... Renvoie le code de la commande frappe */
/* Renvoie -1 si commande non défini				         */
/*         -2 si rien							 */
/*         -3 si l'état est déjà défini...				 */
int get_command_command(int get_com) {
   int res, res2, key;

   Arg_do_funcs.flags=0;
   Arg_str[0]='\0';

   if (get_com==-1) {
     Aff_fin("A vous : ");
     key=Attend_touche();
     if (KeyBoard_Quit) return -1;
     res=get_command(key,CONTEXT_COMMAND,-1,&une_commande);
   } else res=get_com;
   if (res<0) {
      if (une_commande.before) free(une_commande.before);
      if (une_commande.after) free(une_commande.after);
      return res;
   }
   /* res = 0 */
   res2=une_commande.cmd[CONTEXT_COMMAND];
   if (une_commande.before) 
      Parse_nums_article(une_commande.before,NULL,0);
   if (une_commande.after) {
      res2=parse_arg_string(une_commande.after,res2,1);
      free(une_commande.after);
   }
   if ((res2!=FLCMD_UNDEF) && ((res2 & FLCMD_MACRO)==0)) {
     /* Testons si on a besoin d'un (ou plusieurs) parametres */
     if ((une_commande.maybe_after) && 
         ( ((!Options.forum_mode) && (Flcmds[res2].flags & 8))
         || ((Options.forum_mode) && (Flcmds[res2].flags & 4)) )) {
       res=get_str_arg(res2,une_commande.before);
       if (res==-1) res2=-2;
     }
   }
   if (une_commande.before) free(une_commande.before);
   return res2;
}

static int tag_article(Article_List *art, void * flag)
{art->flag |= *(int *)flag; return 0;}
static int untag_article(Article_List *art, void * flag)
{art->flag &= *(int *)flag; return 0;}

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
      etat_loop.etat=1; etat_loop.num_message=3; return 0;
   }
   parcours=Article_courant;
   /* dans le cas ou on est hors limite et num1=Article_courant->numero */
   /* on reaffiche Article_courant, a condition que ce ne soit ni - ni '\n' */
   /* ni un ' ' (sauf cas particuliers) */
   if (Article_courant && ((Arg_do_funcs.flags | 64)==64)) {
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
     if (Arg_do_funcs.flags & 34) {
        ret=Recherche_article(Arg_do_funcs.elem1.num, &parcours, 
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
			   } while(parcours && (parcours->flag & FLAG_KILLED));
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
			 while(parcours2 && (parcours2->flag & FLAG_KILLED)) {
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
				     (parcours2->flag & FLAG_KILLED))
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
      if ((parcours2) || (Options.inexistant_arrow)) parcours=parcours2;
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

static int my_goto_tag (int tag) {
  int ret;

  if (tag >= MAX_TAGS) return -1;
  if (tag <0 ) return -1;
  if (tags[tag].article_deb_key) { /* le tag existe */
    if (tags[tag].article_deb_key==-1) {
       if (Newsgroup_courant && (strcmp(Newsgroup_courant->name,tags[tag].newsgroup_name)==0)) 
         tags[tag].article_deb_key=Newsgroup_courant->article_deb_key;
    }
    if (Newsgroup_courant && (tags[tag].article_deb_key == Newsgroup_courant->article_deb_key)) {
      if (tags[tag].article==NULL) {
        ret=Recherche_article(tags[tag].numero,&(tags[tag].article),0);
	if (ret<0) {
           etat_loop.etat=1;
           etat_loop.hors_struct|=1;
           etat_loop.num_message=3;
	   return 0;
	}
      }
      Article_courant=tags[tag].article;
      etat_loop.etat=0;
    } else {
       /* faut changer de groupe */
      etat_loop.Newsgroup_nouveau =
	cherche_newsgroup(tags[tag].newsgroup_name,1,0);
      if(etat_loop.Newsgroup_nouveau == NULL) {
	etat_loop.etat=1; etat_loop.num_message=-8;
        etat_loop.hors_struct|=1;
	return 0;
      }
      /* si le pointeur est valide, c'est gagné ! */
      if (tags[tag].article_deb_key ==
	  etat_loop.Newsgroup_nouveau->article_deb_key) {
	etat_loop.Article_nouveau=tags[tag].article;
        etat_loop.num_futur_article=-1;
      } else
        etat_loop.num_futur_article=tags[tag].numero;
      return 1;
    }
  } else { /* le tag n'existe pas */
    etat_loop.etat=2;
    etat_loop.num_message=-13;
  }
  return 0;
}

int do_goto_tag(int res) {
  int ret;
  ret=my_goto_tag(((unsigned char)Arg_str[0])+NUM_SPECIAL_TAGS);
  return (ret > 0 ? 1 : 0);
}

/* Attention, je suppose que Newsgroup_courant->name
 * va rester valide   */
static int my_tag (Article_List *article,int tag) {
  if (tag >= MAX_TAGS) return -1;
  if (tag < 0) return -1;
  tags[tag].article_deb_key = Newsgroup_courant->article_deb_key;
  tags[tag].article = Article_courant;
  tags[tag].numero = Article_courant->numero;
/*  strcpy(tags[tag].newsgroup_name,Newsgroup_courant->name); */
  tags[tag].newsgroup_name=Newsgroup_courant->name;
  return 0;
}

static int my_tag_void(Article_List *article,void *vtag) {
  return my_tag(article, (int)(long) vtag);
}

int do_tag (int res) {
  int tag;
  Numeros_List *courant=&Arg_do_funcs;
  tag = ((unsigned char)Arg_str[0]) +NUM_SPECIAL_TAGS;
  distribue_action(courant,my_tag_void,NULL,(void *) (long) tag);
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
  max_tags_ptr%=NUM_SPECIAL_TAGS;
  tags_ptr=max_tags_ptr;
  res=my_tag(Article_courant,tags_ptr);
  return res;
}

int do_next(int res) {
  if (tags_ptr!=max_tags_ptr) {
    tags_ptr++;
    tags_ptr%=NUM_SPECIAL_TAGS;
  }
  return my_goto_tag(tags_ptr);
}

int do_back(int res) {
  tags_ptr += NUM_SPECIAL_TAGS-1;
  tags_ptr %= NUM_SPECIAL_TAGS;
  if ((tags_ptr==max_tags_ptr) || (tags[tags_ptr].article_deb_key==0)) {
    tags_ptr++;
    tags_ptr %= NUM_SPECIAL_TAGS;
  }
  return my_goto_tag(tags_ptr);
}

static int validate_tag_ptr(Flrn_Tag *tag) {
  Newsgroup_List *tmp=NULL;
  if (Newsgroup_courant && (Newsgroup_courant->article_deb_key == tag->article_deb_key))
    return 1;
  tmp = cherche_newsgroup(tag->newsgroup_name,1,0);
  if ((tmp==Newsgroup_courant) && (tag->article_deb_key==-1)) {
     if (Recherche_article(tag->numero, &(tag->article), 0)==0) return 1;
     return 0;
  }
  if (tmp && (tmp->article_deb_key == tag->article_deb_key))
    return 1;
  return 0;
}

static void hist_menu_summary(void *item, char *line, int length) {
  Flrn_Tag *tag = &tags[((int)(long)item)-1];
  *line='\0';
  if (validate_tag_ptr(tag)) {
    Prepare_summary_line(tag->article,NULL,0,line,length,1);
  }
}

int do_hist_menu(int res) {
  int i;
  char buf[80];
  Liste_Menu *menu=NULL, *courant=NULL, *start=NULL;
  int valeur;
  int j, dup, bla;

  if (max_tags_ptr==-1) {
    etat_loop.etat=2; etat_loop.num_message=-15; 
    return 0;
  }
  bla=(max_tags_ptr+1)%NUM_SPECIAL_TAGS;
  for (i=max_tags_ptr; tags[i].article_deb_key;
      i= (i+NUM_SPECIAL_TAGS-1)%NUM_SPECIAL_TAGS) {
    dup=0;
    for (j=max_tags_ptr; j!=i; j= (j+NUM_SPECIAL_TAGS-1)%NUM_SPECIAL_TAGS) {
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
    if (tags[i].numero <0) {
      snprintf(buf,80,"%s:?",tags[i].newsgroup_name);
    } else
      snprintf(buf,80,"%s:%ld",tags[i].newsgroup_name, tags[i].numero);
    courant=ajoute_menu(courant,safe_strdup(buf),(void *)(long)(i+1));
    if (tags[i].article==Article_courant) start=courant;
    if (!menu) menu=courant;
    if (i==bla) break;
  }
  if (!menu) {
    etat_loop.etat=2; etat_loop.num_message=-15; 
    return 0;
  }
  valeur = (int)(long) Menu_simple(menu, start, hist_menu_summary, NULL, "Historique. <entrée> pour choisir, q pour quitter...");
  if (!valeur) {
    etat_loop.etat=3;
    return 0;
  }
  valeur --;
  Libere_menu_noms(menu);
  tags_ptr=valeur;
  return my_goto_tag(valeur);
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
   if ((ret==0) && (Arg_do_funcs.flags & 34))
   	etat_loop.num_futur_article= Arg_do_funcs.elem1.num;
   if (ret>=0) return 1; else 
     if (ret==-1)  etat_loop.etat=3; else
   { etat_loop.etat=2; etat_loop.num_message=-8; 
     etat_loop.hors_struct|=1; }
   return 0;
}


/* do_unsubscribe : pour l'instant, j'ignore les arguments... J'aimerais */
/* bien pouvoir me desabonner a un ensemble de groupes...		 */
int do_unsubscribe(int res) {
   char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,2,str);
   if (ret==-2) ret=change_group(&newsgroup,0,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   newsgroup->flags |= GROUP_UNSUBSCRIBED;
   return (newsgroup==Newsgroup_courant);
}

/* do_abonne : pour l'instant, j'ignore les arguments...      J'aimerais */
/* bien pouvoir m'abonner a un ensemble de groupes...		 */
int do_abonne(int res) {
   char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,3,str);
   if (ret==-2) ret=change_group(&newsgroup,1,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   newsgroup->flags &= ~GROUP_UNSUBSCRIBED;
   if (Options.auto_kill) {
     add_to_main_list(newsgroup->name);
     Newsgroup_courant->flags|=GROUP_IN_MAIN_LIST_FLAG;
   }
   if (newsgroup==Newsgroup_courant) Aff_newsgroup_name(0);
   etat_loop.etat=1; etat_loop.num_message=9;
   return 0;
}

int do_add_kill(int res) {
  char *str=Arg_str;
  int ret;
  Newsgroup_List *newsgroup=Newsgroup_courant;

  ret=change_group(&newsgroup,3,str);
  if (ret==-2) ret=change_group(&newsgroup,1,str);
  if (ret==-2) 
  { etat_loop.etat=2; etat_loop.num_message=-8;
    etat_loop.hors_struct|=1; return 0; }
  if (newsgroup==NULL) {
     etat_loop.etat=2; etat_loop.num_message=-3;
     return 0;
  }
  add_to_main_list(newsgroup->name);
  newsgroup->flags|=GROUP_IN_MAIN_LIST_FLAG;
  if (newsgroup==Newsgroup_courant) Aff_newsgroup_name(0);
  etat_loop.etat=1; etat_loop.num_message=18;
  return 0;
}

int do_remove_kill(int res) {
   char *str=Arg_str;
   int ret;
   Newsgroup_List *newsgroup=Newsgroup_courant;

   ret=change_group(&newsgroup,3,str);
   if (ret==-2) ret=change_group(&newsgroup,1,str);
   if (ret==-2) 
   { etat_loop.etat=2; etat_loop.num_message=-8;
     etat_loop.hors_struct|=1; return 0; }
   if (newsgroup==NULL) {
      etat_loop.etat=2; etat_loop.num_message=-3;
      return 0;
   }
   remove_from_main_list(newsgroup->name);
   newsgroup->flags&=~GROUP_IN_MAIN_LIST_FLAG;
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
  return thread_action(article,0,le_machin->action,le_machin->flag,0);
}
int thread_distribue(Article_List *article, void * beurk) {
  Action_with_args *le_machin = (Action_with_args *) beurk;
  return thread_action(article,1,le_machin->action,le_machin->flag,0);
}
int gthread_distribue(Article_List *article, void * beurk) {
  Action_with_args *le_machin = (Action_with_args *) beurk;
  return gthread_action(article,0,le_machin->action,le_machin->flag,0);
}

static int omet_article(Article_List *article, void * toto)
{  if (article->flag & FLAG_KILLED) return 0;
   if ((article->numero>0) && (article->flag & FLAG_READ) &&
      (Newsgroup_courant->not_read!=-1)) {
        Newsgroup_courant->not_read++;
	article->thread->non_lu++;
      }
      article->flag &= ~FLAG_READ; return 0; }

/* Contrairement à article_read, on ne s'occupe pas des crossposts */
static int mark_article_read(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if ((article->numero>0) && (!(article->flag & FLAG_READ)) &&
      (Newsgroup_courant->not_read>0)) {
        Newsgroup_courant->not_read--;
	article->thread->non_lu--;
      }
   article->flag |= FLAG_READ; 
   if (article->flag & FLAG_IMPORTANT) {
      Newsgroup_courant->important--;
      article->flag &= ~FLAG_IMPORTANT;
   }
   article->flag |= flag; /* Si flag est FLAG_KILLED */
   return 0; 
}
static int mark_article_important(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if (article->flag & FLAG_READ) return 0;
   if (!(article->flag & FLAG_IMPORTANT)) Newsgroup_courant->important++;
   article->flag |= flag; 
   return 0; 
}
static int mark_article_unimportant(Article_List *article, void  *toto)
{ 
   int flag=*((int *)toto);
   if (article->flag & FLAG_IMPORTANT) Newsgroup_courant->important--;
   article->flag &= flag; 
   return 0; 
}

static void transforme_selection() {
   Article_List *parcours=Article_deb;

   while (parcours) {
      if (parcours->flag & FLAG_IS_SELECTED) 
              parcours->flag&= ~FLAG_IS_SELECTED;
      if (parcours->flag & FLAG_SUMMARY) {
          parcours->flag|=FLAG_IS_SELECTED;
	  parcours->flag&=~FLAG_SUMMARY;
      }
      parcours=parcours->next;
   }
   parcours=Article_exte_deb;
   while (parcours) {
      if (parcours->flag & FLAG_IS_SELECTED) 
              parcours->flag&= ~FLAG_IS_SELECTED;
      if (parcours->flag & FLAG_SUMMARY) {
          parcours->flag|=FLAG_IS_SELECTED;
	  parcours->flag&=~FLAG_SUMMARY;
      }
      parcours=parcours->next;
   }
}

int do_omet(int res) { 
  Numeros_List *courant=&Arg_do_funcs;
  Action_with_args beurk;
  int flag, col, use_summary=0;
  int toset;
  char *name=Arg_str;
  int use_argstr=0, ret=0;
  
  if ((res==FLCMD_OMET) || (res==FLCMD_GOMT)) {
     beurk.action=omet_article;
     beurk.flag=NULL;
  } else {
     while ((*name) && (isblank(*name))) name++;
     if (name[0]=='\0') {
       use_argstr=1;
       name=safe_malloc(30*sizeof(char));
       col=Aff_fin("Flag : ");
       if ((ret=getline(name, MAX_PATH_LEN, Screen_Rows-1,col)<0)) {
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
  if ((flag==FLAG_IS_SELECTED) && (toset) && (courant->flags & 64)) {
      flag=FLAG_SUMMARY;
      use_summary=1;
  }
  if (res==FLCMD_GOMT)
      distribue_action(courant,grand_distribue,NULL,&beurk);
  else
      distribue_action(courant,beurk.action,NULL,beurk.flag);
  if (use_summary) transforme_selection();
  Aff_not_read_newsgroup_courant();
  if (use_argstr) free(name);
  etat_loop.etat=1; etat_loop.num_message=(res==FLCMD_OMET ?
  				(res==FLCMD_GOMT ? 5 : 4) : 23);
  return 0;
}

/* La, j'ai un doute sur la semantique
 * KILL tue des threads ? */
static int kill_article(Article_List *article, void * toto)
{ article_read(article); return 0; }
static int temp_kill_article(Article_List *article, void * toto)
{ article_read(article); 
  article->flag|=FLAG_WILL_BE_OMITTED;
  return 0; }
int do_kill(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  Action_with_args beurk;
  
  beurk.action=kill_article;
  beurk.flag=NULL;
  if (res==FLCMD_KILL)
      distribue_action(courant,grand_distribue,NULL,&beurk);
  else if (res==FLCMD_PKIL)
      distribue_action(courant,kill_article,NULL,NULL);
  else if (res==FLCMD_ART_TO_RETURN) 
      distribue_action(courant,temp_kill_article,NULL,NULL);
  else
      distribue_action(courant,thread_distribue,NULL,&beurk);
  /* Est-ce un hack trop crade ? */
  courant->flags=0;
  Aff_not_read_newsgroup_courant();
  if (res==FLCMD_ART_TO_RETURN) {
     etat_loop.etat=1;
     etat_loop.num_message=22;
     return 0;
  }
  if (Article_courant->flag & FLAG_READ) {
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

  blah.next=NULL; blah.flags=32; blah.elem1.num=1;
  blah.num2=Newsgroup_courant->max;
  distribue_action(&blah,tag_article,NULL,&flag);
  flag=~FLAG_IMPORTANT;
  distribue_action(&blah,untag_article,NULL,&flag);
  Recherche_article(Newsgroup_courant->max,&Article_courant,-1);
  if (!Options.zap_change_group) {
    etat_loop.hors_struct|=3; /* Fin du conti */
    etat_loop.etat=1; etat_loop.num_message=11;
  }
  Newsgroup_courant->not_read=0;
  Newsgroup_courant->important=0;
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

static int check_and_tag_article(Article_List *article, void *blah)
{
  flrn_filter *arg = (flrn_filter *) blah;
  if (check_article(article,arg,0)<=0)
  { tag_article(article,& arg->action.flag); return 1; }
  else return 0;
}

static int tag_thread_article(Article_List *article, void *blah)
{
  flrn_filter *arg = (flrn_filter *) blah;
  if ((article->thread) && (check_article(article,arg,0)<=0))
  { article->thread->flags|=FLAG_THREAD_SELECTED; return 1; }
  else return 0;
}

/* do_summary Il faut toujours appeler Aff_summary
 * car Aff_summary retire le FLAG_SUMMARY
 * La, on a des comportements par défaut non triviaux, et qui
 * en plus ont le mauvais gout de dependre de la commande */
static int Do_aff_summary_line(Article_List *art, int *row,
    char *previous_subject, int level, Liste_Menu **courant) {
  return Aff_summary_line(art,row,previous_subject,level);
}

static int Do_menu_summary_line(Article_List *art, int *row,
    char *previous_subject, int level, Liste_Menu **courant) {
  char *buf= safe_malloc(Screen_Cols-2);
  Prepare_summary_line(art,previous_subject,level, buf, Screen_Cols-2,0);
  *courant=ajoute_menu(*courant,buf,art);
  return 0;
}

static void flags_summ_article (void *letruc, char *lachaine, int taille) {
   Article_List *larticle=letruc;
   char *buf;
   lachaine[0]='\0';
   if (taille<7) return;
   strcpy(lachaine,((larticle->flag & FLAG_READ) ? "  " : "un"));
   strcat(lachaine,"read");
   if (taille<19) return;
   if (taille<29) {
     strcat(lachaine,((larticle->flag & FLAG_IS_SELECTED) ? "   " : " un"));
     strcat(lachaine,"selected");
     return;
   } else {
     strcat(lachaine,((larticle->flag & FLAG_KILLED) ? "   " : " un"));
     strcat(lachaine,"killed");
     strcat(lachaine,((larticle->flag & FLAG_IS_SELECTED) ? "   " : " un"));
     strcat(lachaine,"selected");
   }
   if (taille<43) return;
   strcat(lachaine,((larticle->flag & FLAG_IMPORTANT) ? "   " : " un"));
   strcat(lachaine,"interesting");
   if (taille<65) return;
   if (larticle->headers==NULL) return;
   strcat(lachaine,"  Lignes : ");
   buf=lachaine+strlen(lachaine);
   if (larticle->headers->nb_lines) {
     snprintf(buf,taille-strlen(lachaine),"%i",larticle->headers->nb_lines);
   } else {
      if (larticle->headers->k_headers[LINES_HEADER]) {
        strncpy(buf,larticle->headers->k_headers[LINES_HEADER],taille-strlen(lachaine));
      } else strcpy(buf,"?");
   }
   lachaine[taille-1]='\0';
}

static int tag_article_summ (Liste_Menu *courant, char *arg) {
   Article_List *larticle=courant->lobjet;
   int ret, toset, flag;
   char flag_int=*(courant->nom);

   ret=parse_flags(arg,&toset,&flag);
   if (ret<0) return 0;
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
   *(courant->nom)=(larticle->flag & FLAG_READ ? ' ' : '*');
   if (*(courant->nom)!=flag_int) courant->changed=1;
   return courant->changed;
}

static int summary_menu (Liste_Menu *debut_menu, Liste_Menu **courant,
	char *name, int len, Cmd_return *la_commande, int *affiche) {
   int ret,cmd;
   Action_on_menu action=tag_article_summ;
   char *cst_char=la_commande->after;

   *affiche=0;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) return -1; else
   cmd=la_commande->cmd[CONTEXT_COMMAND];
   switch (cmd) {
      case FLCMD_KILL :
      case FLCMD_GKIL :
      case FLCMD_PKIL : cst_char="read";
      			break;
      case FLCMD_OMET :
      case FLCMD_GOMT : cst_char="unread";
      			break;
      case FLCMD_PUT_FLAG : break;
      default : strncpy(name,Messages[MES_UNKNOWN_CMD],len);
      		name[len-1]='\0';
		*affiche=1;
		return 0;
   }
   ret=distribue_action_in_menu(la_commande->before, cst_char,
   		debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   return ret;
}

static Article_List * raw_Do_summary (int deb, int fin, int thread,
    int action(Article_List *, int *, char *, int, Liste_Menu **)) {
  Article_List *parcours;
  Article_List *parcours2;
  char *previous_subject=NULL;
  int level=1;
  int row=0;
  Liste_Menu *courant=NULL, *menu=NULL, *start=NULL;

  /* On DOIT effacer l'écran pour les commandes summary de base... */
  Cursor_gotorc(1,0); Screen_erase_eos();
  /* find first article */
  parcours=Article_deb;
  while (parcours && (parcours->numero<deb)) parcours=parcours->next;
  while (parcours && !(parcours->flag & FLAG_SUMMARY))
    parcours=parcours->next;
  if (parcours && (thread || !Options.ordered_summary)) {
    parcours=root_of_thread(parcours,1);
    if (!(parcours->flag & FLAG_SUMMARY))
       parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	   FLAG_SUMMARY,1);
  }
  parcours2=parcours;

  while (parcours && (!fin || (parcours->numero<=fin))) {
    if ((*action)(parcours,&row,previous_subject,level,&courant)) {
      if(menu) Libere_menu_noms(menu); /* ne doit jamais arriver */
      return NULL;
    }
    if ((courant) && (!menu)) menu=courant;
    if ((courant) && (!start) && (parcours==Article_courant)) start=courant;
    if ((!Options.duplicate_subject) && (parcours->headers))
      previous_subject=parcours->headers->k_headers[SUBJECT_HEADER];
    parcours->flag &= ~FLAG_SUMMARY;
    if (thread || !Options.ordered_summary) {
      parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	  FLAG_SUMMARY,1);
    }
    if (!parcours || !(parcours->flag & FLAG_SUMMARY)) {
      while(parcours2 && (!(parcours2->flag & FLAG_SUMMARY)))
	parcours2=parcours2->next;
      parcours=parcours2; level=1;
      if (parcours && (thread || !Options.ordered_summary)) {
        parcours=root_of_thread(parcours,1);
        if (!(parcours->flag & FLAG_SUMMARY))
          parcours=next_in_thread(parcours,FLAG_SUMMARY,&level,deb,fin,
	     FLAG_SUMMARY,1);
      }
    }
  }
  /* on jouait avec un menu */
  if(menu) {
    parcours=Menu_simple(menu, start, flags_summ_article, summary_menu, "Résumé (q pour quitter).");
    Libere_menu_noms(menu);
    return parcours;
  }
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
static int tag_thread_in (Liste_Menu *courant, char *arg) {
   Thread_List *thr=(Thread_List *)(courant->lobjet);
   int ret, toset, flag;
   char flag_int=*(courant->nom);
   
   ret=parse_flags(arg,&toset,&flag);
   if (ret<0) return 0;
   switch (flag) {
      case FLAG_KILLED :
      case FLAG_READ : if (toset) {
       			  if (thr->flags & FLAG_THREAD_UNREAD) 
      				thr->flags&=~FLAG_THREAD_UNREAD;
			  else thr->flags|=FLAG_THREAD_READ;
		       } else {
       			  if (thr->flags & FLAG_THREAD_READ) 
      				thr->flags&=~FLAG_THREAD_READ;
			  else thr->flags|=FLAG_THREAD_UNREAD;
		       }
		       break;
      case FLAG_IMPORTANT : 
      		       if (toset) 
			  thr->flags|=FLAG_THREAD_IMPORTANT;
		       else
      			  thr->flags&=~FLAG_THREAD_IMPORTANT;
		       break;
       case FLAG_IS_SELECTED :
      		       if (toset) 
			  thr->flags|=FLAG_THREAD_BESELECTED;
		       else
      			  thr->flags&=~FLAG_THREAD_BESELECTED;
		       break;
   }
   *(courant->nom)=(thr->flags & FLAG_THREAD_READ ? '-' :
              (thr->flags & FLAG_THREAD_UNREAD ? '+' : 
	      (thr->flags & FLAG_THREAD_IMPORTANT ? 'I' :
	      (thr->flags & FLAG_THREAD_BESELECTED ? 's' : ' '))));
   if (*(courant->nom)!=flag_int) courant->changed=1;
   return courant->changed;
}

static int thread_menu (Liste_Menu *debut_menu, Liste_Menu **courant, char *name, int len, Cmd_return *la_commande, int *affiche) {
   int ret,cmd;
   Action_on_menu action=tag_thread_in;
   char *cst_char=la_commande->after;

   *affiche=0;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) return -1; else
   cmd=la_commande->cmd[CONTEXT_COMMAND];

   switch (cmd) {
      case FLCMD_KILL :
      case FLCMD_GKIL :
      case FLCMD_PKIL : cst_char="read";
      			break;
      case FLCMD_OMET :
      case FLCMD_GOMT : cst_char="unread";
			break;
      case FLCMD_PUT_FLAG : break;
      default : strncpy(name,Messages[MES_UNKNOWN_CMD],len);
      		name[len-1]='\0';
		*affiche=1;
		return 0;
   }
   ret=distribue_action_in_menu(la_commande->before, cst_char,
   		debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   return ret;
}

static int Menu_selector (Thread_List **retour) {
   Thread_List *parcours=Thread_deb;
   Hash_List *hash_parc;
   Article_List *art;
   Liste_Menu *courant=NULL, *menu=NULL, *start=NULL;
   char *buf, *buf2;
   int place, non_lu, lu, impor, selec, flag=FLAG_IMPORTANT;

   for (;parcours;parcours=parcours->next_thread) {
      parcours->flags&=~FLAG_THREAD_READ;
      parcours->flags&=~FLAG_THREAD_UNREAD;
      parcours->flags&=~FLAG_THREAD_IMPORTANT;
      parcours->flags&=~FLAG_THREAD_BESELECTED;
      if (!(parcours->flags & FLAG_THREAD_SELECTED)) continue;
      hash_parc=parcours->premier_hash;
      while ((hash_parc) && ((hash_parc->article==NULL) 
                         || (hash_parc->article->numero<=0)))
         hash_parc=hash_parc->next_in_thread;
      if (hash_parc==NULL) continue;
      buf=buf2=safe_malloc(Screen_Cols-2);
      art=root_of_thread(hash_parc->article,0);
      if ((art->headers==NULL) ||
          (art->headers->k_headers_checked[SUBJECT_HEADER] == 0))
        cree_header(art,0,0,0);
      if (art->headers==NULL)  continue;
      place=Screen_Cols-2;
      if (Screen_Cols-2>15) {
        if (parcours->number>9999) strcpy(buf," *** "); else
           sprintf(buf," %4d ",parcours->number);
	buf2=buf+6;
        if (parcours->non_lu>9999) strcpy(buf2," *** "); else
           sprintf(buf2,"%4d ",parcours->non_lu);
	buf2+=5;
	place-=11;
      }
      strncpy(buf2,art->headers->k_headers[SUBJECT_HEADER],place);
      courant=ajoute_menu(courant,buf,(void *)parcours);
      if (Article_courant->thread==parcours) start=courant;
      if (!menu) menu=courant;
   }
    
   if (menu) {
      (*retour)=(Thread_List *)Menu_simple(menu,start,NULL,thread_menu,"<total> <non lus>. q pour quitter.");
      Libere_menu_noms(menu);
   } else return -1;
   parcours=Thread_deb;
   for (;parcours;parcours=parcours->next_thread) {
       if (parcours->flags & (FLAG_THREAD_UNREAD | FLAG_THREAD_READ | FLAG_THREAD_IMPORTANT | FLAG_THREAD_BESELECTED)) {
         non_lu=parcours->flags & FLAG_THREAD_UNREAD;
         lu=parcours->flags & FLAG_THREAD_READ;
	 impor=parcours->flags & FLAG_THREAD_IMPORTANT;
	 selec=parcours->flags & FLAG_THREAD_BESELECTED;
         for (hash_parc=parcours->premier_hash;hash_parc;
	      hash_parc=hash_parc->next_in_thread) 
	    if (hash_parc->article) {
	       if (non_lu) omet_article(hash_parc->article,NULL);
	       else if ((lu) && ((hash_parc->article->numero>0) &&
	                (!(hash_parc->article->flag && FLAG_READ))))
	           kill_article(hash_parc->article,NULL);
	/* On évite de faire des requetes superflues et lourdes */
	       if (impor)
	          mark_article_important(hash_parc->article,(void *)(&flag));
	       if (selec)
	          hash_parc->article->flag|=FLAG_IS_SELECTED;
	    }
       }
   }
   return 0;
}


static int default_thread(Article_List *article, void *flag)
{
  Numeros_List blah;
  blah.next=NULL;
  blah.flags=8;
  blah.elem1.article=article;
  distribue_action(&blah,(Action) check_and_tag_article,NULL,flag);
  return 0;
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
  blah.flags=2;
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
  Article_List *ret=NULL;
  char *buf=Arg_str;

  filt=new_filter();
  filt->action.flag=FLAG_SUMMARY;
  act=check_and_tag_article;
  if (Arg_str && *Arg_str) {
    while(*buf==' ') buf++;
    if (parse_filter_flags(Arg_str,filt))
      parse_filter(Arg_str,filt);
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
  result = distribue_action(courant, act, defact, (void *)filt);
  free_filter(filt);
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
  char *buf=Arg_str;
  Thread_List *parcours=Thread_deb, *retour=NULL;
  Article_List *art_ret=NULL;

  filt=new_filter();
  act=tag_thread_article;
  if (Arg_str && *Arg_str) {
    while(*buf==' ') buf++;
    if (parse_filter_flags(Arg_str,filt))
      parse_filter(Arg_str,filt);
  }
  result = distribue_action(courant, act, NULL, (void *)filt);
  free_filter(filt);
  result=Menu_selector(&retour);
  if (result==-1) {
     etat_loop.hors_struct|=1;
     etat_loop.etat=1;
     etat_loop.num_message=10;
     return 0;
  }
  while (parcours) {
    parcours->flags &= ~FLAG_THREAD_SELECTED;
    parcours=parcours->next_thread;
  }
  etat_loop.etat=0;
  /* Est-ce un hack trop crade ? */
  courant->flags=0;
  Aff_not_read_newsgroup_courant();
  if (retour) {
      for (hash_parc=retour->premier_hash; hash_parc;
              hash_parc=hash_parc->next_in_thread) {
	 if (!hash_parc->article) continue;
	 if ((art_ret==NULL) || (!(Article_courant->flag & FLAG_READ))) 
	     art_ret=hash_parc->article;
      }
      if (art_ret) {
         Article_courant=root_of_thread(art_ret,1);
	 if ((!(art_ret->flag & FLAG_READ)) && 
	       ((Article_courant->flag & FLAG_READ) || (Article_courant->numero<0))) {
	    Article_List *myarticle=Article_courant;
	    while (myarticle && (myarticle->numero<0)) {
	       myarticle=Article_courant->prem_fils;
	       while (myarticle && (myarticle->frere_prev))
	           myarticle=myarticle->frere_prev;
	    }
	    if (myarticle) myarticle=next_in_thread(myarticle,FLAG_READ,NULL,
	    				0,0,0,1);
	    if (myarticle) Article_courant=myarticle;
	 }
      }
  } else
  if (Article_courant->flag & FLAG_READ) {
     etat_loop.hors_struct&=~2;
     do_deplace(FLCMD_SPACE);
  }
  else etat_loop.etat=3;
  return 0;
}

static int Sauve_article(Article_List *a_sauver, void *fichier);

/* do_save : on va essayer de voir... */
int do_save(int res) { 
  FILE *fichier;
  char *name=Arg_str;
  int ret, key, use_argstr=0, col;
  struct stat status;
  Numeros_List *courant=&Arg_do_funcs;

  while ((*name) && (isblank(*name))) name++;
  if (name[0]=='\0') {
    use_argstr=1;
    name=safe_malloc(MAX_PATH_LEN*sizeof(char));
    strcpy(name, Newsgroup_courant->name);
    name[0]=toupper(name[0]);
    col=Aff_fin("Sauver dans : ");
    Screen_write_string(name);
    if ((ret=getline(name, MAX_PATH_LEN, Screen_Rows-1,col)<0)) {
      free(name);
      etat_loop.etat=3;
      return 0;
    }
  }
  if (name[0]!='/' && (Options.savepath[0]!='\0')) {
    /* on n'a pas un path complet */
    char *fullname=safe_malloc(strlen(name)+strlen(Options.savepath)+1);
    strcpy(fullname,Options.savepath);
    strcat(fullname,"/");
    strcat(fullname,name);
    if (use_argstr) free(name);
    name=fullname;
  }
  if (stat(name,&status)==0) {
      if (res==FLCMD_SAVE_OPT)
        Aff_fin("Ecraser le fichier ? ");
      else Aff_fin("Ajouter au folder ? "); 
      key=Attend_touche();
      if ((KeyBoard_Quit) || (strchr("yYoO", key)==NULL))  {
        if (use_argstr) free(name);
        etat_loop.etat=3;
        return 0;
      }
  }
  if (res==FLCMD_SAVE_OPT)
    fichier=fopen(name,"w");
  else fichier=fopen(name,"a");
  if (fichier==NULL) {
    if (use_argstr) free(name);
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
    distribue_action(courant,Sauve_article,NULL ,fichier);
  else {
    Action_with_args beurk;

    beurk.action=Sauve_article;
    beurk.flag=fichier;
    distribue_action(courant,grand_distribue, NULL, &beurk);
  }
  fclose(fichier);
  etat_loop.etat=1; etat_loop.num_message=8;
  if (use_argstr) free(name);
  return 0;
}

/* do_launch_pager : voisin de do_save */
int do_launch_pager(int res) { 
  FILE *fichier;
  Numeros_List *courant=&Arg_do_funcs;
  int fd;

  fd=Pipe_Msg_Start(1,0,NULL);
  fichier=fdopen(fd,"w");
  if (fichier==NULL) {
    etat_loop.etat=2; etat_loop.num_message=-6;
    if (fd >=0)
      Pipe_Msg_Stop(fd);
    return 0;
  }
  distribue_action(courant,Sauve_article,NULL,fichier);

  if (fd>0) fclose(fichier);
  Pipe_Msg_Stop(fd);
  etat_loop.etat=3;
  return 0;
}

/* lit le fichier temporaire créé par un filtre */
/* appelée par do_pipe */
int display_filter_file(char *cmd, int flag) {
  FILE *file;
  char name[MAX_PATH_LEN];
  char *home;
  char prettycmd[MAX_CHAR_STRING];
  if (NULL == (home = getenv ("FLRNHOME")))
       home = getenv ("HOME");
  if (home==NULL) return -1;  /* TRES improbable :-) */
  strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_POST_FILE));
  strcat(name,"/"); strcat(name,TMP_POST_FILE);
  file=fopen(name,"r");
  if (file == NULL) {
    etat_loop.etat=2; etat_loop.num_message=-11;
    return 0;
  }
  if (flag) strcpy(prettycmd,"! ");
  else strcpy(prettycmd,"| ");
  strncat(prettycmd,cmd,MAX_CHAR_STRING-10);
  strcat(prettycmd," : ");
  Aff_file(file,NULL,NULL); /* c'est tres moche */
  fclose(file);
  Aff_fin(prettycmd);
  Attend_touche();
  etat_loop.etat=3;
  etat_loop.hors_struct|=1;
  return 0;
}

static int Sauve_header_article (Article_List *a_sauver, void *file_and_int);

/* do_pipe : on va essayer de voir... */
int do_pipe(int res) { 
  FILE *fichier;
  char *name=Arg_str;
  int ret, use_argstr=0, col, num_known_header=0;
  Numeros_List *courant=&Arg_do_funcs;
  int fd;


  while ((*name) && (isblank(*name))) name++;
  if (res==FLCMD_PIPE_HEADER) {
    if (name[0]=='\0') {
	etat_loop.etat=2; etat_loop.num_message=-17; return 0;
    }
    for (num_known_header=0; num_known_header<NB_KNOWN_HEADERS;
	 num_known_header++) {
	if (strncasecmp(name,Headers[num_known_header].header,
			Headers[num_known_header].header_len)==0)
	    break;
    }
    if (num_known_header==NB_KNOWN_HEADERS) {
	etat_loop.etat=2; etat_loop.num_message=-18; return 0;
    }
    name+=Headers[num_known_header].header_len;
    while ((*name) && (isblank(*name))) name++;
  }

  if (name[0]=='\0') {
    use_argstr=1;
    name=safe_malloc(MAX_PATH_LEN*sizeof(char));
    name[0]='\0';
    col=Aff_fin("Piper dans : ");
    if ((ret=getline(name, MAX_PATH_LEN, Screen_Rows-1,col)<0)) {
      free(name);
      etat_loop.etat=3;
      return 0;
    }
  }
  fd=Pipe_Msg_Start((res!=FLCMD_SHELLIN) && (res!=FLCMD_SHELL), 
      (res == FLCMD_FILTER ) || (res == FLCMD_GFILTER)
      || (res == FLCMD_SHELLIN), name);
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
    distribue_action(courant,Sauve_article,NULL ,fichier);
  else if (res==FLCMD_PIPE_HEADER) {
    struct file_and_int beurk2;
    beurk2.file=fichier;
    beurk2.num=num_known_header;
    distribue_action(courant,Sauve_header_article,NULL ,&beurk2);
  }
  else {
    Action_with_args beurk;
    beurk.action=Sauve_article;
    beurk.flag=fichier;
    if ((res != FLCMD_SHELL) && (res != FLCMD_SHELLIN))
      distribue_action(courant,grand_distribue, NULL, &beurk);
  }
  if (fd>0) fclose(fichier);
  Pipe_Msg_Stop(fd);
  if ((res == FLCMD_FILTER ) || (res == FLCMD_GFILTER)
      || (res == FLCMD_SHELLIN))
    display_filter_file(name, res == FLCMD_SHELLIN);
  etat_loop.etat=1; etat_loop.num_message=14;
  if (use_argstr) free(name);
  return 0;
}



/* do_post : lance post */
int do_post(int res) { 
  Article_List *origine;
  char *str=Arg_str;
  int ret;

  if ((res==FLCMD_ANSW) || (res==FLCMD_MAIL) || (res==FLCMD_SUPERSEDES)) {
     origine=Article_courant;
     if (Arg_do_funcs.flags & 34)
       ret=Recherche_article(Arg_do_funcs.elem1.num,&origine,0);
     else if (Arg_do_funcs.flags & 63) {
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
static int cancel_article(Article_List *article, void *toto)
 { return cancel_message(article); }
int do_cancel(int res) {
  Numeros_List *courant=&Arg_do_funcs;
  int ret;

  ret=distribue_action(courant,cancel_article,NULL,NULL);
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
  menu_config_variables();
  etat_loop.etat=3;
  etat_loop.hors_struct|=1;
  return 0;
}

int do_neth(int res) {  /* Très pratique, cette fonction, pour les tests idiots */
/* on va même lui donner une fonction utile... faudra la changer de nom */
  thread_view=1-thread_view;
  if (etat_loop.hors_struct & 8) {
     etat_loop.etat=1;
     etat_loop.num_message=(thread_view ? 24 : 25);
  } else etat_loop.etat=0;
  return 0;
}

#if 0
  unsigned char **grande_table;
  int i;

  Cursor_gotorc(1,0);
  Screen_erase_eos();
  grande_table=safe_malloc((Screen_Rows-1)*sizeof(char *));
  for (i=0;i<Screen_Rows-1;i++)
    grande_table[i]=safe_malloc(Screen_Cols);

  Aff_arbre(1,0,Article_courant,Screen_Cols/4-1,Screen_Cols/4-1,Screen_Rows-2,grande_table,0);
  for (i=0;i<Screen_Rows-1;i++)
    free(grande_table[i]);
  free(grande_table);
  etat_loop.etat=3;
  return 0; 
}
#endif


/* Affiche la liste des newsgroup */
/* res=FLCMD_GLIS -> liste tout */
int do_list(int res) {
   char *gpe=Arg_str;
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
  if (a_sauver->headers->k_headers[truc->num]==NULL) return 0;
  fprintf(truc->file,"%s\n",a_sauver->headers->k_headers[truc->num]);
  return 0;
}

/* Sauve l'article a_sauver dans fichier */
static int Sauve_article(Article_List *a_sauver, void *vfichier) {
  time_t date;
  FILE *fichier = (FILE *) vfichier;
  char *buf, *buf2, saved_char;

  if (Options.use_mailbox) {
    if ((a_sauver->headers) && (a_sauver->headers->k_headers[FROM_HEADER])
          && (a_sauver->headers->date_gmt)) {
      buf=strchr(a_sauver->headers->k_headers[FROM_HEADER],'<');
      if (buf) {
        buf++;
	buf2=strchr(buf,'>');
        if (buf2) {
	   saved_char=*buf2;
	   *buf2='\0';
	} else saved_char='\0';
      } else {
         buf=a_sauver->headers->k_headers[FROM_HEADER];
	 buf2=strchr(buf,'@');
	 *buf2='\0'; buf=strrchr(buf,' '); 
	 if (buf==NULL) buf=a_sauver->headers->k_headers[FROM_HEADER];
	 *buf2='@';
	 buf2=strchr(buf2,' ');
	 if (buf2) {
	    saved_char=*buf2;
	    *buf2='\0';
	 } else saved_char='\0';
      }
      fprintf(fichier, "From %s %s", buf, ctime(&(a_sauver->headers->date_gmt)));
      if (saved_char) *buf2=saved_char;
    } else {
      date=time(NULL);
      fprintf(fichier, "From %s%s %s", flrn_user->pw_name,"@localhost",
        ctime(&date)); 
    }
    fprintf(fichier, "Article: %d of %s\n", a_sauver->numero, Newsgroup_courant->name);
  } else
  fprintf(fichier, "Newsgroup : %s     No : %d\n", Newsgroup_courant->name,
	a_sauver->numero);
  Copy_article(fichier,  a_sauver, 1, NULL);
  fprintf(fichier,"\n");
  return 0;
}


/* Change de newsgroup pour récuperer le même article */
/* On va essayer de faire quelque chose d'assez complet */
typedef struct {
   Newsgroup_List *gpe;
   long numero;
} Flrn_Reference_swap;

static int art_swap_grp(char *xref, char *arg, Newsgroup_List *exclu) {
  int nostr=0, order=-1, best_order=-1;
  long numero;
  regex_t reg;
  char *newsgroup, *num, *tmp_name;
  Newsgroup_List *mygroup=NULL;
  int avec_un_menu=Options.use_menus;
  Liste_Menu *lemenu=NULL, *courant=NULL;
  Flrn_Reference_swap *objet;

  if ((arg==NULL) || (*arg=='\0')) nostr=1;
  if ((!nostr) && (Options.use_regexp)) {
    if(regcomp(&reg,arg,REG_EXTENDED)) {etat_loop.etat=2;
    	etat_loop.num_message=-10; return 0;} }
  /* Bon, faut que je parse le Xref... ça a déjà été fait dans article_read */
  newsgroup=strtok(xref," ");
  while ((newsgroup=strtok(NULL," :"))) {
    num=strtok(NULL, ": ");
    if (!num) { etat_loop.etat=1; etat_loop.num_message=3; 
    		if ((!nostr) && (Options.use_regexp))
		  regfree(&reg);
		return 0;}
    numero=atoi(num);
    if ((exclu) && (!avec_un_menu) && (strcmp(exclu->name,newsgroup)==0))
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
         *(num-1)=':';
         courant=ajoute_menu_ordre(&lemenu,newsgroup,objet,-1+nostr,order);
	 *(num-1)='\0';
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
	 if (mygroup==Newsgroup_courant) {
	    if (exclu==NULL) {
	        Article_List *nouveau=Article_courant;
		Recherche_article(numero,&nouveau,0);
		if (nouveau) Article_courant=nouveau; /* sur ! */
		etat_loop.etat=0;
		return 0;
            }
	    etat_loop.etat=1; etat_loop.num_message=12;
	    return 0;
	 } else {
    	    etat_loop.Newsgroup_nouveau=mygroup;
	    etat_loop.num_futur_article=numero;
	    return 1;
	 }
     } else {
         objet=(Flrn_Reference_swap *)Menu_simple(lemenu,NULL,NULL,NULL,
	     "Quel groupe ?");
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
  char *gpe=Arg_str, *buf;

  while ((*gpe) && (isblank(*gpe))) gpe++;
  article_considere=Article_courant;
  if (Arg_do_funcs.flags & 34) 
    Recherche_article(Arg_do_funcs.elem1.num, &article_considere, 0); else
  if (Arg_do_funcs.flags & 63) article_considere=Arg_do_funcs.elem1.article;
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
  buf=safe_strdup(article_considere->headers->k_headers[XREF_HEADER]);
  ret=art_swap_grp(buf,gpe,Newsgroup_courant);
  free(buf);
  return ret;
}

int do_art_msgid(int res) {
   Article_List article_temp;
   char *gpe=Arg_str;
   int ret;

   while ((*gpe) && (isblank(*gpe))) gpe++;
   article_temp.numero=-1;
   article_temp.msgid=gpe;
   article_temp.headers=NULL;
   cree_header(&article_temp,0,0,1);
   if (article_temp.headers==NULL) { 
      etat_loop.etat=2; etat_loop.num_message=-21;
      return 0;
   }
   ret=art_swap_grp(article_temp.headers->k_headers[XREF_HEADER],NULL,NULL);
   free_article_headers(article_temp.headers);
   return ret;
}

/* zap thread */
/* all >0 si l'on veut "tuer" aussi les cousins, et même =2 pour le BIG */
/* met le flag flag si zap >0, le retire sinon */
void zap_thread(Article_List *article,int all, int flag,int zap)
{
  Article_List *racine=article;
  int level=0;
  if (all) 
    racine = root_of_thread(racine,1);
  if (zap) {
    if (flag == FLAG_READ) article_read(racine); else
      racine->flag |= flag;
    while((racine=next_in_thread(racine,flag,&level,-1,0,0,all == 2)))
      if (flag == FLAG_READ) article_read(racine); else
	racine->flag |= flag;
  }
  else {
    racine->flag &= ~flag;
    while((racine=next_in_thread(racine,flag,&level,-1,0,flag,all == 2)))
      racine->flag &= ~flag;
  }
  return;
}

int do_keybindings(int res) {
   int i, ret, key=-1;
   int context=-1, row=1;
   char ligne[80], *buf=Arg_str, *buf2;
   Liste_Menu *menu=NULL, *courant=NULL;

   if (*buf) {
      while (isblank(*buf)) buf++;
      buf2=strchr(buf,' ');
      if (buf2) *buf2='\0';
      for (context=0; context<NUMBER_OF_CONTEXTS; context++) 
         if (strcasecmp(buf,Noms_contextes[context])==0) break;
      if (context==NUMBER_OF_CONTEXTS) context=-1;
   }
   for (i=0; i<4096; i++) {
      ret=aff_ligne_binding(i,context,ligne,79);
      if (ret<0) continue;
      if (context!=-1) {
         courant=ajoute_menu(courant,safe_strdup(ligne),(void *)(long)i);
	 if (!menu) menu=courant;
      } else row=ajoute_pager(ligne,row);
   }
   if ((context==-1) && (row>Screen_Rows-1)) {
      Init_Scroll_window(row-1,1,Screen_Rows-2);
      Aff_fin("Bindings de touche...");
      key=Page_message(row-1,1,0,1,1,NULL,NULL,NULL);
      free_text_scroll();
   } else {
      Menu_simple(menu,menu,NULL,NULL,Noms_contextes[context]);
      Libere_menu_noms(menu);
   }
   etat_loop.hors_struct|=1;
   etat_loop.etat=3+(key>=0);
   return 0;
}

int do_on_selected (int res) {
  char *name=Arg_str,*buf;
  int ret, key, res2;
  Numeros_List *courant=&Arg_do_funcs;

  while ((*name) && (isblank(*name))) name++;
  if (name[0]=='\0') {
    Aff_fin("Commande : ");
    key=Attend_touche();
    if (KeyBoard_Quit) {
       etat_loop.etat=3;
       return 0;
    }
    ret=get_command(key,CONTEXT_COMMAND,-1,&une_commande);
  } else { 
    une_commande.maybe_after=0;
    if ((name[1]=='\0') || (isblank(name[1]))) {
        une_commande.cmd[CONTEXT_COMMAND]=Flcmd_rev[CONTEXT_COMMAND][(int)(*name)];
	ret=(une_commande.cmd[CONTEXT_COMMAND]==FLCMD_UNDEF ? -1 : CONTEXT_COMMAND);
	une_commande.before=NULL;
	une_commande.after=safe_strdup(name+2);
    } else {
	ret=Lit_cmd_explicite(name,CONTEXT_COMMAND,-1,&une_commande);
	buf=name;
	while ((*buf) && (!isblank(*buf))) buf++;
	if (buf[0]) une_commande.after=safe_strdup(buf);
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
  Arg_str[0]='\0';
  res2=une_commande.cmd[CONTEXT_COMMAND];
  if (une_commande.before) Parse_nums_article(une_commande.before,NULL,0);
  if (une_commande.after) {
     res2=parse_arg_string(une_commande.after,res2,1);
     free(une_commande.after);
  }
  if ((res2 & FLCMD_MACRO)==0) {
     if ((une_commande.maybe_after) && 
         ( ((!Options.forum_mode) && (Flcmds[res2].flags & 8))
	 || ((Options.forum_mode) && (Flcmds[res2].flags & 4)) )) {
       ret=get_str_arg(res2,une_commande.before);
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
  courant->flags|=64;
  courant=courant->next;
  while ((courant) && (courant->flags)) {
     courant->flags|=64;
     courant = courant->next;
  }
  return (*Flcmds[res2].appel)(res2);
}

#if 0
/* passe au prochain thread ou il y a un message non lu */
/* renvoie -1 s'il n'y a rien d'autre */
int next_thread(int flags) {
  Article_List *racine=Article_deb;
  while(racine->next) {
    racine=racine->next;
    racine->flag &= ~FLAG_INTERNE1;
  }
  racine = Article_courant;
  zap_thread(racine, 1, FLAG_INTERNE1,2);
  while(racine && (racine->flag & (FLAG_INTERNE1 | FLAG_READ)))
  { racine = racine->next; }
  if (racine) { Article_courant=racine; return 0; }
  racine = Article_deb;
  while(racine != Article_courant &&
      (racine->flag & (FLAG_INTERNE1 | FLAG_READ)))
  { racine = racine->next; }
  if (!(racine->flag & (FLAG_INTERNE1 | FLAG_READ)))
  { Article_courant=racine; return 0; }
  return -1;
}
#endif

void Get_option_line(char *argument)
{
  int res=0, use_arg=1, ret=0;
  char *buf=argument;
  int color=0, col;
  while (isblank(*buf)) buf++;
  if (*buf=='\0') {
    use_arg=0;
    buf = safe_malloc(MAX_BUF_SIZE);
    *buf='\0';
    col=Aff_fin("Option: ");
    do {
      if (res>0) Aff_ligne_comp_cmd(buf,strlen(buf),col);
      if ((res=magic_getline(buf,MAX_BUF_SIZE,Screen_Rows-1,col,
	  "\011",0,ret))<0) {
        free(buf);
        return;
      }
      if (res>0)
        ret=Comp_general_command(buf,MAX_BUF_SIZE,col,options_comp,Aff_ligne_comp_cmd);
      if (ret<0) ret=0;
    } while (res!=0);
  }
  /* hack pour reconstruir les couleurs au besoin */
  color=(strstr(buf,"color"))?1:0;
  color=(strstr(buf,"mono"))?1:color;
  parse_options_line(buf,1);
  if (color) {
       Init_couleurs();
       Screen_touch_lines (0, Screen_Rows-1);
  }
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
int change_group(Newsgroup_List **newgroup, int flags, char *gpe_tab)
{
   char *gpe=gpe_tab;
   Newsgroup_List *mygroup=Newsgroup_courant, *tmpmygroup=NULL;
   regex_t reg;
   int avec_un_menu=Options.use_menus, correct, order=-1, best_order=-1;
   Liste_Menu *lemenu=NULL, *courant=NULL;
   char *tmp_name=NULL;

   while (*gpe==' ') gpe++;
   if (*gpe=='\0') return 1;

   if (Options.use_regexp) {
      if(regcomp(&reg,gpe,REG_EXTENDED)) {return -2;} }

   /* Si on a GGTO et qu'en plus on utilise les menus, on fait tout de suite */
   /* une requete...							     */
   if ((flags & 1) && avec_un_menu) {
     if (Options.use_regexp) {
        char *mustmatch;
	mustmatch=reg_string(gpe,1);
	if (mustmatch!=NULL) lemenu=menu_newsgroup_re(mustmatch, reg,1+(flags & 2));
	else {
	   regfree(&reg);
	   return -2;
	}
	free(mustmatch);
     } else lemenu=menu_newsgroup_re(gpe,reg,(flags & 2)); 
     	/* On copie n'importe quoi pour reg et c'est pas bo */
   } else {
     /* premiere passe pour voir si on trouve ca apres */
     if (mygroup)
     do { 
       mygroup=mygroup->next;
       /* On sépare les tests du while pour éviter une ligne trop longue */
       if (mygroup==NULL) break;
       if (flags & 2) tmp_name=truncate_group(mygroup->name,0); else
         tmp_name=mygroup->name;
       correct=((Options.use_regexp && 
                    ((order=calcul_order_re(tmp_name,&reg))!=-1))
	   || (!Options.use_regexp && 
	            ((order=calcul_order(tmp_name,gpe))!=-1))) &&
	   ((flags & 1) || !(mygroup->flags & GROUP_UNSUBSCRIBED));
       if (correct) {
         if (avec_un_menu) 
            courant=ajoute_menu_ordre(&lemenu,tmp_name,mygroup,-1,order);
	 else 
	   if ((best_order==-1) || (order<best_order)) {
	      best_order=order;
	      tmpmygroup=mygroup;
	   }
       }
     } while (1); 

     /* mygroup=NULL */
     mygroup=Newsgroup_deb;
     /* deuxieme passe en repartant du debut si on n'a pas trouve */
     while (mygroup!=Newsgroup_courant) {
       if (flags & 2) tmp_name=truncate_group(mygroup->name,0); else
         tmp_name=mygroup->name;
       correct=((Options.use_regexp && 
                    ((order=calcul_order_re(tmp_name,&reg))!=-1))
	   || (!Options.use_regexp && 
	            ((order=calcul_order(tmp_name,gpe))!=-1))) &&
	   ((flags & 1) || !(mygroup->flags & GROUP_UNSUBSCRIBED));
       if (correct) {
         if (avec_un_menu) 
           courant=ajoute_menu_ordre(&lemenu,tmp_name,mygroup,-1,order);
	 else
	   if ((best_order==-1) || (order<best_order)) {
	      best_order=order;
	      tmpmygroup=mygroup;
	   }
       }
       mygroup=mygroup->next;
     }
     /* mygroup=Newsgroup_courant */

     if (mygroup) {
       if (flags & 2) tmp_name=truncate_group(mygroup->name,0); else
         tmp_name=mygroup->name;
     }
     if (!avec_un_menu) {
        if (tmpmygroup) mygroup=tmpmygroup;
	else 
        if (flags & 1) { 
           /* on recupere la chaine minimale de la regexp */
           if (Options.use_regexp) {
	     char *mustmatch;
	     mustmatch=reg_string(gpe,1);
	     if (mustmatch!=NULL) {
	       if ((mygroup=cherche_newsgroup_re(mustmatch,reg,(flags & 2)?1:0))==NULL) {
	         free(mustmatch);
	         regfree(&reg);
	         return -2;
	       }
	     } else {
	       *newgroup=mygroup;
	       regfree(&reg);
	       return 0;
	     }
	     free(mustmatch);
           } else {
	     if ((mygroup=cherche_newsgroup(gpe,0,(flags & 2)?1:0))==NULL) {
	       return -2;
	     } else {
	       *newgroup=mygroup;
	       return 0;
	     }
           }
        } else {
	  if (Options.use_regexp) regfree(&reg);
	  return -2;
        }
     }
   }
   if (lemenu) {
     if (lemenu->suiv==NULL) mygroup=lemenu->lobjet;
     else
       mygroup=Menu_simple(lemenu,NULL,Ligne_carac_du_groupe,NULL,"Quel groupe ?");
     if (mygroup==NULL) mygroup=Newsgroup_courant;
     Libere_menu(lemenu); /* on est bien d'accord qu'on ne libere PAS les noms*/
     			  /* CAR action_select vaut NULL */
   } else if (avec_un_menu) {
     if (Options.use_regexp) regfree(&reg);
     return -2;
   }

   *newgroup=mygroup;
   if (debug && *newgroup) 
       fprintf(stderr, "Nouveau groupe : %s\n", (*newgroup)->name);
   if (Options.use_regexp) regfree(&reg);
   return (mygroup==Newsgroup_courant ? -1 : 0); /* on a bien fait le changement */
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
   if (myarticle && ((myarticle->flag & flag_mask)==flag_res))
     return 0; 

   /* On essaie d'abord de chercher dans la thread */
   if( Options.threaded_space) {
     myarticle=next_in_thread(myarticle,flag_mask,NULL,0,0,flag_res,1);
     if (myarticle) { *debut=myarticle; return 0; }
     myarticle=*debut;
   }
   /* On teste d'abord ce qu'on trouve après Article_courant */
   while (myarticle && ((myarticle->flag & flag_mask)!=flag_res))
      myarticle=myarticle->next;
   if (myarticle==NULL) { 
     /* Puis on repart de Article_deb */
     myarticle=Article_deb;
     while (myarticle && (myarticle!=*debut) && ((myarticle->flag & flag_mask)!=flag_res))
        myarticle=myarticle->next;
     if (myarticle==NULL) myarticle=Article_deb;
   }
   if (myarticle && ((myarticle->flag & flag_mask)==flag_res)) {
     if (Options.threaded_space) {
       myarticle=root_of_thread(myarticle,1);
       if ((myarticle->flag & flag_mask)!=flag_res)
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
     old_flag=save->flag;
     save->flag |= FLAG_READ;
     if (save->flag & FLAG_IMPORTANT) {
        save->flag &= ~FLAG_IMPORTANT;
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
     save->flag=old_flag;
     if (save->flag & FLAG_IMPORTANT)
	Newsgroup_courant->important++;
  }
  if (((*debut)->flag & FLAG_READ) == 0) {
    check_kill_article(*debut,1); /* le kill_file_avec création des headers */
    if (((*debut)->flag & FLAG_READ) != 0)
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
      if (!(mygroup->flags & GROUP_UNSUBSCRIBED )) {
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
      if (!(mygroup->flags & GROUP_UNSUBSCRIBED)) {
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
int parents_action(Article_List *article,int all, Action action, void *param, int flags) {
  Article_List *racine=article;
  int res=0, res2=0;
  while(racine->pere && !(racine->pere->flag & FLAG_INTERNE2)) {
    racine->flag |= FLAG_INTERNE2;
    racine=racine->pere;
  }
  do {
    if ((!(flags & 64)) || (racine->flag & FLAG_IS_SELECTED)) 
        res2=action(racine,param);
    racine->flag &= ~FLAG_INTERNE2;
    if (res2<res) res=res2;
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
int thread_action(Article_List *article,int all, Action action, void *param, int flags) {
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
  racine->flag &= ~FLAG_INTERNE1;
  while((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
      FLAG_INTERNE1,0)))
    racine->flag &= ~FLAG_INTERNE1;
  racine=racine2;
  /* Et la on peut faire ce qu'il faut */
  do { if (racine->numero!=-1) 
         if ((!(flags & 64)) || (racine->flag & FLAG_IS_SELECTED)) 
          res2=action(racine,param);
    if (res2<res) res=res2;
    racine->flag |= FLAG_INTERNE1;
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
int gthread_action(Article_List *article,int all, Action action, void *param, int flags) {
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
  racine->flag &= ~FLAG_INTERNE1;
  while((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
      FLAG_INTERNE1,1)))
    racine->flag &= ~FLAG_INTERNE1;
  racine=racine2;
  /* Et la on peut faire ce qu'il faut */
  do { if (racine->numero!=-1)
         if ((!(flags & 64)) || (racine->flag & FLAG_IS_SELECTED)) 
          res2=action(racine,param);
    if (res2<res) res=res2;
    racine->flag |= FLAG_INTERNE1;
  } while ((racine=next_in_thread(racine,FLAG_INTERNE1,&level,-1,0,
            0,1)));
  return res; 
}


/* Appelle action sur tous les articles de Numero_List */
int distribue_action(Numeros_List *num, Action action, Action special,
    void *param) {
  Article_List *parcours=NULL;
  int res,result=0,res2;
  if (num->flags==0) {
    if (!Article_courant) return -1;
    if (special) return special(Article_courant,param);
    return action(Article_courant,param);
  } else if (num->flags==64) {
      if (!Article_courant) return -1;
      num->flags=96; num->elem1.num=1;
      num->num2=Newsgroup_courant->max;
  }
  while (num) {
    if (num->flags==0) {return 0;}
    if (num->flags & 34) 
       res=Recherche_article(num->elem1.num,&parcours,1);
    else {
       parcours=num->elem1.article;
       res=(parcours ? 0 : -2);
    }
    res2=0;
    switch (num->flags & 63) {
      case 1: case 2 : if (num->flags & 64) {
                           if ((res==0) && (!(parcours->flag & FLAG_IS_SELECTED)))
			        res=-1;
		       }
                  if (res==0) res2=action(parcours,param);
	      break;
      case 4: if (res==0) res2=parents_action(parcours,0,action,param, num->flags);
		break;
      case 8: if (res==0) res2=thread_action(parcours,0,action,param, num->flags);
		break;
      case 16: if (res==0) res2=gthread_action(parcours,0,action,param,num->flags);
		break;
      case 32: if ((res==0) || (res==-1)) {
		while(parcours) {
		  if (parcours->numero > num->num2) break;
		  if ((num->flags==32) || (parcours->flag & FLAG_IS_SELECTED)) 
		       res2=action(parcours,param);
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


