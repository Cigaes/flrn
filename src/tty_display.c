/* flrn v 0.1                                                           */
/*             tty_display.c          25/11/97                          */
/*                                                                      */
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
#include "flrn_command.h"
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

/* place des objets de la barre */
int name_news_col, num_art_col, num_rest_col, num_col_num, name_fin_col;
/* place des messages d'erreur */
int row_erreur, col_erreur;
int error_fin_displayed=0;
/* pour l'arbre */
unsigned char **table_petit_arbre;

/* Objet courant dans l'affichage de l'article */
/* Espace actuellement conservé dans l'affichage de l'article */
int saved_field, saved_space, en_deca;

/* (re-)Determinations des caractéristiques de la fenêtre */
/* si col_num!=0, c'est la taille à réserver pour les numéros */
static int Size_Window(int flag, int col_num) {
   int aff_help=0, aff_mess=1;
   char *name_program2;

   name_news_col=strlen(name_program)+1;
   if (name_news_col>9) name_news_col=9;
   if (col_num==0) col_num=num_col_num; else num_col_num=col_num;
   num_rest_col=Screen_Cols-2-5;
   num_art_col=num_rest_col-10-2*col_num;

   if (num_art_col<name_news_col+5) {
      if (debug || flag) fprintf(stderr, "Nombre de colonnes insuffisant.\n");
      return 0;
   } else if (num_art_col<name_news_col+20) aff_mess=0; else
     if (num_art_col>name_news_col+28) aff_help=1;
   if (aff_help) name_news_col+=9;
   if (aff_mess==0) num_art_col+=7;
   if (Screen_Rows<3+2*Options.skip_line) {
      row_erreur=(Screen_Rows ? 1 : 0);
      if (debug || flag) fprintf(stderr, "Nombre de lignes insuffisant.\n");
      return 0;
   } else 
      row_erreur=Screen_Rows/2;
   col_erreur=3;

   Screen_set_color(FIELD_STATUS);
   Screen_erase_eol(); /* Juste pour tout remplir */
   name_program2=safe_strdup(name_program);
   name_program2[0]=(char) toupper((int) name_program2[0]);
   if (strlen(name_program2)>8) name_program2[8]='\0';
   Screen_write_string(name_program2);
   free(name_program2);
   if (aff_help) Screen_write_string(" (?:Aide)");
   Screen_write_string(": ");
   name_fin_col=num_art_col-1;
   Cursor_gotorc (0,num_art_col);
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
   
   Screen_get_size();
   Screen_init_smg ();
   if (Size_Window(0,0)) {
     if (Newsgroup_courant) { Aff_newsgroup_name(0); Aff_newsgroup_courant(); }
     Aff_error ("Changement de taille de la fenêtre");
   } else Aff_error ("Fenetre trop petite !!!");
   Screen_refresh();
   sigwinch_received=1;
   /*SL*/signal(SIGWINCH, sig_winch);

   return;
}
  

/* Initialise l'écran, crée la barre */
int Init_screen(int stupid_term) {
   int res;

   table_petit_arbre=safe_malloc(7*sizeof(char *));
   for (res=0;res<7;res++)
     table_petit_arbre[res]=safe_malloc(11);
   Get_terminfo ();
   if (stupid_term) Set_term_cannot_scroll(1);
   res=Screen_init_smg ();
  
#if 0  /* Les valeurs de retour de la fonction ont changés */
   if (res==0) {
      fprintf(stderr, "Mémoire insuffisante pour initialiser l'écran.\n");
      return 0;
   }
#endif
   Init_couleurs();

   /*SL*/signal(SIGWINCH, sig_winch);
    
   return Size_Window(1,5);
}

void Reset_screen() {
  int res;
  for (res=0;res<7;res++)
    free(table_petit_arbre[res]);
  free(table_petit_arbre);
  Screen_reset();
}



/* Affiche un message en bas de l'ecran.				*/
/* renvoie la colonne d'arrivée */
int Aff_fin(const char *str) {
   int col=0;
   Cursor_gotorc(Screen_Rows-1,0);
#ifdef CHECK_MAIL
   Screen_set_color(FIELD_NORMAL);
   if (Options.check_mail && newmail(mailbox)) {
      Screen_write_string("(Mail)");
      col=6;
   }
#endif
   Screen_set_color(FIELD_AFF_FIN);
   Screen_write_string((char *)str);
   Screen_set_color(FIELD_NORMAL);
   Screen_erase_eol();
   Cursor_gotorc(Screen_Rows-1, col+strlen(str)); /* En changeant l'objet */
    				                  /* on change la pos du  */
   					          /* curseur.		  */
   return (col+strlen(str));
}

/* Affiche un message sur l'écran (pour l'instant d'une seule ligne max). */
int Aff_error(const char *str) {
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   Screen_set_color(FIELD_ERROR);
   Cursor_gotorc(row_erreur,col_erreur); 
   Screen_write_string((char *)str);
   Screen_set_color(FIELD_NORMAL);
   return 0;
}

/* Affiche un message d'erreur en fin... */
int Aff_error_fin(const char *str, int s_beep) {
   int col=0;
   Cursor_gotorc(Screen_Rows-1,0);
#ifdef CHECK_MAIL
   Screen_set_color(FIELD_NORMAL);
   if (Options.check_mail && newmail(mailbox)) {
      Screen_write_string("(Mail)");
      col=6;
   }
#endif
   Screen_set_color(FIELD_ERROR);
   Screen_write_string((char *)str);
   if (s_beep) Screen_beep();
/* Contrairement à Aff_fin, on ne cherche pas à afficher le curseur */
/* après le message...						    */
   Screen_set_color(FIELD_NORMAL);
   Screen_erase_eol();
   Cursor_gotorc(Screen_Rows-1, Screen_Cols-1); 
   Screen_refresh();
   if (Options.short_errors) sleep(1); else error_fin_displayed=1;
   return 0;
}

