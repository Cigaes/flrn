/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      tty_display.c : gestion de l'écran, affichage des articles
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/*  Programme de gestion de l'écran et d'affichage des articles.        */
/*  S'occupe aussi des entrees des posts.                               */
/*  Faisait aussi la gestion des scrollings.                            */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include "flrn.h"
#include "options.h"
#include "art_group.h"
#include "group.h"
#include "flrn_menus.h"
#include "flrn_tcp.h"
#include "flrn_files.h"
#include "flrn_format.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_menus.h"
#include "flrn_pager.h"
#include "flrn_regexp.h"
#include "flrn_filter.h"
#include "flrn_color.h"
#include "flrn_slang.h"
#include "flrn_xover.h"
#include "flrn_messages.h"
#include "enc/enc_strings.h"
#ifdef USE_SLANG_LANGUAGE
#include <slang.h>
#include "slang_flrn.h"
#endif

static UNUSED char rcsid[]="$Id$";

/* place des objets de la barre */
int screen_inited;
int name_news_col, num_art_col, num_rest_col, num_col_num, name_fin_col;
int num_help_line=0;
/* place des messages d'erreur */
int row_erreur, col_erreur;
int error_fin_displayed=0;
/* pour l'arbre */
Colored_Char_Type **table_petit_arbre;

/* Objet courant dans l'affichage de l'article */
/* Espace actuellement conservé dans l'affichage de l'article */
int saved_field, saved_space, en_deca;

/* (re-)Determinations des caractéristiques de la fenêtre */
/* si col_num!=0, c'est la taille à réserver pour les numéros */
static int Size_Window(int flag, int col_num) {
   int aff_help=0, aff_mess=1, resconv, colc;
   char *name_program2;

   resconv=conversion_to_terminal(name_program,&name_program2,0,(size_t)(-1));
   if (resconv>0) name_program2=safe_strdup(name_program2);
   name_news_col=str_estime_width(name_program2,0,(size_t)-1)+1;
   if (name_news_col>9) name_news_col=9;
   if (col_num==0) col_num=num_col_num; else num_col_num=col_num;
   num_rest_col=Screen_Cols-2-5;
   num_art_col=num_rest_col-10-2*col_num;

   if (num_art_col<name_news_col+5) {
       /* FIXME : français */
      if (debug || flag) fprintf(stderr, "Nombre de colonnes insuffisant.\n");
      return 0;
   } else if (num_art_col<name_news_col+20) aff_mess=0; else
     if (num_art_col>name_news_col+28) aff_help=1;
   if (aff_help) name_news_col+=9;
   if (aff_mess==0) num_art_col+=7;
   if (Screen_Rows<3+2*Options.skip_line) {
      row_erreur=(Screen_Rows ? 1 : 0);
      /* FIXME : français */
      if (debug || flag) fprintf(stderr, "Nombre de lignes insuffisant.\n");
      return 0;
   } else 
      row_erreur=Screen_Rows/2;
   col_erreur=3;

   Screen_set_color(FIELD_STATUS);
   Screen_erase_eol(); /* Juste pour tout remplir */
   name_program2[0]=(char) toupper((int) name_program2[0]);
   colc=0;
   name_program2[to_make_width(name_program2, 8, &colc, 0)]='\0';
   Screen_write_string(name_program2);
   free(name_program2);
   /* FIXME : français */
   if (aff_help) Screen_write_string(" (?:Aide)");
   Screen_write_string(": ");
   name_fin_col=num_art_col-1;
   Cursor_gotorc (0,num_art_col);
   /* FIXME : français */
   if (aff_mess) Screen_write_string("Message");
   Screen_write_char(' ');
   num_art_col+=(aff_mess ? 8 : 1);
   Cursor_gotorc (0,num_art_col+col_num);
   Screen_write_char('/');
   Cursor_gotorc (0,num_rest_col);
   Screen_write_char('(');
   Cursor_gotorc (0,Screen_Cols-1);
   Screen_write_char(')');
   num_rest_col++;
   Screen_set_color(FIELD_NORMAL);

   return 1;
}

/* SIGWINCH */
void sig_winch(int sig) {
   
   Screen_get_size(Options.help_line);
   Screen_reset();
   Screen_init_smg ();
   if (Size_Window(0,0)) {
     if (Newsgroup_courant) { Aff_newsgroup_name(0); Aff_newsgroup_courant(); }
   /* FIXME : français */
     Aff_error (fl_static("Changement de taille de la fenetre"));
   /* FIXME : français */
   } else Aff_error (fl_static("Fenetre trop petite !!!"));
   Screen_refresh();
   sigwinch_received=1;
   /*SL*/signal(SIGWINCH, sig_winch);

   return;
}

int screen_changed_size() {
   int saved_Screen_Rows=Screen_Rows,
       saved_Screen_Rows2=Screen_Rows2,
       saved_Screen_Cols=Screen_Cols;

   Screen_get_size(Options.help_line);
   if ((Screen_Rows!=saved_Screen_Rows) ||
       (Screen_Rows2!=saved_Screen_Rows2) ||
       (Screen_Cols!=saved_Screen_Cols)) {
       Screen_reset(); 
       Screen_init_smg ();
       if (Size_Window(0,0)) {
          if (Newsgroup_courant)
	      { Aff_newsgroup_name(0); Aff_newsgroup_courant(); }
   /* FIXME : français */
          Aff_error (fl_static("Changement de taille de la fenetre"));
   /* FIXME : français */
       } else Aff_error (fl_static("Fenetre trop petite !!!"));
       Screen_refresh();
       return 1;
   }
   return 0;
}

/* Initialise l'écran, crée la barre */
int Init_screen(int stupid_term) {
   int res;

   table_petit_arbre=safe_malloc(7*sizeof(Colored_Char_Type *));
   for (res=0;res<7;res++)
     table_petit_arbre[res]=safe_malloc(11*sizeof(Colored_Char_Type));
   Get_terminfo (Options.help_line);
   if (stupid_term) Set_term_cannot_scroll(1);
   res=Screen_init_smg ();
  
#if 0  /* Les valeurs de retour de la fonction ont changés */
   if (res==0) {
      fprintf(stderr, "Mémoire insuffisante pour initialiser l'écran.\n");
      return 0;
   }
#endif
   Init_couleurs();
   screen_inited=1;

#ifdef USE_SLANG_LANGUAGE
   (void) change_SLang_Error_Hook(1);
#endif

   /*SL*/signal(SIGWINCH, sig_winch);
    
   return Size_Window(1,5);
}

void Reset_screen() {
  int res;
  for (res=0;res<7;res++)
    free(table_petit_arbre[res]);
  free(table_petit_arbre);
  screen_inited=0;

#ifdef USE_SLANG_LANGUAGE
  (void) change_SLang_Error_Hook(0);
#endif

  Screen_reset();
}



/* Affiche un message en bas de l'ecran.				*/
/* renvoie la colonne d'arrivée */
int Aff_fin(const flrn_char *str) {
   int col=0;
   int resconv;
   char *trad;

   Cursor_gotorc(Screen_Rows2-1,0);
#ifdef CHECK_MAIL
   Screen_set_color(FIELD_NORMAL);
   if (Options.check_mail && newmail(mailbox)) {
      Screen_write_string("(Mail)");
      col=6;
   }
#endif
   resconv=conversion_to_terminal(str,&trad,0,(size_t)(-1));
   Screen_set_color(FIELD_AFF_FIN);
   Screen_write_string(trad);
   Screen_set_color(FIELD_NORMAL);
   Screen_erase_eol();
   col+=str_estime_width(trad,col,(size_t)(-1));
   if (resconv==0) free(trad);
   Cursor_gotorc(Screen_Rows2-1, col); /* En changeant l'objet */
    				       /* on change la pos du  */
   				       /* curseur.		  */
   return (col);
}

/* Affiche un message sur l'écran (pour l'instant d'une seule ligne max). */
int Aff_error(const flrn_char *str) {
   int resconv;
   char *trad;

   Cursor_gotorc(1,0);
   Screen_erase_eos();
   Aff_help_line(Screen_Rows-1);
   Cursor_gotorc(1,0);
   Screen_set_color(FIELD_ERROR);
   Cursor_gotorc(row_erreur,col_erreur); 
   resconv=conversion_to_terminal(str,&trad,0,(size_t)(-1));
   Screen_write_string(trad);
   if (resconv==0) free(trad);
   Screen_set_color(FIELD_NORMAL);
   error_fin_displayed=0;
   return 0;
}

/* Affiche un message d'erreur en fin... */
int Aff_error_fin(const flrn_char *str, int s_beep, int short_e) {
   int resconv;
   char *trad;
   if (short_e==-1) short_e=Options.short_errors;
   Cursor_gotorc(Screen_Rows2-1,0);
#ifdef CHECK_MAIL
   Screen_set_color(FIELD_NORMAL);
   if (Options.check_mail && newmail(mailbox)) {
      Screen_write_string("(Mail)");
   }
#endif
   Screen_set_color(FIELD_ERROR);
   resconv=conversion_to_terminal(str,&trad,0,(size_t)(-1));
   Screen_write_string(trad);
   if (resconv==0) free(trad);
   if (s_beep) Screen_beep();
/* Contrairement à Aff_fin, on ne cherche pas à afficher le curseur */
/* après le message...						    */
   Screen_set_color(FIELD_NORMAL);
   Screen_erase_eol();
   Cursor_gotorc(Screen_Rows2-1, Screen_Cols-1); 
   Screen_refresh();
   if (short_e) sleep(1); else error_fin_displayed=1;
   return 0;
}

void Manage_progress_bar(flrn_char *msg, int reset) {
   static int col=0;
   if (screen_inited==0) return;
   Screen_set_color(FIELD_NORMAL);
   if (reset==1) {
       /* FIXME : français */
        Aff_fin((msg ? msg : fl_static("Patientez...")));
        Cursor_gotorc(Screen_Rows2-1, Screen_Cols-13);
	Screen_write_string("[          ]");
	col=Screen_Cols-12;
	Screen_refresh();
	return;
   }
   Cursor_gotorc(Screen_Rows2-1, col);
   Screen_write_string("*");
   col++;
   if (col>Screen_Cols-3) col=Screen_Cols-3;
   Cursor_gotorc(Screen_Rows2-1, Screen_Cols-1);
   Screen_refresh();
}

void aff_try_reconnect() {
   if (screen_inited) 
   /* FIXME : français */
      Aff_error_fin(fl_static("Timeout ? J'essaie de me reconnecter..."),
	      1,0);
   error_fin_displayed=0;
}

void aff_end_reconnect() {
   if (screen_inited) 
   /* FIXME : français */
      Aff_error_fin(fl_static("Reconnexion au serveur effectuee..."),0,1);
}

/* Affichage de la ligne résumé d'un article */
/* NE FAIT PAS LE REFRESH, heureusement ! */
static void raw_Aff_summary_line(Article_List *article, int row,
    flrn_char *previous_subject,
    int level) {
  flrn_char *buf= safe_malloc(Screen_Cols*2);

  void aff_line(flrn_char *bout, size_t sbout, int color) {
      char *trad;
      flrn_char save=bout[sbout];
      int resconv;
      bout[sbout]=fl_static('\0');
      resconv=conversion_to_terminal(bout,&trad,0,(size_t)(-1));
      Screen_set_color(color);
      Screen_write_string(trad);
      if (resconv==0) free(trad);
      bout[sbout]=save;
      return;
  }
  /* TODO : changer ça : on ne peut pas prévoir la taille des
   * lignes, il va donc falloir s'y prendre autrement */
  buf=Prepare_summary_line(article,previous_subject, level, buf, Screen_Cols*2,
	  Screen_Cols, 0, 0, 1);
  Cursor_gotorc(row,0);
  create_Color_line(&aff_line,FIELD_SUMMARY,buf,fl_strlen(buf),FIELD_SUMMARY);
  free(buf);
}

int Aff_summary_line(Article_List *art, int *row,
    flrn_char *previous_subject, int level) {
  struct key_entry key;
  key.entry=ENTRY_ERROR_KEY;
  if (*row==0) *row=1+Options.skip_line;
  if (*row>=Screen_Rows-1-Options.skip_line) {
      /* FIXME : français */
    Aff_fin(fl_static("-suite-"));
    Attend_touche(&key);
    if (KeyBoard_Quit) { Free_key_entry(&key); return 1; }
    Cursor_gotorc(1,0);
    Screen_erase_eos();
    num_help_line=14;
    Aff_help_line(Screen_Rows-1);
    Cursor_gotorc(1,0);
    *row=1+Options.skip_line;
    previous_subject=NULL;
  } 
  raw_Aff_summary_line(art,(*row)++,previous_subject,level);
  Free_key_entry(&key);
  return 0;
}