/* Affichage de la ligne résumé d'un article */
/* NE FAIT PAS LE REFRESH, heureusement ! */
static void raw_Aff_summary_line(Article_List *article, int row,
    char *previous_subject,
    int level) {
  char *buf= safe_malloc(Screen_Cols);
  Prepare_summary_line(article,previous_subject, level, buf, Screen_Cols,0);
  Cursor_gotorc(row,0);
  Aff_color_line(1,NULL,NULL,FIELD_SUMMARY,buf,Screen_Cols,1,FIELD_SUMMARY);
  free(buf);
}

int Aff_summary_line(Article_List *art, int *row,
    char *previous_subject, int level) {
  int key;
  if (*row==0) *row=1+Options.skip_line;
  if (*row>=Screen_Rows-1-Options.skip_line) {
    Aff_fin("-suite-");
    key=Attend_touche();
    if (KeyBoard_Quit) return 1;
    Cursor_gotorc(1,0);
    Screen_erase_eos();
    *row=1+Options.skip_line;
    previous_subject=NULL;
  } 
  raw_Aff_summary_line(art,(*row)++,previous_subject,level);
  return 0;
}

/* Calcul le flag d'un groupe */
static char calcul_flag (Newsgroup_List *groupe) {
   int num_flag;

   num_flag=(groupe->flags & GROUP_READONLY_FLAG ? 4 : 0)+
   		(groupe->flags & GROUP_UNSUBSCRIBED ? 2 : 0)+
		(groupe->flags & GROUP_IN_MAIN_LIST_FLAG ? 0 : 1);
   if (Options.flags_group && (strlen(Options.flags_group)>num_flag) 
       	     && (Options.flags_group[num_flag]!=' ')) 
	     	 return Options.flags_group[num_flag];
   return 0;
}


/* Fonctions utilisées pour Liste_groupe... On devrait tout déplacer */
/* dans group.c à mon avis. Tout ça n'a plus rien à faire là.        */
/* Quoique... meme ici il y a de l'affichage...			     */
int chg_grp_in(void *value, char **nom, int i, char *name, int len, int key) {
   Newsgroup_List *choisi=(Newsgroup_List *)value;
   char flag;
   int zapped=(choisi==Newsgroup_courant);
      /* On ne peut pas zapper le groupe courant */

   *name='\0'; /* ça on en est sur */
   if (key==13) {
     Cursor_gotorc(Screen_Rows-2,0);
     Screen_erase_eol();
     if (choisi->flags & GROUP_UNSUBSCRIBED) 
        Screen_write_string(" désabonné     ");
     else Screen_write_string(" abonné   ");
     if (choisi->flags & GROUP_IN_MAIN_LIST_FLAG)
        Screen_write_string(" mode censuré  ");
     else Screen_write_string(" mode tout  ");
     if (!zapped) Screen_write_string("  (zap possible) ");
     key=Attend_touche();
   }
   if (KeyBoard_Quit) key=0;
   switch (key<MAX_FL_KEY ? Flcmd_rev[CONTEXT_COMMAND][key] : FLCMD_UNDEF) {
     case FLCMD_ABON : choisi->flags&=~GROUP_UNSUBSCRIBED;
                break;
     case FLCMD_UNSU : choisi->flags|=GROUP_UNSUBSCRIBED;
                break;
     case FLCMD_GOTO : 
     case FLCMD_GGTO : return 1;
     case FLCMD_REMOVE_KILL : choisi->flags&=~GROUP_IN_MAIN_LIST_FLAG;
         	remove_from_main_list(choisi->name);
                break;
     case FLCMD_ADD_KILL : choisi->flags|=GROUP_IN_MAIN_LIST_FLAG;
     		add_to_main_list(choisi->name);
                break;
     case FLCMD_ZAP : zap_group_non_courant(choisi);
     		if (**nom=='+') **nom=' ';
		zapped=1;
       		break;
   }
   flag=calcul_flag(choisi);
   if (choisi!=Newsgroup_courant) **nom=(flag!=0 ? flag : ' ');
   return 0;
}

/* Cette fonction DOIT renvoyer 0 si on veut éviter une horreur de typage */
int chg_grp_not_in(void *value, char **nom, int i, char *name, int len, int key) {
   char *nom_groupe=(char *)value;
   Newsgroup_List *creation;

   if (**nom=='A') return 0; 
   	/* Tant pis... on ne peut se désabonner aussi sec */
   creation=cherche_newsgroup(nom_groupe, 1, 0);
   if (!creation) return 0;
   Cursor_gotorc(Screen_Rows-2,0);
   Screen_erase_eol();
   Screen_write_string(" S'(a)bonner à ce groupe  ? ");
   if (key==13) key=Attend_touche();
   key=tolower(key);
   if (KeyBoard_Quit) key=0;
   if ((key=='o') || (key=='a')) {
      creation->flags&=~GROUP_UNSUBSCRIBED;
      if ((Options.auto_kill) && (!in_main_list(creation->name))) {
        creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
 	add_to_main_list(creation->name);
      }
      **nom='A';
      strncpy(name," Vous êtes abonnés à ce groupe.",len);
      name[len-1]='\0';
   } else *name=0;
   return 0;
}
      


/* Affichage d'une liste de groupes */
/* flags=1 -> on affiche tout le .flnewsrc */
/* flags=2 -> on affiche tout */
/* On essaie une manip a coup de regexp... */
/* renvoie -1 en cas de pb avec les regexp */
/* renvoie 1 : on a choisi un groupe */
/* TODO : remplacer l'affichage brut (sans menu) par un scrolling, et */
/* tout mettre dans group.c */
/* D'autre part, on supprime la recherche de la description quand on fait */
/* un menu : ça fout trop la merde (sans menu, c'est plus facile... */
int Liste_groupe (int flags, char *mat, Newsgroup_List **retour) {
   int row=0;
   int res, code, key;
   char *buf[2], *ptr, *nom=NULL, une_ligne[80], flag;
   int passage=-1;
   Newsgroup_List *parcours;
   regex_t reg;
   Liste_Menu *menu=NULL, *courant=NULL, *start=NULL;
   
   if (Options.use_regexp) {
     if (regcomp(&reg,mat,REG_EXTENDED|REG_NOSUB))
       return -1;
   }

   buf[0]=safe_malloc(8*sizeof(char));
   parcours=NULL;
   while (parcours || (passage<flags)) {
      if (parcours==NULL) {
	 if (passage>=0) {
	    if (!Options.use_menus) {
              Aff_fin("-suite-");
              key=Attend_touche();
              if (KeyBoard_Quit) {
	        free(buf[0]);
	        if (Options.use_regexp) regfree(&reg);
	        return 0;
	      }
	    } else if ((menu) && (passage!=0)) {
	      *retour=(passage<2 ? Menu_simple(menu,start,
				(passage<2 ? Ligne_carac_du_groupe : NULL),
	     		            (passage<2 ? chg_grp_in :
	     					 chg_grp_not_in),
			  nom) : NULL);
	      Libere_menu_noms(menu);
	      if (*retour) {
	         free(buf[0]);
		 if (Options.use_regexp) regfree(&reg);
		 return 0;
	      }
	      menu=start=courant=NULL;
	    }
	 }
	 if (!Options.use_menus) {
           row=1+Options.skip_line;
           Cursor_gotorc(1,0);
           Screen_erase_eos();
           Cursor_gotorc(row,0);
	 }
	 passage++;
	 switch (passage) {
	   case 0 : nom="Newsgroups auquels vous êtes abonnés.";
	   	    if (!Options.use_menus) Screen_write_string(nom);
		    break;
	   case 1 : nom="Newsgroups présents dans le .flnewsrc.";
	   	    if (!Options.use_menus) Screen_write_string(nom);
		    break;
	   case 2 : nom="Autres newsgroups.";
	   	    if (!Options.use_menus) Screen_write_string(nom);
		    strcpy (*buf, "active");
		    {
		       char *buf2;
		       int tofree=0;
		       if (Options.use_regexp)
		       {buf2=reg_string(mat,1); tofree=1; }
		       else
			 buf2=mat;
		       if (!buf2) {buf2="*"; tofree=0;}
		       buf[1]=safe_malloc(strlen(buf2)+4);
		       if (Options.use_regexp)
			 sprintf(buf[1],"%s",buf2);
		       else
		         sprintf(buf[1],"*%s*",buf2);
		       if(tofree) free(buf2);
		    }
		    if (debug) fprintf(stderr,"Liste des groupes %s\n",
			buf[1]);

		    res=write_command (CMD_LIST, 2, buf);
		    if (res<1) code=-1; 
		    else code=return_code();
                    if ((code<0) || (code>400)) {
		      free(buf[0]); free(buf[1]);
		      if (Options.use_regexp) regfree(&reg);
		      return 0;
		    }
		    break;
         }
         parcours=Newsgroup_deb;
         row++;
	 continue;
      }

      if ((passage==0) && (parcours->flags & GROUP_UNSUBSCRIBED)) {
	 parcours=parcours->next;
	 continue;
      }
      if ((passage==1) && !(parcours->flags & GROUP_UNSUBSCRIBED)) {
	 parcours=parcours->next;
	 continue;
      }
      if ( (passage<2) &&(
	  (Options.use_regexp && regexec(&reg,parcours->name,0,NULL,0)!=0) ||
	  (!Options.use_regexp && !strstr(parcours->name, mat)))) {
	 parcours=parcours->next;
	 continue;
      }
      /* Peut-etre a mettre ailleurs */
      if ((passage<2) && (parcours->description==NULL) 
      		      && (!Options.use_menus)) {
	get_group_description(parcours);
      }
      if (passage==2) {
        res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1);
        if (res<2) {
	  free(buf[0]); free(buf[1]);
	  if (Options.use_regexp) regfree(&reg);
	  return 0;
	}
        if (tcp_line_read[0]=='.') { parcours=NULL; continue; }
	ptr=strchr(tcp_line_read,' ');
	if (ptr) *ptr='\0'; else tcp_line_read[res-2]='\0';
	/* Test si ce newsgroup existe deja */
	while (parcours) {
	   if (strcmp(tcp_line_read, parcours->name)==0) {
	      parcours=Newsgroup_deb;
	      break;
	   }
	   parcours=parcours->next;
	}
	if (parcours!=NULL) continue;
	parcours=Newsgroup_deb;
	if (Options.use_regexp && regexec(&reg,tcp_line_read,0,NULL,0))
	  continue;
	if (!Options.use_regexp && !strstr(tcp_line_read,mat))
	  continue;
      }  /* On se sert directement de tcp_line_read */
	 
      if ((row>Screen_Rows-2-Options.skip_line) && (!Options.use_menus)) {
         Aff_fin("-suite-");
         key=Attend_touche();
         if (KeyBoard_Quit) { 
	   if (passage==2) discard_server();
	   return 0;
	 }
         Cursor_gotorc(1,0);
         Screen_erase_eos();
         row=1+Options.skip_line;
      } 
       
      strcpy(une_ligne,"  ");
      if (passage<2) {
        Cursor_gotorc(row,0);
	if (parcours==Newsgroup_courant)
	  *une_ligne='>';
	else
        if ((flag=calcul_flag(parcours)))
          *une_ligne=flag;
	else
	  if (NoArt_non_lus(parcours,0)>0) *une_ligne='+';
      }
      Cursor_gotorc(row,2);
      if (passage<2) {
        strncat(une_ligne,truncate_group(parcours->name,0),77);
	une_ligne[79]='\0';
        if (parcours->description) strncat(une_ligne,parcours->description,79-strlen(une_ligne));
	une_ligne[79]='\0';
      } else strncat(une_ligne,tcp_line_read,77);
      if (Options.use_menus) {
          ptr=safe_strdup(une_ligne);
          courant=ajoute_menu(courant,ptr,(passage < 2 ? (void *)parcours : ptr+2));
	  if ((passage<2) && (parcours==Newsgroup_courant)) start=courant;
	  if (!menu) menu=courant;
      } else Screen_write_string(une_ligne);
      if (passage<2) parcours=parcours->next;
      row++;
   }
   if ((menu) && (Options.use_menus)) {
      *retour=(passage<2 ? Menu_simple(menu,start,(passage<2 ? 
      						Ligne_carac_du_groupe : NULL),
   		            (passage<2 ? chg_grp_in :
    					 chg_grp_not_in),
		  nom) : NULL);
      Libere_menu_noms(menu);
   }
   free(buf[0]);
   if (Options.use_regexp) regfree(&reg);
   if (passage==2) free(buf[1]);
   return 0;
}