/* Calcul le flag d'un groupe */
static flrn_char *calcul_flag (Newsgroup_List *groupe) {
   int num_flag;
   size_t bla, ltmp;

   num_flag=(groupe->flags & GROUP_READONLY_FLAG ? 4 : 0)+
   		(groupe->flags & GROUP_UNSUBSCRIBED ? 2 : 0)+
		(groupe->flags & GROUP_IN_MAIN_LIST_FLAG ? 0 : 1);
   if (Options.flags_group) {
       bla=0;
       while ((num_flag>0) && (Options.flags_group[bla])) {
	   ltmp=next_flch(Options.flags_group,bla);
	   bla+=((int)ltmp>0 ? ltmp : 1);
	   num_flag--;
       }
       if ((Options.flags_group[bla]==fl_static('\0')) ||
	       (Options.flags_group[bla]==fl_static(' '))) return NULL;
       else {
	   flrn_char *res=safe_malloc((fl_strlen(Options.flags_group+bla)+1)
		    * sizeof(flrn_char));
	   fl_strcpy(res,Options.flags_group+bla);
	   ltmp=next_flch(res,0);
	   res[((int)ltmp>0 ? ltmp : 0)]=fl_static('\0');
	   return res;
       }
   }
   return NULL;
}

/* pour chg_grp_in */
static int abon_group_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *choisi=(Newsgroup_List *)(courant->lobjet);
   flrn_char *flag;

   if (!(choisi->flags & GROUP_UNSUBSCRIBED)) return 0;
   choisi->flags&=~GROUP_UNSUBSCRIBED;
   if (choisi!=Newsgroup_courant) {
       flag=calcul_flag(choisi);
       change_menu_line(courant,0,flag);
       return 1;
   }
   return 0;
}
static int unsu_group_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *choisi=(Newsgroup_List *)(courant->lobjet);
   flrn_char *flag;

   if (choisi->flags & GROUP_UNSUBSCRIBED) return 0;
   choisi->flags|=GROUP_UNSUBSCRIBED;
   if (choisi!=Newsgroup_courant) {
       flag=calcul_flag(choisi);
       change_menu_line(courant,0,(flag ? flag : 
		   (NoArt_non_lus(choisi,0)>0 ? fl_static("+") :
		    fl_static(""))));
       if (flag) free(flag);
       return 1;
   }
   return 0;
}
static int goto_group_in (Liste_Menu *courant, char *arg) {
   return -1;
}
static int remov_group_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *choisi=(Newsgroup_List *)(courant->lobjet);
   flrn_char *flag;

   if (!(choisi->flags & GROUP_IN_MAIN_LIST_FLAG)) return 0;
   choisi->flags&=~GROUP_IN_MAIN_LIST_FLAG;
   remove_from_main_list(choisi->name);
   if (choisi!=Newsgroup_courant) {
       flag=calcul_flag(choisi);
       change_menu_line(courant,0,(flag ? flag : 
		   (NoArt_non_lus(choisi,0)>0 ? fl_static("+") :
		    fl_static(""))));
       if (flag) free(flag);
       return 1;
   }
   return 0;
}
static int add_group_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *choisi=(Newsgroup_List *)(courant->lobjet);
   flrn_char *flag;

   if (choisi->flags & GROUP_IN_MAIN_LIST_FLAG) return 0;
   choisi->flags|=GROUP_IN_MAIN_LIST_FLAG;
   add_to_main_list(choisi->name);
   if (choisi!=Newsgroup_courant) {
       flag=calcul_flag(choisi);
       change_menu_line(courant,0,(flag ? flag : 
		   (NoArt_non_lus(choisi,0)>0 ? fl_static("+") :
		    fl_static(""))));
       if (flag) free(flag);
       return 1;
   }
   return 0;
}
static int zap_group_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *choisi=(Newsgroup_List *)(courant->lobjet);
   flrn_char *flag;

   if  (choisi==Newsgroup_courant) {
      do_zap_group(FLCMD_ZAP);
      return 0;
   }
   zap_group_non_courant(choisi);
   flag=calcul_flag(choisi);
   if (flag) { free(flag); return 0; }
   change_menu_line(courant,0,"");
   return 1;
}

/* Fonctions utilisées pour Liste_groupe... On devrait tout déplacer */
/* dans group.c à mon avis. Tout ça n'a plus rien à faire là.        */
/* Quoique... meme ici il y a de l'affichage...			     */
int chg_grp_in(Liste_Menu *debut_menu, Liste_Menu **courant, flrn_char **name, int *tofree, Cmd_return *la_commande) {
   int ret=-1,cmd,rc;
   const char *msg=NULL;
   Action_on_menu action=NULL;
   
   *tofree=0;
   *name=NULL;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) return -1;
   cmd=la_commande->cmd[CONTEXT_COMMAND];

   switch (cmd) {
      case FLCMD_ABON : action=abon_group_in; 
			msg=_(Messages[MES_ABON]);
      			break;
      case FLCMD_UNSU : action=unsu_group_in; 
			msg=_(Messages[MES_DESABON]);
      			break;
      case FLCMD_GOTO : case FLCMD_GGTO : action=goto_group_in; break;
      case FLCMD_REMOVE_KILL : action=remov_group_in;
			       msg=_(Messages[MES_OP_DONE]);
			       break;
      case FLCMD_ADD_KILL : action=add_group_in;
			    msg=_(Messages[MES_OP_DONE]);
			    break;
      case FLCMD_ZAP : action=zap_group_in;
		       msg=_(Messages[MES_ZAP]);
		       break;
      default : msg=_(Messages[MES_UNKNOWN_CMD]);
		ret=0;
   }
   if (ret==-1)
      ret=distribue_action_in_menu(la_commande->before, la_commande->after,
   		  debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   /* FIXME:  ici la conversion est faite en fin... vérifier */
   rc=conversion_from_utf8(msg,name,0,(size_t)(-1));
   *tofree=(rc==0);
   return ret;
}

static int abon_group_not_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *creation;
   flrn_char *nom_groupe=(flrn_char *)(courant->lobjet);

   creation=cherche_newsgroup(nom_groupe, 1, 0);
   if (!creation) return -2;
   creation->flags&=~GROUP_UNSUBSCRIBED;
   if ((Options.auto_kill) && (!in_main_list(creation->name))) {
     creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
     add_to_main_list(creation->name);
   }
   change_menu_line(courant,0,fl_static("A"));
   return 1;
}
static int unsu_group_not_in (Liste_Menu *courant, char *arg) {
   Newsgroup_List *creation;
   flrn_char *nom_groupe=(flrn_char *)(courant->lobjet);

   creation=Newsgroup_deb;
   while (creation) {
      if (strcmp(creation->name,nom_groupe)==0) break;
      creation=creation->next;
   }
   if (!creation) return -2;
   creation->flags|=GROUP_UNSUBSCRIBED;
   change_menu_line(courant,0,fl_static(""));
   return 1;
}

/* Cette fonction NE DOIT PAS renvoyer -1 si on veut éviter une horreur de 
   typage */
int chg_grp_not_in(Liste_Menu *debut_menu, Liste_Menu **courant, flrn_char **name, int *tofree, Cmd_return *la_commande) {
   int ret=-10,cmd, rc;
   const char *msg=NULL;
   Action_on_menu action=abon_group_not_in;

   *tofree=0;
   *name=NULL;
   if (la_commande->cmd[CONTEXT_MENU]!=FLCMD_UNDEF) ret=-1;
   else {
     cmd=la_commande->cmd[CONTEXT_COMMAND];
     if (cmd==FLCMD_ABON) {
	msg=_(Messages[MES_ABON]);
     } else if (cmd==FLCMD_UNSU) {
        action=unsu_group_not_in;
	msg=_(Messages[MES_DESABON]);
     } else if ((cmd==FLCMD_GOTO) || (cmd==FLCMD_GGTO)) 
        action=goto_group_in;
     else {
	msg=_(Messages[MES_UNKNOWN_CMD]);
        ret=0;
     }
   }
   if (ret==-10) 
     ret=distribue_action_in_menu(la_commande->before, la_commande->after,
                 debut_menu, courant, action);
   if (la_commande->before) free(la_commande->before);
   if (la_commande->after) free(la_commande->after);
   if (msg) {
       /* FIXME : ici la conversion est faite en fin */
     rc=conversion_from_utf8(msg,name,0,(size_t)(-1));
     *tofree=(rc==0);
   }
   return ret;
}

/* Fonctions pour Liste_groupe */
/* en fonction du passage, et de Options.use_menus */
/* un ajoute_elem, et un fin_passage -> 4 fonctions */
/* FIXME : francais */
static char *chaine0="Newsgroups auquels vous êtes abonnés.";
static char *chaine1="Newsgroups présents dans le .flnewsrc.";
static char *chaine2="Autres newsgroups.";
int ajoute_elem_not_menu (void *element, int passage) {
   Newsgroup_List *groupe=NULL;
   flrn_char *trad=NULL;
   char *ligne=NULL;
   static int row;
   flrn_char *flag;
   int rc1,free_flag=0;

   if (element==NULL) {
      int rc2;
      row=1+Options.skip_line;
      Cursor_gotorc(1,0);
      Screen_erase_eos();
      num_help_line=4;
      Aff_help_line(Screen_Rows-1);
      Cursor_gotorc(row++,0);
      rc1=conversion_from_utf8(passage == 0 ? chaine0 :
	      (passage==1 ? chaine1 : chaine2), &trad,0,(size_t)(-1));
      rc2=conversion_to_terminal(trad,&ligne,0,(size_t)(-1));
      Screen_write_string(ligne);
      if (rc2==0) free(ligne);
      if (rc1==0) free(trad);
      return 0;
   }
   if (passage<2) groupe=(Newsgroup_List *)element; else
   		  trad=(flrn_char *)element;
   if (row>Screen_Rows-2-Options.skip_line) {
       /* FIXME: français */
      Aff_fin(fl_static("-suite-"));
      Attend_touche(NULL);
      if (KeyBoard_Quit) return -1;
      Cursor_gotorc(1,0);
      Screen_erase_eos();
      num_help_line=4;
      Aff_help_line(Screen_Rows-1);
      row=1+Options.skip_line;
   }
   if (passage<2) {
     Cursor_gotorc(row,2);
     if (groupe==Newsgroup_courant) flag = fl_static(">");
       else { 
	   flag=calcul_flag(groupe); 
	   if (flag==NULL)  flag=((NoArt_non_lus(groupe,0)>0) ? fl_static("+") :
		   fl_static(" "));
	   else free_flag=1;
     }
     rc1=conversion_to_terminal(flag,&ligne,0,(size_t)(-1));
     Screen_write_string(ligne);
     if (rc1==0) free(ligne); ligne=NULL;
     if (free_flag) free(flag);
     Cursor_gotorc(row,4);
     rc1=conversion_to_terminal(truncate_group(groupe->name,0),&ligne,0,(size_t)(-1));
     Screen_write_string(ligne);
     if (rc1==0) free(ligne); ligne=NULL;
     if (groupe->description==NULL) get_group_description(groupe);
     if (groupe->description) {
	 rc1=conversion_to_terminal(groupe->description,&ligne,0,(size_t)(-1));
	 Screen_write_string(ligne);
	 if (rc1==0) free(ligne);
     }
   } else {
     Cursor_gotorc(row,4);
     rc1=conversion_to_terminal(trad,&ligne,0,(size_t)(-1));
     Screen_write_string(ligne);
     if (rc1==0) free(ligne);
   }
   row++;
   return 0;
}

int fin_passage_not_menu (void **retour, int passage, int flags) {
    /* FIXME: français */
  Aff_fin(fl_static("-suite-"));
  Attend_touche(NULL);
  if (KeyBoard_Quit) return -1;
  return 0;
}

static Liste_Menu *menu_liste=NULL, *courant_liste=NULL, *start_liste=NULL;
static struct format_elem_menu fmt_grp_menu_e [] =
       { { 8, 8, 2, 2, 0, 7 }, { 0, 0, -1, 4, 0, 1 } };
struct format_menu fmt_grp_menu = { 2, &(fmt_grp_menu_e[0]) };
int ajoute_elem_menu (void *element, int passage) {
   Newsgroup_List *groupe=NULL;
   flrn_char *lelem[2];
   int free_flag=0;

   if (element==NULL) {
      if (passage==0) menu_liste=start_liste=courant_liste=NULL;
      /* Les autres réinitialisation sont traitées à un autre moment */
      return 0;
   }
   if (passage<2) groupe=(Newsgroup_List *)element; 
   if (passage<2) {
     if (groupe==Newsgroup_courant) {
	 lelem[0]=fl_static(">");
     } else {
	 lelem[0]=calcul_flag(groupe);
	 if (lelem[0]==NULL) lelem[0]=
	     ((NoArt_non_lus(groupe,0)>0) ? fl_static("+") :
	        fl_static(""));
	 else free_flag=1;
     }
     lelem[1]=groupe->name;
   } else {
     lelem[0]=fl_static("");
     lelem[1]=(flrn_char *)element;
   }
   courant_liste=ajoute_menu_ordre(&menu_liste,&fmt_grp_menu,lelem,
	   (passage < 2 ? element : safe_flstrdup(lelem[1])),1,0);
   if ((passage<2) && (groupe==Newsgroup_courant)) start_liste=courant_liste;
   if (start_liste==NULL) start_liste=menu_liste;
   if (free_flag) free(lelem[0]);
   return 0;
}

int fin_passage_menu (void **retour, int passage, int flags) {
   flrn_char *trad=NULL;
   char *msg;
   int rc1;
   
   if ((passage==0) && (flags>0)) return 0;
   num_help_line=12;
   if (passage<2) msg=chaine1; else msg=chaine2;
   rc1=conversion_from_utf8(msg,&trad,0,(size_t)(-1));

   *retour=Menu_simple(menu_liste, start_liste,
   		(passage<2 ? Ligne_carac_du_groupe : NULL),
		    (passage<2 ? chg_grp_in : chg_grp_not_in),
	        trad);
   if (rc1==0) free(trad);
   if ((*retour) && (passage==2)) {
      /* on va essayer de sauver le truc */
      flrn_char *nom=(flrn_char *)(*retour);
      Newsgroup_List *creation;
      creation=cherche_newsgroup(nom, 1, 0);
      *retour=creation;
      if ((creation) && 
           (Options.auto_kill) && (!in_main_list(creation->name))) {
        creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
        add_to_main_list(creation->name);
      }
   }
   Libere_menu(menu_liste);
   menu_liste=start_liste=courant_liste=NULL;
   if (*retour) return 1;
   return 0;
}


/* Affichage d'une liste de groupes */
/* flags=1 -> on affiche tout le .flnewsrc */
/* flags=2 -> on affiche tout */
/* On essaie une manip a coup de regexp... */
/* renvoie -1 en cas de pb avec les regexp, -2 pour une commande invalide  */
/* renvoie 1 : on a choisi un groupe */
/* TODO : remplacer l'affichage brut (sans menu) par un scrolling, et */
/* tout mettre dans group.c */
/* D'autre part, on supprime la recherche de la description quand on fait */
/* un menu : ça fout trop la merde (sans menu, c'est plus facile... */
int Liste_groupe (int flags, flrn_char *mat, Newsgroup_List **retour) {
   regex_t reg; /* En cas d'usage des regexp */
   flrn_char *mustmatch=NULL;
   int ret;

   int lg1_ajoute_elem(Newsgroup_List *param,
	   int flag_bis, int order, void **retour) {
       int ret;

       if (param==NULL) {
	    if (Options.use_menus) ret=fin_passage_menu(retour,
		                       (flag_bis & 32 ? 1 : 0),flags); else
		                   ret=fin_passage_not_menu(retour,
				       (flag_bis & 32 ? 1 : 0),flags);
	    return ret;
       }
       if (Options.use_menus) ret=ajoute_elem_menu((void *)param, 
	                            (flag_bis & 32 ? 1 : 0)); else
	                      ret=ajoute_elem_not_menu ((void *)param,
				      (flag_bis & 32 ? 1 : 0));
       return ret;
   }

   int lg1_order (flrn_char *unused, void *unused2) {
       return 0;
   }

   int lg2_ajoute_elem(char *param,
	   int flag_bis, int order, void **retour) {
       Newsgroup_List *parcours;
       char *ptr;
       int ret;
       int rc;
       flrn_char *trad;

       if (param==NULL) {
	    if (Options.use_menus) ret=fin_passage_menu(retour,
		                       2,flags); else
		                   ret=fin_passage_not_menu(retour,
				       2,flags);
	    return ret;
       }
       /* On teste si ce newsgroup existe déjà */
       ptr=strchr(param,' ');
       if (ptr) *ptr='\0';
       rc=conversion_from_utf8(param,&trad,0,(size_t)(-1));
       parcours=Newsgroup_deb;
       while (parcours) {
	   if (fl_strcmp(param, parcours->name)==0) break;
	   parcours=parcours->next;
       }
       if (parcours)
          if (Options.use_menus) ret=ajoute_elem_menu((void *)trad, 2); 
	  else ret=ajoute_elem_not_menu ((void *)trad, 2);
       else ret=0;
       if (rc==0) free (trad);
       return ret;
    }


   if (Options.use_regexp) {
       if (fl_regcomp(&reg,mat,REG_EXTENDED|REG_NOSUB))
          return -1;
       mustmatch = reg_string(mat,1);
   }
   if (Options.use_menus) ajoute_elem_menu (NULL,0); else
                          ajoute_elem_not_menu (NULL,0);
   ret = cherche_newsgroups_in_list (
	   (Options.use_regexp ? mustmatch : mat), reg,
	   8+(Options.use_regexp ? 2 : 0)+32,
	   &lg1_ajoute_elem,
           &lg1_order,
           (void **)retour);
   if (ret || (flags==0)) {
       if (Options.use_regexp) fl_regfree(&reg);
       return (ret>0 ? ret : 0);
   }
   if (Options.use_menus) ajoute_elem_menu (NULL,1); else
                          ajoute_elem_not_menu (NULL,1);
   ret = cherche_newsgroups_in_list (
	   (Options.use_regexp ? mustmatch : mat), reg,
	   48+(Options.use_regexp ? 2 : 0),
	   &lg1_ajoute_elem,
           &lg1_order,
           (void **)retour);
   if (ret || (flags==1)) {
       if (Options.use_regexp) regfree(&reg);
       return (ret>0 ? ret : 0);
   }
   if (Options.use_menus) ajoute_elem_menu (NULL,2); else
                          ajoute_elem_not_menu (NULL,2);
   ret = cherche_newsgroups_base (
	   (Options.use_regexp ? mustmatch : mat), reg,
	   (Options.use_regexp ? 2 : 0),
	   &lg2_ajoute_elem,
           &lg1_order,
           (void **)retour);
   if (Options.use_regexp) regfree(&reg);
   return (ret>0 ? ret : 0);
}
  