/* Affichage de l'arbre autour d'un message */
#define SYMB_ART_READ 'o'
#define SYMB_ART_UNK  '?'
#define SYMB_ART_UNR  'O'
#define SYMB_ART_COUR '@'
#define SYMB_SLASH    '/'
#define SYMB_ANTISLASH '\\'
#define SYMB_LOWER    'v'
#define SYMB_HIGHER   '^'
#define SYMB_BEFORE   '<'
#define SYMB_AFTER    '>'
/* row, col : coordonnées du haut a gauche */
/* on essaie d'arranger to_left et to_right... height est la max... */
/* table est un pointeur vers un tableau de taille ((to_left+to_right)*2+3, */
/* height+1) ou plutôt l'inverse... il est déjà alloué... */
/* add_to_scroll ajoute l'arbre au scrolling, en supposant que le scrolling */
/* commence en premiere ligne...					*/
/* on renvoie la colonne atteinte...					*/
int Aff_arbre (int row, int col, Article_List *init,
    		int to_left, int to_right, int height,
		unsigned char **table, int add_to_scroll) {
  Article_List *parcours=init, *parcours2, *parcours3;
  int up,down, act_right, act_right_deb, act_right_temp, left, right, initcol;
  int row_deb=row; /* juste pour l'affichage */
  unsigned short *toto;

  /* on "nettoie" table */
  for (up=0;up<height+1;up++)
    memset (table[up], ' ', (to_left+to_right)*2+3);
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
     (table[0])[0]='-';
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
     if (parcours==init) (table[0])[cl]=SYMB_ART_COUR;
        else (table[0])[cl]=char_for_art(parcours);
     if (act_right!=-left) (table[0])[cl-1]='-'; 
     /* on commence l'infame parcours */
     parcours2=parcours->frere_prev;
     rw=height+1-up;
     if (parcours2)
     while (act_right>=act_right_deb) {
        if ((remonte!=1) && (up+((down==0) ? 1 : down)==height)) {
      /* cela ne peut pas arriver si on vient de descendre. C'est un frere */
      /* A moins que remonte==2 */
	   if (((table[rw])[cl-1]==' ') && (parcours2->frere_suiv))
	           (table[rw])[cl-1]=SYMB_SLASH;
	   (table[rw])[cl]=SYMB_HIGHER;
	   c=cl-2; r=rw+1;
	   if (((table[rw])[cl-1]==SYMB_ANTISLASH) || (parcours2->frere_suiv==NULL))
	           remonte=2; /* obligatoire */
	   else
	   if ((c<0) || ((table[rw])[c]!=' ')) remonte=1; 
	   else {
	     while ((r<height+1) && 
	         ((table[r])[c]==' ') && ((table[r])[c+1]!=SYMB_ANTISLASH)) 
	        (table[r++])[c]='|';
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
	   (table[rw])[cl]=char_for_art(parcours2);
	   if (parcours2->prem_fils) (table[rw])[cl+1]=SYMB_AFTER;
	   act_right_temp=act_right; parcours3=parcours2; c=cl;
	   /* et on remonte pour l'affichage */
           while (act_right_temp>act_right_deb) {
	      if (parcours3->pere->prem_fils==parcours3) {
		 parcours3=parcours3->pere;
		 act_right_temp--; c-=2;
		 (table[rw])[c]=char_for_art(parcours3);
		 (table[rw])[c+1]='-';
		 r=rw+1;
		 if (parcours3->prem_fils->frere_suiv)
		 while ((r<height+1) && ((table[r])[c]==' ') && 
		     ((table[r])[c+1]!=SYMB_ANTISLASH))
		     (table[r++])[c]='|';
	      } else break;
	   }
	   if (parcours3->frere_suiv==NULL) 
	      (table[rw])[c-1]=SYMB_ANTISLASH; else
	   if (parcours3->frere_prev==NULL) {
	      (table[rw])[c-1]=SYMB_SLASH;
	      r=rw+1;
	      while ((r<height+1) && ((table[r])[c-2]==' '))
		(table[r++])[c-2]='|';
	   } else (table[rw])[c-1]='-';
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
	   if (((table[rw])[cl-1]==' ') && (parcours2->frere_prev))
	            (table[rw])[cl-1]=SYMB_ANTISLASH;
	   (table[rw])[cl]=SYMB_LOWER;
	   c=cl-2; r=rw-1;
	   if (((table[rw])[cl-1]==SYMB_SLASH) || (parcours2->frere_prev==NULL))
	                         remonte=2; /* obligatoire */
	   if ((c<0) || ((table[rw])[c]!=' ')) remonte=1; 
	   else {
	     while ((r>0) && 
	         ((table[r])[c]==' ') && ((table[r])[c+1]!=SYMB_SLASH)) 
	        (table[r--])[c]='|';
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
	   (table[rw])[cl]=char_for_art(parcours2);
	   if (parcours2->prem_fils) (table[rw])[cl+1]=SYMB_AFTER;
	   act_right_temp=act_right; parcours3=parcours2; c=cl;
	   /* et on remonte pour l'affichage */
           while (act_right_temp>act_right_deb) {
	      if (parcours3->pere->prem_fils==parcours3) {
		 parcours3=parcours3->pere;
		 act_right_temp--; c-=2;
		 (table[rw])[c]=char_for_art(parcours3);
		 (table[rw])[c+1]='-';
		 r=rw-1;
		 if (parcours3->prem_fils->frere_prev)
		 while ((r>0) && ((table[r])[c]==' ') && ((table[r])[c+1]!=SYMB_SLASH))
		     (table[r--])[c]='|';
	      } else break;
	   }
	   if (parcours3->frere_prev==NULL) 
	      (table[rw])[c-1]=SYMB_SLASH; else
	   if (parcours3->frere_suiv==NULL) {
	      (table[rw])[c-1]=SYMB_ANTISLASH;
	      r=rw-1;
	      while ((r>0) && ((table[r])[c-2]==' '))
		(table[r--])[c-2]='|';
	   } else (table[rw])[c-1]='-';
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
    Screen_write_nstring(table[height+1-up],initcol+right*2+2);
    if (add_to_scroll) {
       toto=cree_chaine_mono(table[height+1-up],FIELD_NORMAL,initcol+right*2+2);
       if (Rajoute_color_Line(toto,row-1-row_deb,initcol+right*2+2,col)==NULL) {
          Ajoute_color_Line(NULL,0,Screen_Cols);
	  Rajoute_color_Line(toto,-1,initcol+right*2+2,col);
       }
       free(toto);
    }
  }
  for (up=0;up<=down;up++) {
    Cursor_gotorc(row++,col);
    Screen_write_nstring(table[up],initcol+right*2+2);
    if (add_to_scroll) {
       toto=cree_chaine_mono(table[up],FIELD_NORMAL,initcol+right*2+2);
       if (Rajoute_color_Line(toto,row-1-row_deb,initcol+right*2+2,col)==NULL) {
          Ajoute_color_Line(NULL,0,Screen_Cols);
	  Rajoute_color_Line(toto,-1,initcol+right*2+2,col);
       }
       free(toto);
    }
  }
  return (row-1);
}
     


/* Affichage d'un header, juste appelé par Aff_headers */
/* flag=1 : juste une ligne... flag=2 : cas particulier reponse_a */
/* with_arbre : on va afficher l'arbre, réserver de la place...   */
/*    (0 : rien    1 : petites fleches     2 : arbre complet  */
/* add_to_scroll : on ajoute au scrolling ce qu'on écrit....  */
/* (en gros c'est a peu près comme flag sauf cas flag=2 :-(   */
static int Aff_header (int flag, int with_arbre, int row, int col, char *str, 
    		unsigned short *to_aff, int add_to_scroll) {
   char *buf=str, *buf2;
   unsigned short *ptr=to_aff;
   int decalage;

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
}

/* Utilise Options.user_flags pour faire des flags immonde */
/* Pour l'instant, on fait seulement les Known_headers */
static char *Recupere_user_flags (Article_List *article) {
   char *str, flag, *buf;
   int size=0;
   flrn_filter *filt;
   string_list_type *parcours=Options.user_flags;

   while (parcours) {
     size++;
     parcours=parcours->next;
   }
   if (size==0) return NULL;
   str=safe_malloc(size+1);
   size=0;
   filt=new_filter();
   parcours=Options.user_flags;
   while (parcours) {
     memset(filt,0,sizeof(flrn_filter));
     filt->action.flag=FLAG_SUMMARY;
     buf=parcours->str;
     parcours=parcours->next;
     flag=*(buf++);
     while ((*buf) && (isblank(*buf))) buf++;
     if (parse_filter_flags(buf,filt)) 
       parse_filter(buf,filt);
     if (!check_article(article,filt,0)) str[size++]=flag;
   }
   free_filter(filt);
   if (size==0) free(str); else str[size]='\0';
   return (size==0 ? NULL : str);
}


/* flag==1 : pas de scrolling possible */
int Aff_place_article (int flag){
   int with_arbre;
   int row=0;

   with_arbre=1+Options.alpha_tree;
   Screen_set_color(FIELD_NORMAL);
   if (with_arbre==1) {
     /* On affiche les petites fleches */
     /* provisoirement, on les ajoute pas au scrolling (ARGH !!!) */
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
   int index=0, row, row2, col, i, j, i0, length;
   char buf[15];
   Header_List *tmp=Last_head_cmd.headers;
   char *une_ligne=NULL, *flags_header;
   unsigned short *une_belle_ligne=NULL;
   int flags[NB_KNOWN_HEADERS];
   int with_arbre;

/*   with_arbre=1+(Options.alpha_tree && (!flag)); */
   with_arbre=1+Options.alpha_tree;
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
	  if (strncasecmp(tmp->header,unknown_Headers[-2-i].header,
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
	  if (strncasecmp(tmp->header,unknown_Headers[-2-i].header,
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
       if (une_belle_ligne) {
	 free(une_belle_ligne);
	 une_belle_ligne=NULL;
       }
       switch (i0) {
         case REFERENCES_HEADER: /* traitement special */
           if (Article_courant->headers->reponse_a) {
	     une_ligne=safe_malloc(22+2*strlen(Article_courant->headers->reponse_a));
             strcpy(une_ligne,"Réponse à: ");
	     if (!Options.parse_from) 
	     	strcat(une_ligne,Article_courant->headers->reponse_a);
	     else ajoute_parsed_from(une_ligne,Article_courant->headers->reponse_a);
	     une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	     Aff_color_line(0,une_belle_ligne, &length,
		 FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
	     row=Aff_header(2, with_arbre, row, 0, une_ligne, une_belle_ligne,!flag);
   	     Screen_set_color(FIELD_HEADER);
	     if (Article_courant->parent>0) {
	        sprintf(buf," [%d]", Article_courant->parent);
	        Screen_write_string(buf);
		if (!flag) {
		  free(une_belle_ligne);
		  une_belle_ligne=cree_chaine_mono(buf,FIELD_HEADER,-1);
		  Rajoute_color_Line(une_belle_ligne,-1,strlen(buf),-1);
		}
	     }
	    /* On ne garde qu'une ligne dans tous les cas */
           } else if (Article_courant->parent!=0) {
   	      Screen_set_color(FIELD_HEADER);
              Cursor_gotorc(row,0);
	      une_ligne="Réponse à un message non disponible.";
	      une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	      Aff_color_line(0,une_belle_ligne, &length,
		 FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
	      row=Aff_header(1, with_arbre, row, 0, une_ligne, une_belle_ligne,!flag);
	      une_ligne=NULL; /* ne pas liberer une chaine non allouée */
           } 
         break;
         case DATE_HEADER:
           Cursor_gotorc(row,0);
	   une_ligne=safe_malloc(60);
           strcpy(une_ligne,"Date: ");
	   if (Article_courant->headers->date_gmt) {
	     strncat(une_ligne,ctime(&Article_courant->headers->date_gmt),53);
	   } else
	     strncat(une_ligne,
	              Article_courant->headers->k_headers[DATE_HEADER],53);
	   une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	   Aff_color_line(0,une_belle_ligne, &length,
	          FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
	   row=Aff_header(1, with_arbre, row, 0, une_ligne, une_belle_ligne,!flag);
           break;
         case FROM_HEADER:
	   une_ligne=safe_malloc(14+2*strlen(Article_courant->headers->k_headers[FROM_HEADER]));
           strcpy(une_ligne,"Auteur: ");
	   if (!Options.parse_from) 
	     	strcat(une_ligne,Article_courant->headers->k_headers[FROM_HEADER]);
	   else ajoute_parsed_from(une_ligne,Article_courant->headers->k_headers[FROM_HEADER]);
	   une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	   Aff_color_line(0,une_belle_ligne, &length,
		 FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
	   row=Aff_header(flag,with_arbre, row,0,une_ligne,une_belle_ligne,!flag);
         break;
         case SUBJECT_HEADER:
   	   Screen_set_color(FIELD_HEADER);
           Cursor_gotorc(row,0);
	   col=0;
	   if (flag) { Screen_write_string("(suite) "); col+=8; }
	      /* pas besoin d'allouer : add_to_scroll =0  dans Aff_header */
	   if ((flags_header=Recupere_user_flags(Article_courant))) {
	  	Screen_write_string(flags_header); 
		col+=strlen(flags_header); 
		if (!flag) {
		  une_belle_ligne=cree_chaine_mono(flags_header,FIELD_HEADER,-1);
		  Ajoute_color_Line(une_belle_ligne,strlen(flags_header),Screen_Cols);
		  free(une_belle_ligne);
		}
		free(flags_header);
	   }
	   une_ligne=safe_malloc(9+strlen(Article_courant->headers->k_headers[SUBJECT_HEADER]));
           strcpy(une_ligne,"Sujet: ");
	   strcat(une_ligne,Article_courant->headers->k_headers[SUBJECT_HEADER]);
	   une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	   Aff_color_line(0,une_belle_ligne, &length,
		 FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
	   row=Aff_header(flag,with_arbre, row,col,une_ligne,une_belle_ligne,!flag);
         break;
         default: if (i0==NB_KNOWN_HEADERS) {
		    tmp=Last_head_cmd.headers;
		    while (tmp) {
		      if (tmp->num_af==index) {
			if (une_belle_ligne) free(une_belle_ligne);
	   		une_belle_ligne=safe_malloc((strlen(tmp->header)+1)
			    		*sizeof(short));
			Aff_color_line(0,une_belle_ligne, &length, 
			    FIELD_HEADER, tmp->header, strlen(tmp->header),
			    1,FIELD_HEADER);
	   		row=Aff_header(flag,with_arbre, row,0,tmp->header,
					une_belle_ligne,!flag);
		      }
		      tmp=tmp->next;
		    }
		    break;
		  } 
	 	  if ((i0>=0) && 
		      (Article_courant->headers->k_headers[i0])==NULL) 
		    break; else
		  if (i0<0) {
		    tmp=Last_head_cmd.headers;
		    while (tmp) {
		      if (tmp->num_af==index) break;
		      tmp=tmp->next;
		    }
		    if (!tmp) break;
		    une_ligne=tmp->header;
		  } 
	 	  if (i0>=0) {
	            une_ligne=safe_malloc(Headers[i0].header_len+3+
			    strlen(Article_courant->headers->k_headers[i0]));
           	    strcpy(une_ligne,Headers[i0].header);
		    strcat(une_ligne," ");
	            strcat(une_ligne,Article_courant->headers->k_headers[i0]);
	          }
	         une_belle_ligne=safe_malloc((strlen(une_ligne)+1)*sizeof(short));
	         Aff_color_line(0,une_belle_ligne, &length,
		   FIELD_HEADER, une_ligne, strlen(une_ligne), 1,FIELD_HEADER);
		 row=Aff_header(flag,with_arbre,row,0,une_ligne,
		 			une_belle_ligne,!flag);
		 if (i<0) une_ligne=NULL; /* Ne pas libérer un header */
         break;
       }
     }
   }
   if (une_ligne) free(une_ligne);
   if (une_belle_ligne) free(une_belle_ligne);
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
   int res, tmp_col=0, percent, len_to_write;
   char *buf,*buf2, *une_ligne;
   char buf3[15];
   int last_color;
   int bol;
   unsigned short *une_belle_ligne;
   int length;

   if ((!from_file) && (tcp_line_read[0]=='.')) {
      if (tcp_line_read[1]=='.') buf=tcp_line_read+1; else 
	return (en_deca ? 0 : -act_row+saved_space);
   } else buf=tcp_line_read; 
   if (saved_field!=FIELD_SIG) {
     if (buf[0]=='>') saved_field=FIELD_QUOTED; else saved_field=FIELD_NORMAL;
   }
   if (strncmp(buf,"-- \r",4)==0) saved_field=FIELD_SIG;
   une_ligne=safe_malloc(Screen_Cols+1); /* si on atteint le max, ce sera
					    un blanc... */
   une_belle_ligne=safe_malloc(sizeof(short)*(Screen_Cols+1));
   length=0;
   une_ligne[0]='\0';
   if (*buf=='\r') { 
     saved_field=FIELD_NORMAL; /* On enleve le style "signature" */
     saved_space++;
     free(une_ligne); free(une_belle_ligne);
     return (act_row+1); /* On sort tout de suite, mais en augmentant le  */
     	                 /* nombre de ligne (pour éviter le fameux (100%) */
   }
   /*
   Screen_set_color(saved_field);
   */
   if (from_file) saved_field=FIELD_FILE;
   last_color=saved_field;
   bol=1;

   buf2=strchr(buf,(from_file ? '\n' : '\r'));    
	/* Bon d'accord, on formate rien, mais bon */
   if ((!buf2) && (!from_file)) buf2=strchr(buf,'\n');
	/* Pour corriger une erreur dans inn-2.1 :-( */
   if (buf2) *buf2='\0';
   while (1) {
      if ((act_row>Screen_Rows-2) && (en_deca)) {  /* Fin de l'ecran */

	 if ((!from_file) && (Article_courant->headers->nb_lines!=0)) {
	    percent=((read_line-saved_space-1)*100)/(Article_courant->headers->nb_lines+1);
            sprintf(buf3,"(%d%%)",percent);
	 } else buf3[0]='\0';
	 strcat(buf3,"-More-");
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
   return 0;
}      


/* Gestion du scrolling... */
int Gere_Scroll_Message (int *key_int, int row_act, int row_deb, 
				int scroll_headers, int to_build) {
  int act_row, key;
  int num_elem=-row_act-row_deb, percent;
  char buf3[15];

  if (to_build) {
     Aff_fin("Patientez...");
     Screen_refresh();
     to_build=cree_liste_suite(0);
     percent=((Screen_Rows-row_deb-2)*100)/(num_elem+1);
     sprintf(buf3,"(%d%%)",percent);
     strcat(buf3,"-More-");
     Aff_fin(buf3);
  }
  key=Attend_touche();
  if (KeyBoard_Quit) return -1;
  Cursor_gotorc(1,0);
  Screen_set_color(FIELD_NORMAL);
  Screen_erase_eos();
  if (scroll_headers==0) act_row=Aff_headers(1)+1; else
      act_row=1+Options.skip_line;
  Init_Scroll_window(num_elem,act_row,Screen_Rows-act_row-1);
  key=Page_message(num_elem, 1, key, act_row, row_deb, NULL, "A vous : ",
  		    (to_build ? cree_liste_suite : NULL));
  if (key!=-1) *key_int=key & (~MAX_FL_KEY); else return -1;
  return (((key==0) || (key & MAX_FL_KEY)) ? 0 : -1);
}

/* Affichage du nom du newsgroup */
void Aff_newsgroup_name(int erase_scr) {
   char *buf=NULL, *tmp_name;
   int buf_to_free=0;
   char flag_aff=0;

   Screen_set_color(FIELD_STATUS);
   Cursor_gotorc(0,name_news_col);
   if (name_fin_col-name_news_col>0) {
     if (Newsgroup_courant) {
       if (!(Newsgroup_courant->flags & GROUP_READONLY_TESTED))
          test_readonly(Newsgroup_courant);
       flag_aff=calcul_flag(Newsgroup_courant);
       tmp_name=truncate_group(Newsgroup_courant->name,0);
       if (strlen(tmp_name)<name_fin_col-name_news_col-((flag_aff ? 1 : 0)*3))
          buf=tmp_name;
       else 
          buf=tmp_name+(strlen(tmp_name)-name_fin_col+name_news_col+((flag_aff ? 1 : 0)*3));
       if (flag_aff) {
	 Screen_write_char('[');
	 Screen_write_char(flag_aff);
	 Screen_write_char(']');
       }
     } else {
       buf=safe_malloc((name_fin_col-name_news_col+1)*sizeof(char));
       memset(buf, ' ', name_fin_col-name_news_col+1);
       buf_to_free=1;
     }
     Screen_write_nstring(buf, name_fin_col-name_news_col+1-((flag_aff ? 1 : 0)*3));
   }
   if (erase_scr) {
     Screen_set_color(FIELD_NORMAL);
     Cursor_gotorc(1,0);
     Screen_erase_eos();
     Screen_refresh(); 
          /* Cas particulier : pour le temps que ça prend parfois */
   }
   if(buf_to_free) free(buf);
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


/* Affichage de l'article courant */
/* Cette fonction renvoie le code de la touche qui suit, ssi il y a eu */
/* interruption... et 0 sinon...			 */
/* Note : les appels se font avec le numero de l'article */
/* comme il semble que ça marche mieux...		 */
int Aff_article_courant(int to_build) {
   int res, actual_row, read_line=1;
   int key_interrupt, first_line, scroll_headers=0;
   char *num, buf[10];
   
   if (debug) fprintf(stderr, "Appel a Aff_article_courant\n");

   key_interrupt=0;
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
   if (Article_courant->numero==-10) 
	return Aff_error("Pas d'article disponible.");
    
   Cursor_gotorc(1,0);
   Screen_erase_eos(); /* on veut effacer aussi les lignes du haut */
   Cursor_gotorc(1+Options.skip_line,0);

  /* Headers  -- on veut all_headers <=> un appel a cree_headers */
   if ((!Article_courant->headers) ||
       (Article_courant->headers->all_headers==0) || (Last_head_cmd.Article_vu!=Article_courant)) {
        cree_header(Article_courant,1,1,0);
        if (!Article_courant->headers) {
	    Aff_place_article(1);
	    Screen_set_color(FIELD_ERROR);
	    Cursor_gotorc(row_erreur,col_erreur); 
	    Screen_write_string("Article indisponible.");
	    Screen_set_color(FIELD_NORMAL);
	    return 0;
	}
        /* On suppose ca provisoire */
	/* le bon check, c'est pere != NULL ou parent !=0 ?
	 * esperons que ca coincide */
   } else if ((Article_courant->pere) && 
              (Article_courant->headers->reponse_a_checked==0)) 
	   ajoute_reponse_a(Article_courant);
   actual_row=Aff_headers(0);
   if ((actual_row>Screen_Rows-2) || (Options.headers_scroll)) {
      Ajoute_color_Line(NULL,0,0); /* on saute une ligne dans le vide */
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
     Aff_error("Article indisponible."); 
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
      actual_row=Gere_Scroll_Message(&key_interrupt,actual_row,
   	(scroll_headers ? 1+Options.skip_line : first_line), scroll_headers, to_build);
   free_text_scroll();
   if (actual_row==0)  
     article_read(Article_courant); /*Article_courant->flag |= FLAG_READ;*/
   Aff_not_read_newsgroup_courant();
   if (debug) fprintf(stderr,"Fin d'affichage \n");
   return key_interrupt;
}

       
/* Affichage d'un fichier. */
int Aff_file (FILE *file, char *exit_chars, char *end_mesg) {
   int row=1,read_line=1;
   int key=0;
  
   saved_field=FIELD_NORMAL;
   saved_space=0;
   en_deca=1;
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   while (fgets(tcp_line_read, MAX_READ_SIZE-1, file)) {
      row=Ajoute_aff_formated_line(row, read_line, 1);
      read_line++;
   }
   if (row>Screen_Rows-1) {  /* On entame un scrolling */
      key=Attend_touche();
      if (KeyBoard_Quit) return 0;
      Init_Scroll_window(row-1, 1, Screen_Rows-2);
      key=Page_message(row-1, 0, key, 1, 1, exit_chars, end_mesg, NULL); 
      if (key==-1) key=0;
   }
   free_text_scroll();
   return key;
}