/* Affichage de l'arbre autour d'un message */
#define SYMB_ART_READ 'o'
#define SYMB_ART_UNK  '?'
#define SYMB_ART_UNR  'O'
#define SYMB_ART_COUR '@'
#define SYMB_SLASH    build_Colored_Ascii('/',FIELD_NORMAL)
#define SYMB_ANTISLASH build_Colored_Ascii('\\',FIELD_NORMAL)
#define SYMB_LOWER    build_Colored_Ascii('v',FIELD_NORMAL)
#define SYMB_HIGHER   build_Colored_Ascii('^',FIELD_NORMAL)
#define SYMB_BEFORE   build_Colored_Ascii('<',FIELD_NORMAL)
#define SYMB_AFTER    build_Colored_Ascii('>',FIELD_NORMAL)
#define SYMB_DASH    build_Colored_Ascii('-',FIELD_NORMAL)
#define SYMB_PIPE    build_Colored_Ascii('|',FIELD_NORMAL)
#define SYMB_SPACE	(Colored_Char_Type)(' ')
/* row, col : coordonnées du haut a gauche */
/* on essaie d'arranger to_left et to_right... height est la max... */
/* table est un pointeur vers un tableau de taille ((to_left+to_right)*2+3, */
/* height+1) ou plutôt l'inverse... il est déjà alloué... */
/* add_to_scroll ajoute l'arbre au scrolling, en supposant que le scrolling */
/* commence en premiere ligne...					*/
/* on renvoie la colonne atteinte...					*/
int Aff_arbre (int row, int col, Article_List *init,
    		int to_left, int to_right, int height,
		Colored_Char_Type **table, int add_to_scroll) {
  Article_List *parcours=init, *parcours2, *parcours3;
  int up,down, act_right, act_right_deb, act_right_temp, left, right, initcol;
  int row_deb=row; /* juste pour l'affichage */
  Colored_Char_Type symb_space=build_Colored_Ascii(' ',FIELD_NORMAL);

  /* on "nettoie" table */
  for (up=0;up<height+1;up++)
      for (left=0;left<(to_left+to_right)*2+3; left++) 
        table[up][left]=symb_space;
#define field_for_art(x) ((Est_proprietaire(x)>0) ? FIELD_AT_MINE : FIELD_AT_OTH)
#define char_for_art(x) (x->numero== -1 ? SYMB_ART_UNK : (x->flag & FLAG_READ ? SYMB_ART_READ : SYMB_ART_UNR))
  /* Enfin, on va commencer "modifier" to_left et to_right... */
  left=up=down=act_right=act_right_deb=0;
  while (parcours->pere) {
      left++;
      if (left>to_left) break;
      parcours=parcours->pere;
  }
  if (left>to_left) 
     /* On dessine le < */
     (table[0])[0]=SYMB_BEFORE;
  else if (parcours->pere && (parcours->pere->numero<0)) 
     (table[0])[0]=SYMB_DASH;
  /* L'ancêtre est celui dont on regarde pas les frères... */
  parcours=init;
  if (to_left>left) {
     to_right+=to_left-left;
     initcol=left*2+1;
  } else {
    initcol=to_left*2+1;
    left=to_left;
  }
  while (parcours->prem_fils) {
      act_right_deb++;
      if (act_right_deb>to_right) break;
      parcours=parcours->prem_fils;
  }
  if (act_right_deb>to_right) {
     (table[0])[(left+to_right)*2+2]=SYMB_AFTER;
     act_right_deb=to_right;
  }
  right=act_right_deb; /* right sera le max atteint à droite */
  while (act_right_deb>=-left) {
     int remonte, cl, rw, c, r;

     remonte=0;
     act_right=act_right_deb;
     cl=initcol+act_right*2;
     if (parcours==init) (table[0])[cl]= 
	 build_Colored_Ascii (SYMB_ART_COUR,field_for_art(parcours));
        else (table[0])[cl]=
	 build_Colored_Ascii (char_for_art(parcours),field_for_art(parcours));
     if (act_right!=-left) (table[0])[cl-1]=SYMB_DASH; 
     /* on commence l'infame parcours */
     parcours2=parcours->frere_prev;
     rw=height+1-up;
     if (parcours2)
     while (act_right>=act_right_deb) {
        if ((remonte!=1) && (up+((down==0) ? 1 : down)==height)) {
      /* cela ne peut pas arriver si on vient de descendre. C'est un frere */
      /* A moins que remonte==2 */
	   if (((table[rw])[cl-1]==symb_space) && (parcours2->frere_suiv))
	           (table[rw])[cl-1]=SYMB_SLASH;
	   (table[rw])[cl]=SYMB_HIGHER;
	   c=cl-2; r=rw+1;
	   if (((table[rw])[cl-1]==SYMB_ANTISLASH) || (parcours2->frere_suiv==NULL))
	           remonte=2; /* obligatoire */
	   else
	   if ((c<0) || ((table[rw])[c]!=symb_space)) remonte=1; 
	   else {
	     while ((r<height+1) && 
	         ((table[r])[c]==symb_space) &&
		 ((table[r])[c+1]!=SYMB_ANTISLASH)) 
	        (table[r++])[c]=SYMB_PIPE;
	     if ((r<height+1) && (c>=0) && ((table[r])[c+1]==SYMB_ANTISLASH)) remonte=2;
	         /* cas particulier : on veut afficher le "/^" pour le pere */
	     else remonte=1;
	   }
	   parcours2=parcours2->pere;
	   cl-=2;
	   act_right--;
	   continue;
	}
	if ((parcours2->prem_fils==NULL) || (act_right==to_right)) {
	   /* on ne veut pas aller plus a droite */
	   /* on fait donc l'affichage (remonte=0 toujours ici) */

	   up++; rw--;
	   (table[rw])[cl]=build_Colored_Ascii(char_for_art(parcours2),
					     field_for_art(parcours2));
	   if (parcours2->prem_fils) (table[rw])[cl+1]=SYMB_AFTER;
	   act_right_temp=act_right; parcours3=parcours2; c=cl;
	   /* et on remonte pour l'affichage */
           while (act_right_temp>act_right_deb) {
	      if (parcours3->pere->prem_fils==parcours3) {
		 parcours3=parcours3->pere;
		 act_right_temp--; c-=2;
		 (table[rw])[c]=build_Colored_Ascii(char_for_art(parcours3),
				field_for_art(parcours3));
		 (table[rw])[c+1]=SYMB_DASH;
		 r=rw+1;
		 if (parcours3->prem_fils->frere_suiv)
		 while ((r<height+1) && ((table[r])[c]==symb_space) && 
		     ((table[r])[c+1]!=SYMB_ANTISLASH))
		     (table[r++])[c]=SYMB_PIPE;
	      } else break;
	   }
	   if (parcours3->frere_suiv==NULL) 
	      (table[rw])[c-1]=SYMB_ANTISLASH; else
	   if (parcours3->frere_prev==NULL) {
	      (table[rw])[c-1]=SYMB_SLASH;
	      r=rw+1;
	      while ((r<height+1) && ((table[r])[c-2]==symb_space))
		(table[r++])[c-2]=SYMB_PIPE;
	   } else (table[rw])[c-1]=SYMB_DASH;
	} else if (!remonte) {
	   parcours2=parcours2->prem_fils;
	   while (parcours2->frere_suiv) parcours2=parcours2->frere_suiv;
	   act_right++; cl+=2;
	   if (right<act_right) right=act_right;
	   continue;
	}
	if (parcours2->frere_prev!=NULL) {
	   parcours2=parcours2->frere_prev;
	   remonte=0;
	   continue;
	}
	remonte=1;
	parcours2=parcours2->pere;
	act_right--;
	cl-=2;
     }
     remonte=0;
     act_right=act_right_deb;
     cl=initcol+act_right*2;
     /* on recommence l'infame parcours */
     parcours2=parcours->frere_suiv;
     rw=down;
     if (parcours2)
     while (act_right>=act_right_deb) {
        if ((remonte!=1) && (((up==0) ? 1 : up)+down==height)) {
      /* cela ne peut pas arriver si on vient de descendre. C'est un frere */
	   if (((table[rw])[cl-1]==symb_space) && (parcours2->frere_prev))
	            (table[rw])[cl-1]=SYMB_ANTISLASH;
	   (table[rw])[cl]=SYMB_LOWER;
	   c=cl-2; r=rw-1;
	   if (((table[rw])[cl-1]==SYMB_SLASH) || (parcours2->frere_prev==NULL))
	                         remonte=2; /* obligatoire */
	   else
	   if ((c<0) || ((table[rw])[c]!=symb_space)) remonte=1; 
	   else {
	     while ((r>0) && 
	         ((table[r])[c]==symb_space) && ((table[r])[c+1]!=SYMB_SLASH)) 
	        (table[r--])[c]=SYMB_PIPE;
	     if ((r>0) && ((table[r])[c+1]==SYMB_SLASH)) remonte=2;
	         /* cas particulier : on veut afficher le "\\v" pour le pere */
	     else remonte=1;
	   }
	   parcours2=parcours2->pere;
	   cl-=2;
	   act_right--;
	   continue;
	}
	if ((parcours2->prem_fils==NULL) || (act_right==to_right)) {
	   /* on ne veut pas aller plus a droite */
	   /* on fait donc l'affichage (remonte=0 toujours ici) */

	   down++; rw++;
	   (table[rw])[cl]=build_Colored_Ascii(char_for_art(parcours2),
					       field_for_art(parcours2));
	   if (parcours2->prem_fils) (table[rw])[cl+1]=SYMB_AFTER;
	   act_right_temp=act_right; parcours3=parcours2; c=cl;
	   /* et on remonte pour l'affichage */
           while (act_right_temp>act_right_deb) {
	      if (parcours3->pere->prem_fils==parcours3) {
		 parcours3=parcours3->pere;
		 act_right_temp--; c-=2;
		 (table[rw])[c]=build_Colored_Ascii(char_for_art(parcours3),
						field_for_art(parcours3));
		 (table[rw])[c+1]=SYMB_DASH;
		 r=rw-1;
		 if (parcours3->prem_fils->frere_prev)
		 while ((r>0) && ((table[r])[c]==symb_space) &&
			 ((table[r])[c+1]!=SYMB_SLASH))
		     (table[r--])[c]=SYMB_PIPE;
	      } else break;
	   }
	   if (parcours3->frere_prev==NULL) 
	      (table[rw])[c-1]=SYMB_SLASH; else
	   if (parcours3->frere_suiv==NULL) {
	      (table[rw])[c-1]=SYMB_ANTISLASH;
	      r=rw-1;
	      while ((r>0) && ((table[r])[c-2]==symb_space))
		(table[r--])[c-2]=SYMB_PIPE;
	   } else (table[rw])[c-1]=SYMB_DASH;
	} else if (!remonte) {
	   parcours2=parcours2->prem_fils;
	   while (parcours2->frere_prev) parcours2=parcours2->frere_prev;
	   act_right++; cl+=2;
	   if (right<act_right) right=act_right;
	   continue;
	}
	if (parcours2->frere_suiv!=NULL) {
	   parcours2=parcours2->frere_suiv;
	   remonte=0;
	   continue;
	}
	remonte=1;
	parcours2=parcours2->pere;
	act_right--;
	cl-=2;
     }
     parcours=parcours->pere;
     act_right_deb--;
  }
  /* table est correctement remplie, right est le maximum atteint a droite,
   * left celui a gauche, up en haut et down en bas... Tout le reste n'est
   * qu'affichage, mais c'est quand même mieux si c'est centré... :-) */ 
  if (up<height/2) row=row+(height/2-up>height-up-down ? height-up-down :
  				height/2-up);
  if (left<to_left) col=col+2*(to_left-left<to_right-right ? 
  			to_left-left : to_right-right);
		/* un peu bizarre, mais left+to_right=to_left+to_right_ini */
  				
  for (;up>0;up--) {
    Cursor_gotorc(row++,col);
    if (row<Screen_Rows-1)
	Screen_write_color_chars(table[height+1-up],initcol+right*2+2);
    if (add_to_scroll) {
       if (Rajoute_color_Line(table[height+1-up],NULL,
		      row-1-row_deb,initcol+right*2+2,0,col)==NULL) {
          Ajoute_color_Line(NULL,NULL,0,0,Screen_Cols,Screen_Cols,0);
	  Rajoute_color_Line(table[height+1-up],NULL,-1,
		  initcol+right*2+2,0,col);
       }
    }
  }
  for (up=0;up<=down;up++) {
    Cursor_gotorc(row++,col);
    if (row<Screen_Rows-1)
       Screen_write_color_chars(table[up],initcol+right*2+2);
    if (add_to_scroll) {
       if (Rajoute_color_Line(table[up],NULL,
		     row-1-row_deb,initcol+right*2+2,0,col)==NULL) {
          Ajoute_color_Line(NULL,NULL,0,0,Screen_Cols,Screen_Cols,0);
	  Rajoute_color_Line(table[up],NULL,-1,initcol+right*2+2,0,col);
       }
    }
  }
  return (row-1);
}
     
/* ajout d'un bout de chaîne */
/* ça devrait être assez générique pour servir dans toutes
 * les occasions */
struct construct_fill {
    int row;
    int colcur;
    int colwr;
    int maxcol;
    int finished;
    int afficher;
    Colored_Char_Type *blreste;
    Colored_Char_Type *reste;
    size_t rstlen;
    size_t blrstlen;
    flrn_char *blflreste;
    flrn_char *flreste;
};

const flrn_char *sepspn = fl_static(" \t\n");
static void add_strings_bit (flrn_char *str, size_t len, int field,
	struct construct_fill *cf,
	int flag, int add_to_scroll, int new_maxcol(int) ) {
    size_t lwd,rlen=len;
    char *trad;
    int rc;

    Screen_set_color(field);
    if (cf->finished) return;
    while ((rlen>0) || (str==NULL)) {
       if (str) lwd=fl_strcspn(str,sepspn); else lwd=0;
       if (lwd>rlen) lwd=rlen;
       if (lwd==0) {
	      /* un blanc */
	      /* on affiche d'abord ce qui reste */
	      if ((cf->blreste) && (cf->reste)) { /* col==0 a priori */
		  if (cf->afficher) 
		      Screen_write_color_chars(cf->blreste,cf->blrstlen);
		  if (add_to_scroll) {
		      if (cf->colcur==0) 
		        Ajoute_color_Line(cf->blreste,cf->blflreste,
				cf->blrstlen,
				(cf->maxcol*2>cf->blrstlen ?
				    cf->maxcol*2 : cf->blrstlen),
				fl_strlen(cf->blflreste),
				(cf->maxcol*2>fl_strlen(cf->blflreste) ?
				  cf->maxcol*2 : fl_strlen(cf->blflreste)), 0);
		      else
			Rajoute_color_Line(cf->blreste,cf->blflreste,-1,
				cf->blrstlen, fl_strlen(cf->blflreste),-1);
		  }
		  free(cf->blreste); free(cf->blflreste);
		  cf->blreste=NULL; cf->blflreste=NULL;
		  cf->blrstlen=0;
		  cf->colcur=1; /* juste un cours instant ,pour dire
				   de ne pas sauter de ligne */
	      }
	      if (cf->reste) {
		  if (cf->afficher)
		      Screen_write_color_chars(cf->reste,cf->rstlen);
		  if (add_to_scroll) {
		      if (cf->colcur==0) 
		        Ajoute_color_Line(cf->reste,cf->flreste,
				cf->rstlen,
				(cf->maxcol*2>cf->rstlen ?
				    cf->maxcol*2 : cf->rstlen),
				fl_strlen(cf->flreste),
				(cf->maxcol*2>fl_strlen(cf->flreste) ?
				  cf->maxcol*2 : fl_strlen(cf->flreste)), 0);
		      else
			Rajoute_color_Line(cf->reste,cf->flreste,-1,cf->rstlen,
				fl_strlen(cf->flreste),-1);
		  }
		  free(cf->reste); free(cf->flreste);
		  cf->reste=NULL; cf->flreste=NULL;
		  cf->rstlen=0;
		  cf->colcur=cf->colwr;
	      }
	      if (str==NULL) {
		  cf->finished=1;
		  return;
	      }
	      lwd=fl_strspn(str,sepspn); 
	      if (lwd>rlen) lwd=rlen;
	      if (lwd==0) break;
	      if (flag) {
		  if (cf->blreste==NULL) {
		    cf->blreste=cree_chaine_mono(" ",field,1,&cf->blrstlen);
		    cf->blflreste=safe_flstrdup(fl_static(" "));
		    cf->colwr++;
		  }
	      } else {
		  int count=0;
		  size_t p,lstcr=0,vlstcr=lwd+1;
		  while (vlstcr>0) {
		      vlstcr--;
		      if (str[vlstcr-1]==fl_static('\n')) break;
		  }
		  for (p=0;p<vlstcr;p++) {
		      if (str[p]==fl_static('\n')) {
			  if (count) {
			      str[p]='\0';
			      if (add_to_scroll) 
				  Ajoute_line(str+lstcr+1,0,field);
			      cf->row++;
			      cf->maxcol=new_maxcol(cf->row);
			      cf->colcur=cf->colwr=0;
			      Cursor_gotorc(cf->row,0);
			      str[p]='\n';
			  } else {
			      if (cf->blreste) {
				  if (cf->colcur==0) {
				     if (add_to_scroll)
		                       Ajoute_color_Line(cf->blreste,
					       cf->blflreste,
				         cf->blrstlen,0, 
					 fl_strlen(cf->blflreste), 0, 0);
				     if (cf->afficher)
					 Screen_write_color_chars(cf->blreste,
					     cf->blrstlen);
				  }
				  free(cf->blreste);
				  free(cf->blflreste);
				  cf->blreste=NULL; cf->blflreste=NULL;
				  cf->blrstlen=0;
			      }
			      cf->row++;
			      cf->maxcol=new_maxcol(cf->row);
			      cf->colcur=cf->colwr=0;
			      Cursor_gotorc(cf->row,0);
			  }
			  lstcr=p;
			  count++;
		      } else 
			  if ((count) && (cf->afficher))
			      Screen_write_char((char)str[p]);
		  }
		  if (vlstcr!=lwd) { 
		      /* the last one is vlstcr */
		      if (cf->blreste==NULL) {
		        cf->blflreste=safe_calloc((lwd+1-vlstcr),sizeof(flrn_char));
		        fl_strncpy(cf->blflreste,str+vlstcr,(lwd-vlstcr));
		        rc=conversion_to_terminal(cf->blflreste,&trad,0,
				(size_t)(-1));
		        cf->blrstlen=strlen(trad);
			cf->colwr+=str_estime_width(trad,cf->colwr,(size_t)(-1));
		        cf->blreste=cree_chaine_mono(trad,field,cf->blrstlen,&cf->blrstlen);
		      } else {
			size_t bla=fl_strlen(cf->blflreste);
			Colored_Char_Type *blreste2;
			cf->blflreste=safe_realloc(cf->blflreste,
				(bla+(lwd-vlstcr+1))*
				sizeof(flrn_char));
			fl_strncpy(cf->blflreste+bla,str+vlstcr,
				(lwd-vlstcr));
		        rc=conversion_to_terminal(cf->blflreste+bla,&trad,0,
				(size_t)(-1));
		        bla=strlen(trad);
			cf->colwr+=str_estime_width(trad,cf->colwr,(size_t)(-1));
		        blreste2=cree_chaine_mono(trad,field,bla,
				&bla);
			cf->blreste=safe_realloc(cf->blreste,(bla+cf->blrstlen)*
				sizeof(Colored_Char_Type));
			memcpy(cf->blreste+cf->blrstlen,blreste2,
				bla*sizeof(Colored_Char_Type));
			free(blreste2);
			cf->blrstlen+=bla;
		      }
		      if (rc==0) free(trad);
		  }
	      }
	      str+=lwd;
	      rlen-=lwd;
	  } else { /* un non-blanc */
	      flrn_char *flreste2=NULL;
	      Colored_Char_Type *reste2=NULL;
	      size_t rstlen2=0;
	      flrn_char *ptr;
	      int colwr2=cf->colwr;

	      flreste2=safe_calloc(lwd+1,sizeof(flrn_char));
	      fl_strncpy(flreste2,str,lwd);
	      rc=conversion_to_terminal(flreste2,&trad,0,(size_t)(-1));
	      colwr2+=str_estime_width(trad,cf->colwr,(size_t)(-1));
	      ptr=flreste2;
	      while (colwr2>cf->maxcol) {
		  if (flag) {
		      if (cf->blreste) {
			  if (cf->afficher)
			      Screen_write_color_chars(cf->blreste,cf->blrstlen);
			  if (add_to_scroll)
			      Rajoute_color_Line(cf->blreste,cf->blflreste,
				      -1,cf->blrstlen,fl_strlen(cf->blflreste),-1);
			  cf->colcur++;
			  free(cf->blreste); free(cf->blflreste);
			  cf->blrstlen=0;
			  cf->blreste=NULL; cf->blflreste=NULL;
		      }
		      if (cf->reste) {
			  if (cf->afficher)
			      Screen_write_color_chars(cf->reste,cf->rstlen);
			  if (add_to_scroll) 
			      Ajoute_color_Line(cf->reste,cf->flreste,
				      cf->rstlen,0,
				      fl_strlen(cf->flreste),0,0);
			  cf->colcur=cf->colwr;
			  free(cf->reste); free(cf->flreste); cf->reste=NULL;
			  cf->flreste=NULL; cf->rstlen=0;
		      }
		      {
		        size_t bla;
		        flrn_char sve;
		        if (rc==0) free(trad);
		        bla=to_make_width_convert(ptr,
				cf->maxcol,&(cf->colcur),0);
		        sve=ptr[bla];
		        ptr[bla]=fl_static('\0');
		        if (cf->afficher) {
		            rc=conversion_to_terminal(ptr,&trad,0,(size_t)(-1));
			    Screen_write_string(trad);
		            if (rc==0) free(trad);
			}
		        if (add_to_scroll) {
			    if (cf->colcur==0) Ajoute_line(ptr,0,field);
			    else {
			        size_t bla3;
			      /* cf->reste=NULL */
			        cf->reste=cree_chaine_mono(trad,field,
				        strlen(trad),&bla3);
			        Rajoute_color_Line(cf->reste,
				        ptr,-1,bla3,bla,-1);
			        free(cf->reste); cf->reste=NULL;
			    }
			}
			free(flreste2);
			Screen_set_color(FIELD_HEADER);
			Cursor_gotorc(cf->row,cf->maxcol);
			if (cf->afficher) Screen_write_string("[...]");
			if (add_to_scroll) {
			  size_t dummy;
			  cf->flreste=fl_static("[...]");
	                  cf->reste=cree_chaine_mono("[...]",FIELD_HEADER,
				  (size_t)(-1), &dummy);
			  Rajoute_color_Line(cf->reste,cf->flreste,-1,dummy,5,cf->maxcol);
			  free(cf->reste);
			  cf->reste=NULL;
			}
		        cf->finished=1;
		        return;
		      }
		  }
		  if ((cf->colcur!=0) || (cf->blreste)) {
		      if (cf->colcur==0) {
			  if (add_to_scroll)
			      Ajoute_color_Line(cf->blreste,cf->blflreste,
				      cf->blrstlen,0,
				      fl_strlen(cf->blflreste), 0, 0);
			  if (cf->afficher)
			      Screen_write_color_chars(cf->blreste,cf->blrstlen);
		      }
		      cf->row++;
	  	      cf->maxcol=new_maxcol(cf->row);
		      cf->colcur=0;
		      if (cf->reste==NULL) colwr2=cf->colwr=0; else {
			  int rc2;
			  char *trad2;
			  rc2=conversion_to_terminal(cf->flreste,&trad2,
				  0,(size_t)(-1));
			  colwr2=cf->colwr=str_estime_width(trad2,0,(size_t)(-1));
			  if (rc2==0) free(trad2);
		      }
		      Cursor_gotorc(cf->row,0);
		      if (cf->blreste) {
			  free(cf->blreste); free(cf->blflreste); cf->blrstlen=0;
			  cf->blreste=NULL; cf->blflreste=NULL;
		      }
		      colwr2+=str_estime_width(trad,cf->colwr,(size_t)(-1));
		      continue;
		  }
		  {
		      size_t bla;
		      flrn_char sve;
		      if (rc==0) free(trad);
		      if (cf->reste) {
			  if (cf->afficher)
			      Screen_write_color_chars(cf->reste,cf->rstlen);
			  if (add_to_scroll) 
			      Ajoute_color_Line(cf->reste,cf->flreste,
				      cf->rstlen,0,
				      fl_strlen(cf->flreste),0,0);
			  cf->colcur=cf->colwr;
			  free(cf->reste); free(cf->flreste); cf->reste=NULL;
			  cf->flreste=NULL; cf->rstlen=0;
		      }
		      bla=to_make_width_convert(ptr,cf->maxcol,&(cf->colcur),0);
		      sve=ptr[bla];
		      ptr[bla]=fl_static('\0');
		      if (cf->afficher) {
		         rc=conversion_to_terminal(ptr,&trad,0,(size_t)(-1));
		         Screen_write_string(trad);
		         if (rc==0) free(trad);
		      }
		      if (add_to_scroll) {
			  if (cf->colcur==0) Ajoute_line(ptr,0,field);
			  else {
			      size_t bla3;
			      /* cf->reste=NULL */
			      cf->reste=cree_chaine_mono(trad,field,
				      strlen(trad),&bla3);
			      Rajoute_color_Line(cf->reste,
				      ptr,-1,bla3,bla,-1);
			      free(cf->reste); cf->reste=NULL;
			  }
		      }
		      cf->row++; cf->colcur=cf->colwr=0;
	  	      cf->maxcol=new_maxcol(cf->row);
		      Cursor_gotorc(cf->row,0);
		      ptr[bla]=sve;
		      ptr+=bla;
		      rc=conversion_to_terminal(ptr,&trad,0,
			      (size_t)(-1));
		      colwr2=str_estime_width(trad,0,(size_t)(-1));
		  }
	      }
	      cf->colwr=colwr2;
	      rstlen2=strlen(trad);
	      reste2=cree_chaine_mono(trad,field,rstlen2,&rstlen2);
	      if (rc==0) free(trad);
	      if (ptr!=flreste2) { /* décalé _après_ avoir utilisé trad
				      parce que trad pouvait être égal à
				      ptr */
		  flrn_char *flreste3;
		  flreste3=safe_flstrdup(ptr);
		  free(flreste2); flreste2=flreste3;
	      }
	      if (cf->reste) {
		  cf->flreste=safe_realloc(cf->flreste,
			  (fl_strlen(cf->flreste)+fl_strlen(flreste2)+1)*
			  sizeof(flrn_char));
		  fl_strcat(cf->flreste,flreste2);
		  free(flreste2);
		  cf->reste=safe_realloc(cf->reste,
			  (cf->rstlen+rstlen2)*sizeof(Colored_Char_Type));
		  memcpy(cf->reste+
			  cf->rstlen,reste2,rstlen2*sizeof(Colored_Char_Type));
		  cf->rstlen+=rstlen2;
		  free(reste2);
	      } else {
		  cf->reste=reste2;
		  cf->flreste=flreste2;
		  cf->rstlen=rstlen2;
	      }
	      str+=lwd;
	      rlen-=lwd;
	  }
    }
}
   

/* Affichage d'un header, juste appelé par Aff_headers */
/* flag=1 : juste une ligne... flag=2 : cas particulier reponse_a */
/* with_arbre : on va afficher l'arbre, réserver de la place...   */
/*    (0 : rien    1 : petites fleches     2 : arbre complet  */
/* add_to_scroll : on ajoute au scrolling ce qu'on écrit....  */
/* (en gros c'est a peu près comme flag sauf cas flag=2 :-(   */
/* note : str ne doit pas être une chaîne constante et est modifiée */
int Aff_header (int flag, int with_arbre, int row, int col, 
	flrn_char *str, int add_to_scroll) {
   struct construct_fill cf; 

   int mxcl (int n) {
       int decalage;
       decalage=flag ? 5 : 0;  
       if (flag==2) decalage+=8;
       if ((with_arbre==1) && (n<4+Options.skip_line)) decalage+=5; 
       if ((with_arbre==2) && (n<8+Options.skip_line)) decalage+=12; 
       cf.afficher=(n<Screen_Rows-2);
       return Screen_Cols-1-decalage;
   }

   
   void add_line_aff_header (flrn_char *chaine, size_t len, int field) {
      add_strings_bit (chaine, len, field, &cf,
	      flag, add_to_scroll, mxcl);
   }
   
   memset(&cf,0,sizeof(struct construct_fill));
   cf.row=row;
   cf.colcur=cf.colwr=col;
   cf.maxcol=mxcl(row);

   Cursor_gotorc(row,col);
   create_Color_line(add_line_aff_header,FIELD_HEADER,
	   str, fl_strlen(str),FIELD_HEADER);
   add_line_aff_header(NULL,0,0);
   return (cf.row+1 /* (flag>0) */); 
      /* Si flag!=0, on n'est pas arrive en fin de ligne */
      /* FIXME  : hum, à voir, mettre éventuellement row+1 */
#if 0
   while (buf && *buf) {
      Cursor_gotorc(row,col);
      buf2=strchr(buf,'\n');
      if (buf2==NULL) buf2=strchr(buf,'\0'); 
      decalage=flag ? 5 : 0;
      if (flag==2) decalage+=8;
      if ((with_arbre==1) && (row<4+Options.skip_line)) decalage+=5; 
      if ((with_arbre==2) && (row<8+Options.skip_line)) decalage+=12; 
      							/* pour l'arbre */
      if (buf2-buf>Screen_Cols-col-decalage) {
	 Screen_write_color_chars(ptr, Screen_Cols-col-decalage);
	 if (add_to_scroll) {
	    if (col==0) Ajoute_color_Line(ptr, Screen_Cols-decalage,
	    					Screen_Cols);
	    else Rajoute_color_Line(ptr,-1,Screen_Cols-col-decalage,col);
	 }
	 if (flag) {
	    buf="[...]";
	    ptr=cree_chaine_mono(buf,FIELD_HEADER,-1);
	    if (add_to_scroll) Rajoute_color_Line(ptr,-1,5,
	    					Screen_Cols-decalage);
	    Screen_write_color_chars(ptr,5);
	    free(ptr);
	    return (row+1);
	 }
	 buf+=Screen_Cols-col-decalage;
	 ptr+=Screen_Cols-col-decalage;
	 col=0; row++;
      } else {
	 Screen_write_color_chars(ptr, buf2-buf);
	 if (add_to_scroll) {
	    if (col==0) Ajoute_color_Line(ptr, buf2-buf, Screen_Cols);
	    else Rajoute_color_Line(ptr,-1,buf2-buf,col);
	 }
	 if (flag) col+=buf2-buf+1;
	 else { col=0; row++; }
	 if (*buf2) { ptr=to_aff+(buf2-str)+1; buf=buf2+1; }
	   else buf=NULL;
      }
  }
  return (row+(flag>0)); 
      /* Si flag!=0, on n'est pas arrive en fin de ligne */
#endif
}

/* Utilise Options.user_flags pour faire des flags immonde */
static flrn_char *Recupere_user_flags (Article_List *article) {
   flrn_char *str, *buf, *deb;
   int size=0;
   size_t len,sl;
   flrn_filter *filt;
   string_list_type *parcours=Options.user_flags;

   while (parcours) {
     size++;
     parcours=parcours->next;
   }
   if (size==0) return NULL;
   len=size*6+1;
   str=safe_malloc(len*sizeof(flrn_char));
   *str=fl_static('\0');
   size=0;
   filt=new_filter();
   parcours=Options.user_flags;
   while (parcours) {
     memset(filt,0,sizeof(flrn_filter));
     filt->action.flag=FLAG_SUMMARY;
     buf=parcours->str;
     parcours=parcours->next;
     sl=next_flch(buf,0);
     if ((int)sl<=0) continue;
     deb=buf;
     buf+=sl;
     while ((*buf) && (fl_isblank(*buf))) buf++;
     if (parse_filter_flags(buf,filt)) 
       parse_filter(buf,filt);
     if (!check_article(article,filt,1)) {
	 if (size+sl>=len) break;
	 strncat(str,deb,sl);
	 size+=sl;
	 str[size]=fl_static('\0');
     }
   }
   free_filter(filt);
   if (size==0) free(str);
   return (size==0 ? NULL : str);
}


/* flag==1 : pas de scrolling possible */
int Aff_place_article (int flag){
   int with_arbre;
   int row=0;

   with_arbre=1+Options.small_tree;
   Screen_set_color(FIELD_NORMAL);
   if (with_arbre==1) {
     /* On affiche les petites fleches */
     /* provisoirement, on les ajoute pas au scrolling (FIXME) */
     Cursor_gotorc(2+Options.skip_line, Screen_Cols-4);
     Screen_write_char('+');
     if (Article_courant->frere_prev) {
        Cursor_gotorc(1+Options.skip_line, Screen_Cols-4);
        Screen_write_char('^');
        if (Article_courant->frere_prev->prem_fils)
          Screen_write_char('\'');
     }
     if (Article_courant->frere_suiv) {
        Cursor_gotorc(3+Options.skip_line, Screen_Cols-4);
        Screen_write_char('v');
        if (Article_courant->frere_suiv->prem_fils)
          Screen_write_char(',');
     }
     if (Article_courant->pere) {
        Cursor_gotorc(2+Options.skip_line, Screen_Cols-5);
        Screen_write_char('<');
     }
     if (Article_courant->prem_fils) {
        int numfils=1;
        Article_List *parcours=Article_courant->prem_fils;
        while ((parcours->frere_prev)) parcours=parcours->frere_prev;
        while ((parcours=parcours->frere_suiv)) numfils++;
        Cursor_gotorc(2+Options.skip_line, Screen_Cols-3);
        if (numfils > 9) Screen_write_char('*');
        else if (numfils > 1) Screen_write_char('0'+numfils);
         Screen_write_char('>');
        if (Article_courant->prem_fils->prem_fils)
          Screen_write_char('>');
     }
     row=4+Options.skip_line;
   } else if (with_arbre==2) {
     row=Aff_arbre(1+Options.skip_line,Screen_Cols-12,Article_courant,2,2,6,
	 table_petit_arbre,!flag);
   }
   return row;
}


/* Affichage des headers... Le parametre flag vaut 1 si c'est la suite d'un */
/* message 								    */
/* Un truc bien est maintenant d'ajouter éventuellement au scrolling les    */
/* headers dans le cas ou flag vaut 0. On gardera ensuite ledit scrolling   */
/* selon les options obtenues...					    */
int Aff_headers (int flag) {
   int index=0, row, row2, col, i, j, i0, szl;
   char buf[15];
   Header_List *tmp=Last_head_cmd.headers;
   flrn_char *une_ligne=NULL, *flags_header;
   Colored_Char_Type *une_belle_ligne;
   int flags[NB_KNOWN_HEADERS];
   int with_arbre;

/*   with_arbre=1+(Options.small_tree && (!flag)); */
   with_arbre=1+Options.small_tree;
   row=1+Options.skip_line;
   /* On commence pas mettre le flag pour tous les headers référencés */
   for (j=0;j<NB_KNOWN_HEADERS;j++) flags[j]=-1;
   while (tmp) { tmp->num_af=-1; tmp=tmp->next; }
   while ((i=Options.header_list[index++])!=-1) {
      if ((i>NB_KNOWN_HEADERS) || (i<-MAX_HEADER_LIST-1)) continue;
      if ((i>=0) && (i<NB_KNOWN_HEADERS)) flags[i]=index; else
      if (i==NB_KNOWN_HEADERS) {
	tmp=Last_head_cmd.headers;
	while (tmp) { if (tmp->num_af==-1) tmp->num_af=index;
	  		tmp=tmp->next; }
	for (j=0;j<NB_KNOWN_HEADERS;j++) if (flags[j]<0) flags[j]=index;
      } else { /* i<-1 */ /* On suppose que ce header ne peut etre double */
	tmp=Last_head_cmd.headers;
	while (tmp) {
	  if (fl_strncasecmp(tmp->header_head,unknown_Headers[-2-i].header,
		unknown_Headers[-2-i].header_len)==0) break;
	  tmp=tmp->next;
	}
	if (tmp) tmp->num_af=index;
      }
   }
   index=0;
   while ((i=Options.hidden_header_list[index++])!=-1) {
      if ((i>NB_KNOWN_HEADERS) || (i<-MAX_HEADER_LIST-1)) continue;
      if ((i>=0) && (i<NB_KNOWN_HEADERS)) flags[i]=-1; else
      if (i==NB_KNOWN_HEADERS) {
	/* depuis quand hidden_header peut contenir others ??? */
      } else { /* i<-1 */ /* On suppose que ce header ne peut etre double */
	tmp=Last_head_cmd.headers;
	while (tmp) {
	  if (fl_strncasecmp(tmp->header_head,unknown_Headers[-2-i].header,
		unknown_Headers[-2-i].header_len)==0) break;
	  tmp=tmp->next;
	}
	if (tmp) tmp->num_af=-1;
      }
   }

   index=0;
   while ((i=Options.header_list[index++])!=-1)
   {
     if ((i>NB_KNOWN_HEADERS) || (i<-MAX_HEADER_LIST-1)) continue;
     if (flag) {
       for (j=0; Options.weak_header_list[j]!=-1; j++)
         if (Options.weak_header_list[j]==i) break;
       if (Options.weak_header_list[j]!=-1) continue;
     }
     if (i==NB_KNOWN_HEADERS) i0=0; else i0=i;
     for (;i0<=i;i0++) { 
       if ((i==NB_KNOWN_HEADERS) && (i0!=NB_KNOWN_HEADERS) &&
	   (flags[i0]!=index)) continue;
       if (une_ligne) {
	 free(une_ligne);
	 une_ligne=NULL;
       }
       switch (i0) {
         case REFERENCES_HEADER+NB_DECODED_HEADERS: /* traitement special */
           if (Article_courant->headers->reponse_a) {
	     une_ligne=safe_malloc((22+2*fl_strlen(Article_courant->headers->reponse_a))*sizeof(flrn_char));
	     /* FIXME : français */
             fl_strcpy(une_ligne,fl_static("Reponse a: "));
	     if (!Options.parse_from) 
	     	fl_strcat(une_ligne,Article_courant->headers->reponse_a);
	     else ajoute_parsed_from(une_ligne,Article_courant->headers->reponse_a,NULL);
	     row=Aff_header(2, with_arbre, row, 0, une_ligne, !flag);
   	     Screen_set_color(FIELD_HEADER);
	     if (Article_courant->parent>0) {
	        sprintf(buf," [%d]", Article_courant->parent);
	        Screen_write_string(buf);
		if (!flag) {
		  size_t bla;
		  une_belle_ligne=cree_chaine_mono(buf,FIELD_HEADER,
			  (size_t)(-1),&bla);
		  Rajoute_color_Line(une_belle_ligne,
			  fl_static_rev(buf),-1,fl_strlen(buf),
			  bla,-1);
		  free(une_belle_ligne);
		}
	     }
	    /* On ne garde qu'une ligne dans tous les cas */
           } else if (Article_courant->parent!=0) {
   	      Screen_set_color(FIELD_HEADER);
              Cursor_gotorc(row,0);
	      /* FIXME : français */
	      une_ligne=safe_flstrdup(fl_static("Reponse à un message non disponible."));
	      row=Aff_header(1, with_arbre, row, 0, une_ligne, !flag);
           } 
         break;
         case DATE_HEADER:
           Cursor_gotorc(row,0);
	   une_ligne=safe_calloc(60,sizeof(flrn_char));
	   /* FIXME : français */
           fl_strcpy(une_ligne,fl_static("Date: "));
	   if (Article_courant->headers->date_gmt) {
	     fl_strncat(une_ligne,
		     fl_static_tran(ctime(&Article_courant->headers->date_gmt))
		     ,53);
	   } else
	     if (Article_courant->headers->k_headers[DATE_HEADER])
	        fl_strncat(une_ligne,
	              Article_courant->headers->k_headers[DATE_HEADER],53);
	     else
	        fl_strcat(une_ligne,fl_static("???"));
	   row=Aff_header(1, with_arbre, row, 0, une_ligne, !flag);
           break;
         case FROM_HEADER:
	   szl=20+2*fl_strlen(Article_courant->headers->k_headers[FROM_HEADER]);
	   if (Article_courant->headers->k_headers[SENDER_HEADER])
	       szl+=fl_strlen(Article_courant->headers->
		       k_headers[SENDER_HEADER]);
	   une_ligne=safe_calloc(szl,sizeof(flrn_char));
	   /* FIXME : français */ 
           fl_strcpy(une_ligne,"Auteur: ");
	   if (!Options.parse_from) 
	     	fl_strcat(une_ligne,
			Article_courant->headers->k_headers[FROM_HEADER]);
	   else ajoute_parsed_from(une_ligne,Article_courant->headers->k_headers[FROM_HEADER],Article_courant->headers->k_headers[SENDER_HEADER]);
	   row=Aff_header(flag,with_arbre, row,0,une_ligne,!flag);
         break;
         case SUBJECT_HEADER:
   	   Screen_set_color(FIELD_HEADER);
           Cursor_gotorc(row,0);
	   col=0;
	   /* FIXME : français */
	   if (flag) { Screen_write_string(fl_static("(suite) ")); col+=8; }
	      /* pas besoin d'allouer : add_to_scroll =0  dans Aff_header */
	   flags_header=Recupere_user_flags(Article_courant);
	   une_ligne=safe_malloc((9+fl_strlen(Article_courant->headers->k_headers[SUBJECT_HEADER])+(flags_header ? fl_strlen(flags_header) : 0))*sizeof(flrn_char));
	   if (flags_header) fl_strcpy(une_ligne,flags_header); else 
	       *une_ligne=fl_static('\0');
	   /* FIXME: français */
           fl_strcat(une_ligne,"Sujet: ");
	   fl_strcat(une_ligne,Article_courant->headers->k_headers[SUBJECT_HEADER]);
	   row=Aff_header(flag,with_arbre, row,col,une_ligne,!flag);
	   if (flags_header) free(flags_header);
         break;
         default: if (i0==NB_KNOWN_HEADERS) {
		    tmp=Last_head_cmd.headers;
		    while (tmp) {
		      if (tmp->num_af==index) {
			une_ligne=safe_calloc(fl_strlen(tmp->header_head)+
			   fl_strlen(tmp->header_body)+3,sizeof(flrn_char));
			fl_strcpy(une_ligne,tmp->header_head);
			fl_strcat(une_ligne,fl_static(" "));
			fl_strcat(une_ligne,tmp->header_body);
	   		row=Aff_header(flag,with_arbre, row,0,une_ligne,
					!flag);
			free(une_ligne); une_ligne=NULL;
		      }
		      tmp=tmp->next;
		    }
		    break;
		  } 
	 	  if ((i0>=0) &&
		      (((i0<NB_DECODED_HEADERS) &&	  
		       (Article_courant->headers->k_headers[i0]==NULL)) ||
		       ((i0>=NB_DECODED_HEADERS) &&
			 (Article_courant->headers->k_raw_headers
			[i0-NB_DECODED_HEADERS]==NULL)))) 
		    break; else
		  if (i0<0) {
		    tmp=Last_head_cmd.headers;
		    while (tmp) {
		      if (tmp->num_af==index) break;
		      tmp=tmp->next;
		    }
		    if (!tmp) break;
		    une_ligne=safe_calloc(fl_strlen(tmp->header_head)+
		       fl_strlen(tmp->header_body)+3,sizeof(flrn_char));
		    fl_strcpy(une_ligne,tmp->header_head);
		    fl_strcat(une_ligne,fl_static(" "));
		    fl_strcat(une_ligne,tmp->header_body);
		  } else {
		    if (i0<NB_DECODED_HEADERS)
	              une_ligne=safe_calloc(Headers[i0].header_len+3+
			    fl_strlen(Article_courant->headers->k_headers[i0]),
			    sizeof(flrn_char));
		    else une_ligne=safe_calloc(Headers[i0].header_len+3+
			    strlen(Article_courant->headers->
				k_raw_headers[i0-NB_DECODED_HEADERS]),
			    sizeof(flrn_char));
           	    fl_strcpy(une_ligne,fl_static_tran(Headers[i0].header));
		    fl_strcat(une_ligne," ");
		    if (i0<NB_DECODED_HEADERS)
	              fl_strcat(une_ligne,
			    Article_courant->headers->k_headers[i0]);
                      else
	              fl_strcat(une_ligne,
			    fl_static_rev(
				Article_courant->headers->k_raw_headers
				[i0-NB_DECODED_HEADERS]));
	          }
		 row=Aff_header(flag,with_arbre,row,0,une_ligne,!flag);
         break;
       }
     }
   }
   if (une_ligne) free(une_ligne);
   row2=Aff_place_article(flag);
   if (row2>row) row=row2;
   return row;
}


/* Formate et affiche une ligne d'un article. Prend la ligne courante 	*/
/* et renvoie la nouvelle ligne courante.				*/
/* On ajoute aussi la ligne pour un futur scrolling...			*/
/* Renvoie 0 en fin de message, négatif si un scroll doit etre fait     */
/* on renvoie alors - la ligne ou nous sommes arrivés...		*/
/* Si from_file=1, on ne formate pas...					*/
int Ajoute_aff_formated_line (int act_row, int read_line, int from_file) {
   int res, percent;
   char *buf, *buf2;
   flrn_char buf3[15];
   int last_color;
   flrn_char *trad;
   int rc;
   struct construct_fill cf;

   int chgrow (int n) {
      if ((n>Screen_Rows-2) && (en_deca)) {  /* Fin de l'ecran */

	 if ((!from_file) && (Article_courant->headers->nb_lines!=0)) {
	    percent=((read_line-saved_space-1)*100)/(Article_courant->headers->nb_lines+1);
            fl_snprintf(buf3,8,"(%d%%)",percent);
	 } else buf3[0]='\0';
	 fl_strcat(buf3,"-More-");
	 num_help_line=(from_file ? 4 : 2);
         Aff_help_line(Screen_Rows-1);
	 Aff_fin(buf3);
	 Screen_refresh(); /* CAS particulier */
	 cf.afficher=0;
	 en_deca=0;
      }
      return(Screen_Cols-1);
   }

   void add_line_aff_line (flrn_char *chaine, size_t len, int field) {
       add_strings_bit (chaine, len, field, &cf,
	       0,1,chgrow);
   }
   
   if ((!from_file) && (tcp_line_read[0]=='.')) {
      if (tcp_line_read[1]=='.') buf=tcp_line_read+1; else 
	return (en_deca ? 0 : -act_row+saved_space);
   } else buf=tcp_line_read; 
   if (*buf==(from_file ? '\n' : '\r')) {
     saved_field=FIELD_NORMAL; /* On enleve le style "signature" */
     saved_space++;
     return (act_row+1); /* On sort tout de suite, mais en augmentant le  */
     	                 /* nombre de ligne (pour éviter le fameux (100%) */
   }
   if (saved_field!=FIELD_SIG) {
     if (buf[0]=='>') saved_field=FIELD_QUOTED; else saved_field=FIELD_NORMAL;
   }
   if (strcmp(buf,"-- ")==0) saved_field=FIELD_SIG;
   if (from_file) saved_field=FIELD_FILE;
   last_color=saved_field;
   memset(&cf,0,sizeof(struct construct_fill));
   cf.row=act_row;
   cf.colcur=cf.colwr=0;
   cf.maxcol=chgrow(act_row);
   cf.afficher=(act_row<Screen_Rows-1);
   Cursor_gotorc(act_row,0);

   while (saved_space>0) {
      Ajoute_line(fl_static(""),0,FIELD_NORMAL);
      saved_space--;
   }
   while (1) {
     buf2=strchr(buf,(from_file ? '\n' : '\r'));
     if ((!buf2) && (!from_file)) buf2=strchr(buf,'\n');
     if (buf2) *buf2='\0';
     if (from_file) rc=conversion_from_file(buf,&trad,0,(size_t)(-1));
       else rc=conversion_from_message(buf,&trad,0,(size_t)(-1));
     create_Color_line(add_line_aff_line,saved_field,trad,fl_strlen(trad),
	     saved_field);
     if (rc==0) free(trad);
     if (buf2) break;
     if (from_file) break;
     res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
     if (res<2) break;
     buf=tcp_line_read;
   }
   add_line_aff_line(NULL,0,0);


#if 0 

   while (1) {
#ifdef WITH_CHARACTER_SETS
      if ((must_decode) && (!deja_decode)) {
        char *buf2;
        int ret;
        ret=Decode_ligne_message(buf,&buf2);
        if (ret==0) {
           buf=buf2;
	   mustfree=buf2;
        }
      }
      deja_decode=0;
#endif
      if ((act_row>Screen_Rows-2) && (en_deca)) {  /* Fin de l'ecran */

	 if ((!from_file) && (Article_courant->headers->nb_lines!=0)) {
	    percent=((read_line-saved_space-1)*100)/(Article_courant->headers->nb_lines+1);
            sprintf(buf3,"(%d%%)",percent);
	 } else buf3[0]='\0';
	 strcat(buf3,"-More-");
	 num_help_line=(from_file ? 4 : 2);
         Aff_help_line(Screen_Rows-1);
	 Aff_fin(buf3);
	 Screen_refresh(); /* CAS particulier */
	 en_deca=0;
      }
      while (saved_space>0) { /* Alors une_ligne[0]='\0' */
	Ajoute_form_Ligne(une_ligne, saved_field);
        saved_space--;
      }
      if (buf[0]=='\0') {
	if (act_row<Screen_Rows-1) {
	  Cursor_gotorc(act_row,0);
	}
	last_color=Aff_color_line((act_row<Screen_Rows-1),une_belle_ligne,
	    &length,saved_field, une_ligne, Screen_Cols, bol,last_color);
	bol=0;
	Ajoute_color_Line(une_belle_ligne,length,0);
	free(une_ligne); free(une_belle_ligne);
#ifdef WITH_CHARACTER_SETS
        if (mustfree) free(mustfree);
#endif
	return (act_row+1); /* Fin de ligne */
      }
      len_to_write=to_make_len(buf,Screen_Cols,tmp_col);
      if (len_to_write==0) { 
	if (act_row<Screen_Rows-1) {
	  Cursor_gotorc(act_row,0);
	}
	last_color=Aff_color_line((act_row<Screen_Rows-1),une_belle_ligne,
	    &length,saved_field, une_ligne, Screen_Cols, bol,last_color);
	bol=0;
	Ajoute_color_Line(une_belle_ligne,length,0);
	une_ligne[0]='\0';
	act_row++; tmp_col=0; 
	continue; 
      } /* on a deja elimine le cas fin de ligne */

      strncat(une_ligne,buf,len_to_write);
      if ((!from_file) &&
       ((!buf2) || (buf2-tcp_line_read==MAX_READ_SIZE-1)))    /* ouille ! */
      {
         if (strlen(buf)<=len_to_write) {
             tmp_col=str_estime_len(buf,tmp_col,-1);
             res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
#ifdef WITH_CHARACTER_SETS
             if (mustfree) free(mustfree);
	     mustfree=NULL;
#endif
             if (res<1) {free(une_ligne); free(une_belle_ligne);
	       return act_row; }
             buf=tcp_line_read;
           /* le cas ou buf2 est défini correspond a un \r en fin de lecture */
           /* dans ce cas, on est sur de lire simplement \n */
             if (!buf2) buf2=strchr(buf,'\r'); else buf2=tcp_line_read;
             if (buf2) *buf2='\0';
             continue;
          } else { 
	    buf+=len_to_write;
	    tmp_col=str_estime_len(buf,tmp_col,len_to_write);
	    if (str_estime_len(buf,0,-1)<Screen_Cols-1) {
	       strcpy(tcp_line_read,buf); /* A priori, cols < 1024 */
	       buf=tcp_line_read;
               res=read_server(buf+strlen(buf), 1, MAX_READ_SIZE-strlen(buf)-1);
           /* le cas ou buf2 est défini correspond a un \r en fin de lecture */
           /* dans ce cas, on est sur de lire simplement \n */
               if (!buf2) buf2=strchr(buf,'\r'); else buf2=tcp_line_read;
               if (buf2) *buf2='\0';
#ifdef WITH_CHARACTER_SETS
               if (mustfree) free(mustfree);
               mustfree=NULL;
#endif
	       continue;
	    } /* sinon, on se contente de changer de ligne */
	 }
      }
      else 
      if (strlen(buf)<=len_to_write) { /* fini */
	  if (act_row<Screen_Rows-1) {
	    Cursor_gotorc(act_row,0);
	  }
	  last_color=Aff_color_line((act_row<Screen_Rows-1),une_belle_ligne,
	      &length,saved_field, une_ligne, Screen_Cols, bol,last_color);
	  bol=0;
	  Ajoute_color_Line(une_belle_ligne,length,0);
	  free(une_ligne); free(une_belle_ligne);
#ifdef WITH_CHARACTER_SETS
          if (mustfree) free(mustfree);
#endif
          return (act_row+1);
      }
      else buf+=len_to_write;
      if (act_row<Screen_Rows-1) {
        Cursor_gotorc(act_row,0);
      }
      last_color=Aff_color_line((act_row<Screen_Rows-1),une_belle_ligne,
	  &length,saved_field, une_ligne, Screen_Cols, bol,last_color);
      bol=0;
      Ajoute_color_Line(une_belle_ligne,length,0);
      une_ligne[0]='\0';
      tmp_col=0;
      act_row++;
   }
#ifdef WITH_CHARACTER_SETS
   if (mustfree) free(mustfree);
#endif
#endif
   return (act_row+1);
}      


/* Gestion du scrolling... */
/* -1 : ^C    -2 : cmd et pas fin    0 : fin    1 : cmd et fin */
int Gere_Scroll_Message (int row_act, int row_deb, 
				int scroll_headers, int to_build) {
  int act_row, ret;
  int num_elem=-row_act-row_deb, percent;
  flrn_char buf3[15];
  struct key_entry key;

  key.entry=ENTRY_ERROR_KEY;

  if (to_build) {
      /* FIXME: français */
     Aff_fin(fl_static("Patientez..."));
     Screen_refresh();
     to_build=cree_liste_suite(0);
     percent=((Screen_Rows-row_deb-2)*100)/(num_elem+1);
     fl_snprintf(buf3,8,"(%d%%)",percent);
     fl_strcat(buf3,fl_static("-More-"));
     Aff_fin(buf3);
  }
  Attend_touche(&key);
  if (KeyBoard_Quit) return -1;
  Cursor_gotorc(1,0);
  Screen_set_color(FIELD_NORMAL);
  Screen_erase_eos();
  Aff_help_line(Screen_Rows-1);
  if (scroll_headers==0) act_row=Aff_headers(1)+1; else
      act_row=1+Options.skip_line;
  Init_Scroll_window(num_elem,act_row,Screen_Rows-act_row-1);
  ret=Page_message(num_elem, 1, &key, act_row, row_deb, NULL, 
	  /* FIXME: français */
	  fl_static("A vous : "),
  		    (to_build ? cree_liste_suite : NULL),NULL);
  if (ret==-1) return -1;
  if (ret==-2) return 0;
  if (ret==MAX_FL_KEY) return 1;
  return -2;
}

/* Affichage du nom du newsgroup */
void Aff_newsgroup_name(int erase_scr) {
   flrn_char *tmp_name,*buf;
   int rc,ucol=0;
   flrn_char *flag_aff=0;
   char *trad;
#ifdef USE_SLANG_LANGUAGE
   int used_slang=0;
   
#endif
   if (name_fin_col-name_news_col>0) {
#ifdef USE_SLANG_LANGUAGE
     used_slang = try_hook_newsgroup_string ("Newsgroup_title_string",
	                                 Newsgroup_courant, &buf);
#endif
     Screen_set_color(FIELD_STATUS);
     Cursor_gotorc(0,name_news_col); /* après try_hook... */
#ifdef USE_SLANG_LANGUAGE
     if (used_slang==0) {
#endif
       if (Newsgroup_courant) {
         if (!(Newsgroup_courant->flags & GROUP_MODE_TESTED))
            test_readonly(Newsgroup_courant);
         flag_aff=calcul_flag(Newsgroup_courant);
	 if (flag_aff) {
	    rc=conversion_to_terminal(flag_aff,
		    &trad,0,(size_t)(-1));
	    Screen_write_char('[');
	    Screen_write_string(trad);
	    Screen_write_char(']');
	    ucol=2+str_estime_width(trad,name_news_col,(size_t)(-1));
	    if (rc==0) free(trad);
	    free(flag_aff);
	 }
         tmp_name=truncate_group(Newsgroup_courant->name,0);
	 trad=safe_calloc(fl_strlen(tmp_name)*6+1,sizeof(flrn_char));
	 format_flstring_from_right(trad,tmp_name,
		 name_fin_col-name_news_col-ucol,
		 fl_strlen(tmp_name)*6,0);
	 rc=0;
       } else {
	 trad=NULL;
	 rc=1;
       }
#ifdef USE_SLANG_LANGUAGE
    } else {
	trad=safe_calloc(fl_strlen(buf)*6+1,sizeof(flrn_char));
	format_flstring_from_right(trad,buf,
		name_fin_col-name_news_col-ucol,
		fl_strlen(buf)*6,0);
	free(buf);
	rc=0;
    }
#endif
       if (trad) {
	   ucol+=name_news_col+str_estime_width(trad,name_news_col,
	       (size_t)(-1));
	   Screen_write_string(trad);
           if (rc==0) free(trad);
       } else ucol+=name_news_col;
       Screen_write_nstring("",name_fin_col-ucol+1);
   }
   if (erase_scr) {
     Screen_set_color(FIELD_NORMAL);
     Cursor_gotorc(1,0);
     Screen_erase_eos();
     Screen_refresh(); 
          /* Cas particulier : pour le temps que ça prend parfois */
   }
}
     

/* Affichage du newsgroup courant */
void Aff_newsgroup_courant() {
   char buf[10];
   int ret=1;

   if (Newsgroup_courant) {
     if ((Newsgroup_courant->max>99999) && (num_col_num<6)) ret=Size_Window(0,8);
     if ((Newsgroup_courant->max<100000) && (num_col_num>5)) ret=Size_Window(0,5);
     if (ret==0) Size_Window(0,5); /* retour arriere, et tant pis */
   }
   Screen_set_color(FIELD_STATUS);
   Cursor_gotorc(0,num_art_col);
   Screen_write_nstring("         ",num_col_num);
   Screen_write_char('/');
   if (Newsgroup_courant) {
     sprintf(buf,(num_col_num==5 ? "%5d " : "%8d "), Newsgroup_courant->max);
     Screen_write_string(buf); 
   } else
   Screen_write_nstring("         ",num_col_num+1);
   Cursor_gotorc(0,num_rest_col);
   Screen_write_string("     )");
   Screen_set_color(FIELD_NORMAL);
   Screen_refresh(); /* Cas particulier aussi */
}


/* Affichage du reste à lire... */
void Aff_not_read_newsgroup_courant() {
   char buf[10];

   Screen_set_color(FIELD_STATUS);
   Cursor_gotorc(0,num_rest_col);
   if (Newsgroup_courant->not_read>=0) {
     sprintf(buf,  "%5d)", Newsgroup_courant->not_read); 
     Screen_write_string(buf);
   } else
     Screen_write_string("  ?  )");
   Screen_set_color(FIELD_NORMAL);
}

/* TODO TODO TODO TODO : changer ça */
/* FIXME : lignes d'aide */
void Aff_help_line(int ligne) {
/*   int rc;
   char *trad; */
   if (Options.help_line==0) return;
   Cursor_gotorc(ligne,0);
   Screen_set_color(FIELD_STATUS);
   Screen_erase_eol();
   Screen_write_string((char *)(Help_Lines[num_help_line]));
   Screen_set_color(FIELD_NORMAL);
}

static int base_Aff_article(int to_build) {
   char buf[10];

  /* barre */
   Screen_set_color(FIELD_STATUS);
   Cursor_gotorc(0,num_art_col);
   if (Article_courant->numero>=0) {
     sprintf(buf,(num_col_num==8 ? "%8d/" : "%5d/"), Article_courant->numero);
     Screen_write_string(buf); 
   } else 
     Screen_write_string(num_col_num==8 ? "   ?    /" : "  ?  /");
   sprintf(buf, (num_col_num==8 ? "%8d " : "%5d "), Newsgroup_courant->max); 
   Screen_write_string(buf);
   Aff_not_read_newsgroup_courant();

   Screen_set_color(FIELD_NORMAL);
   if (Article_courant->numero==-10) {
       /* FIXME : françaus */
        Aff_error(fl_static("Pas d'article disponible."));
        return -1;
   }

   Cursor_gotorc(1,0);
   Screen_erase_eos(); /* on veut effacer aussi les lignes du haut */
   return 0;
}
  


/* On affiche un grand thread, plus un résumé de l'article courant */
int Aff_grand_thread(int to_build) {
   Colored_Char_Type **grande_table;
   int i;

   if (base_Aff_article(to_build)==-1) return 0;
   grande_table=safe_malloc((Screen_Rows-4)*sizeof(Colored_Char_Type *));
   for (i=0;i<Screen_Rows-4;i++)
     grande_table[i]=safe_malloc(Screen_Cols*sizeof(Colored_Char_Type));
   Aff_arbre(2,0,Article_courant,Screen_Cols/4-1,Screen_Cols/4-1,
        Screen_Rows-5,grande_table,0);
   raw_Aff_summary_line(Article_courant, Screen_Rows-2, NULL, 0);
   for (i=0;i<Screen_Rows-4;i++)
     free(grande_table[i]);
   free(grande_table);
   return 0;
}

/* Affichage de l'article courant */
/* renvoie 1 en cas de commande, 0 sinon */
/* Note : les appels se font avec le numero de l'article */
/* comme il semble que ça marche mieux...		 */
int Aff_article_courant(int to_build) {
   int res, actual_row, read_line=1;
   int first_line, scroll_headers=0;
   char *num;
   Header_List *tmp;
   
   if (debug) fprintf(stderr, "Appel a Aff_article_courant\n");
   if (base_Aff_article(to_build)==-1) return 0;
   Cursor_gotorc(1+Options.skip_line,0);

  /* Headers  -- on veut all_headers <=> un appel a cree_headers */
   if ((!Article_courant->headers) ||
       (Article_courant->headers->all_headers==0) || (Last_head_cmd.Article_vu!=Article_courant)) {
        cree_header(Article_courant,1,1,0);
        if ((!Article_courant->headers) || (Article_courant->headers->k_headers[FROM_HEADER]==NULL) || (Article_courant->headers->k_headers[SUBJECT_HEADER]==NULL)) {
	    Aff_place_article(1);
	    Screen_set_color(FIELD_ERROR);
	    Cursor_gotorc(row_erreur,col_erreur); 
	    if (Article_courant->headers==NULL) 
		/* FIXME : français */
	       Screen_write_string("Article indisponible."); else
	       Screen_write_string("Article incompréhensible.");
	    Screen_set_color(FIELD_NORMAL);
	    if (!(Article_courant->flag & FLAG_READ) && (Article_courant->numero>0) &&
	           (Newsgroup_courant->not_read>0)) {
		      Newsgroup_courant->not_read--;
		      Article_courant->thread->non_lu--;
	    }
	    if (Article_courant->flag & FLAG_IMPORTANT) {
              Newsgroup_courant->important--;
	      Article_courant->flag &= ~FLAG_IMPORTANT;
            }
	    Article_courant->flag |= FLAG_READ;
	    Article_courant->flag |= FLAG_KILLED;
	    return -1;
	}
        /* On suppose ca provisoire */
	/* le bon check, c'est pere != NULL ou parent !=0 ?
	 * esperons que ca coincide */
   } else if ((Article_courant->pere) && 
              (Article_courant->headers->reponse_a_checked==0)) 
	   ajoute_reponse_a(Article_courant);
   actual_row=Aff_headers(0);
   {
      tmp=Last_head_cmd.headers;
      while (tmp) {
         if (fl_strncasecmp(tmp->header_head,
		     fl_static("content-type:"),13)==0) {
	     parse_ContentType_header(tmp->header_body);
	     break;
	 }
	 tmp=tmp->next;
      }
      if (tmp==NULL) parse_ContentType_header(NULL);
   }
   if ((actual_row>Screen_Rows-2) || (Options.headers_scroll)) {
      Ajoute_color_Line(NULL,NULL,0,0,0,0,0);
      /* on saute une ligne dans le vide */
      scroll_headers=1;
   } else free_text_scroll();
   
  /* Messages */
   saved_field=FIELD_NORMAL;
   saved_space=0;
   en_deca=1;
   first_line=++actual_row;  /* on saute une ligne */
   
   num=safe_malloc(280*sizeof(char));
   if (Article_courant->numero!=-1) 
     sprintf(num, "%d", Article_courant->numero); else
     strcpy(num, Article_courant->msgid);
   res=write_command(CMD_BODY, 1, &num);
   free(num);
   if (res<0) { if (debug) fprintf(stderr, "erreur en ecriture\n"); 
     free_text_scroll();
     return -1; }
   res=return_code();
   if (res==423) { /* Bad article number ? */
      num=safe_malloc(280*sizeof(char));
      strcpy(num, Article_courant->msgid);
      res=write_command(CMD_BODY, 1, &num);
      free(num);
      if (res<0) { if (debug) fprintf(stderr, "erreur en ecriture\n");
        free_text_scroll();
      return -1; }
      res=return_code();
   }
   if (res<0 || res>400) { 
     if (debug)
       fprintf(stderr, "Pas d'article : %d\n", Article_courant->numero); 
     Aff_place_article(1);
     Screen_set_color(FIELD_ERROR);
     Cursor_gotorc(row_erreur,col_erreur); 
     /* FIXME : français */
     Screen_write_string("Article indisponible.");
     Screen_set_color(FIELD_NORMAL);
     if (!(Article_courant->flag & FLAG_READ) && (Article_courant->numero>0) &&
           (Newsgroup_courant->not_read>0)) {
	      Newsgroup_courant->not_read--;
	      Article_courant->thread->non_lu--;
     }
     if (Article_courant->flag & FLAG_IMPORTANT) {
        Newsgroup_courant->important--;
        Article_courant->flag &= ~FLAG_IMPORTANT;
     }
     Article_courant->flag |= FLAG_READ;
     Article_courant->flag |= FLAG_KILLED;
     free_text_scroll(); 
     return -1;
   }
   do {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<1) { 
	if (debug) fprintf(stderr, "Erreur en lecture...\n"); 
	free_text_scroll(); return -1; }
      actual_row=Ajoute_aff_formated_line(actual_row, read_line,0);
      read_line++;
   } while (actual_row>0);
   if (actual_row<0)  /* On entame un scrolling */
	  res=Gere_Scroll_Message(actual_row,
	    (scroll_headers ? 1+Options.skip_line : first_line), scroll_headers, to_build);
   else res=0;
   free_text_scroll();
   if (res>=0)  
     article_read(Article_courant); /*Article_courant->flag |= FLAG_READ;*/
   Aff_not_read_newsgroup_courant();
   if (debug) fprintf(stderr,"Fin d'affichage \n");
   return ((res==1) || (res==-2));
}

	   
    /* Affichage d'un fichier. */
int Aff_file (FILE *file, char *exit_chars, flrn_char *end_mesg,
	struct key_entry *keyr) {
   int row=1,read_line=1,ret=0;
   struct key_entry key;
    
   key.entry=ENTRY_ERROR_KEY;
   saved_field=FIELD_NORMAL;
   saved_space=0;
   en_deca=1;
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   num_help_line=4;
   Aff_help_line(Screen_Rows-1);
   while (fgets(tcp_line_read, MAX_READ_SIZE-1, file)) {
     row=Ajoute_aff_formated_line(row, read_line, 1);
     read_line++;
   }
   if (row>Screen_Rows-1) {  /* On entame un scrolling */
      Attend_touche(&key);
      if (KeyBoard_Quit) return 0;
      Init_Scroll_window(row-1, 1, Screen_Rows-2);
      ret=Page_message(row-1, 0, &key, 1, 1, exit_chars, end_mesg, NULL, keyr); 
      if (ret<0) ret=0;
   }
   free_text_scroll();
   return ret;
}
