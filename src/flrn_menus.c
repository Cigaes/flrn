/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_menus.c : gestion des menus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#define IN_FLRN_MENUS_C

#include <ctype.h>
#include <stdlib.h>

#include "flrn.h"
#include "options.h"
#include "flrn_menus.h"
#include "flrn_command.h"
#include "tty_display.h"
#include "flrn_slang.h"
#include "flrn_messages.h"

static UNUSED char rcsid[]="$Id$";

/* le tableau touche -> commande */
int *Flcmd_menu_rev = &Flcmd_rev[CONTEXT_MENU][0];
/* pour les macros */
char *Macro_menu_bef, *Macro_menu_aft;
int nxt_menu_cmd;

/* Cette fonction est définie dans flrn_pager.c */
/* Je préfère la déclarer en extern ici */
extern int get_new_pattern();

int get_menu_signification(char *str) {
   int a;
   while ((*str) && (isblank(*str))) str++;
   if (*str=='\0') return 1;
   switch (*str) {
         case '*' : return -1; /* tout */
         case '.' : return -2; /* selected */
         case '_' : return -3; /* tout ce qui matche */
         case '<' : return -4; /* tout ce qui precede */
         case '>' : return -5; /* tout ce qui suit */
         default :  a=strtol(str,NULL,10);
                    return (a<1 ? 1 : a);
   }
}


/* retour : -4 : CONTEXT_COMMAND */
int get_command_menu(int with_command, int *number)  {
   int res, res2=-1, bef=0, num_macro=0;

   if (nxt_menu_cmd==-1) {
     res=get_command(0,CONTEXT_MENU, (with_command ? CONTEXT_COMMAND : -1), &une_commande);
     if ((res==0) && ((res2=une_commande.cmd[CONTEXT_MENU]) & FLCMD_MACRO)) {
         num_macro=res2 ^ FLCMD_MACRO;
	 if (Flcmd_macro[num_macro].next_cmd!=-1) {
           if (une_commande.before) 
	      Macro_menu_bef=safe_strdup(une_commande.before);
           if (une_commande.after) 
	      Macro_menu_aft=safe_strdup(une_commande.after);
	 }
     }
   } else {
     res=0;
     if (Macro_menu_bef) une_commande.before=safe_strdup(Macro_menu_bef); else
         une_commande.before=NULL;
     if (Macro_menu_aft) une_commande.after=safe_strdup(Macro_menu_aft); else
         une_commande.after=NULL;
     res2=une_commande.cmd[CONTEXT_MENU]=nxt_menu_cmd;
   }
   if ((res==1) && (une_commande.cmd[CONTEXT_COMMAND] & FLCMD_MACRO)) {
      Aff_error_fin(_(Messages[MES_MACRO_FORBID]),1,1);
      if (une_commande.before) free(une_commande.before);
      if (une_commande.after) free(une_commande.after);
      return -1;
   }
   if (res==-1)
      Aff_error_fin(_(Messages[MES_UNKNOWN_CMD]),1,1);
   if (res<0) {
      if (une_commande.before) free(une_commande.before);
      if (une_commande.after) free(une_commande.after);
      return res;
   }
   if (res==1) return -4;
   *number=1;
   /* Le search */
   if (res2 & FLCMD_MACRO) {
      num_macro=res2 ^ FLCMD_MACRO;
      nxt_menu_cmd=Flcmd_macro[num_macro].next_cmd;
      res2=Flcmd_macro[num_macro].cmd;
      if ((res2!=FLCMD_MENU_CMD) && (Flcmd_macro[num_macro].arg)) {
          if (une_commande.after) free(une_commande.after);
	  une_commande.after=safe_strdup(Flcmd_macro[num_macro].arg);
      }
   } else nxt_menu_cmd=num_macro=-1;
   if (nxt_menu_cmd==-1) {
      if (Macro_menu_bef) free(Macro_menu_bef);
      if (Macro_menu_aft) free(Macro_menu_aft);
      Macro_menu_bef=Macro_menu_aft=NULL;
   }
   if (res2==FLCMD_MENU_CMD) {
      une_commande.cmd[CONTEXT_MENU]=-1;
      if ((num_macro!=-1) && (Flcmd_macro[num_macro].arg)) {
         char *buf=Flcmd_macro[num_macro].arg;
	 while ((*buf) && (isblank(*buf))) buf++;
         res=Lit_cmd_explicite(buf, CONTEXT_COMMAND,-1,&une_commande);
	 buf=strchr(buf,' ');
	 if (buf!=NULL) {
	   if (une_commande.after) free(une_commande.after);
	   une_commande.after=safe_strdup(buf);
	 }
      } else if (une_commande.after) {
         char *buf=une_commande.after;
	 while ((*buf) && (isblank(*buf))) buf++;
         res=Lit_cmd_explicite(buf,CONTEXT_COMMAND,-1,&une_commande);
	 buf=strchr(buf,' ');
	 if (buf==NULL) {
	    free(une_commande.after);
	    une_commande.after=NULL;
	 } else {
	    buf=safe_strdup(buf);
	    free(une_commande.after);
	    une_commande.after=buf;
	 }
      } else res=-1;
      if (res<0) {
         if (une_commande.before) free(une_commande.before);
         if (une_commande.after) free(une_commande.after);
         Aff_error_fin(_(Messages[MES_UNKNOWN_CMD]),1,1);
         return -1;
      }
      /* res=0 */
      return -4;
   }
   if (une_commande.before) {
      bef=1;
      *number=get_menu_signification(une_commande.before);
      free(une_commande.before);
   }
   if (res2!=FLCMD_MENU_SEARCH) {
      if (une_commande.after) {
	  if (!bef) *number=get_menu_signification(une_commande.after);
	  free(une_commande.after);
      }
      return res2;
   }
   if (une_commande.after) {
      strncpy(pattern_search,une_commande.after,SIZE_PATTERN_SEARCH-1);
      pattern_search[SIZE_PATTERN_SEARCH-1]='\0';
      free(une_commande.after);
   } else {
      res=get_new_pattern();
      if (res<0) return -2;
   }
   return res2;
}

/* Cette fonction ne renvoie qu'un élément sélectionné, ou bien entendu NULL.
 * action est appelée quand le curseur est devant un item. Elle recoit une
 * ligne dont la longueur est passée avec laquel elle peut jouer.
 * action_select est appelée quand on fait return ou une commande exterieure.
 * si elle est vaut NULL, on quite simplement. On quite si elle renvoie -1,
 * on ne fait rien si -2, et on affiche une ligne si no_change.*=1
 * on update affichage si retour =1
 * */
void *Menu_simple (Liste_Menu *debut_menu, Liste_Menu *actuel,
	      void (action)(void *,char *,int),
	      int (action_select)(Liste_Menu *, Liste_Menu **,
	          char *,int, Cmd_return *, int *), char *chaine) 
{
  int act_row=1+Options.skip_line, last_act_row, num_elem=0, number=1;
  int correct=0; /* Pour savoir si on s'arrete ou pas... */
  Liste_Menu *courant=actuel, *parcours=debut_menu;
  int avant=1;
  char *Une_Ligne;
  int Une_Ligne_len;
  
  Macro_menu_bef=NULL; Macro_menu_aft=NULL; nxt_menu_cmd=-1;
  if (parcours==NULL) return NULL; /* Ouais ouais ouais... */
  Une_Ligne=safe_malloc(Une_Ligne_len=(Screen_Cols +1));
  
/* Il va falloir créer un scrolling pour ce menu, on commence donc par
 * remplir Text_scroll...
 * A priori, cette partie-ci pourra être recopiée pour toutes les
 * fonctions de menus, moyennant quelques modifications.
 * Faut-il vraiment faire un scrolling quand ce n'est pas nécessaire...
 * Faudra voir si on ne peut pas s'en passer, pour économiser la
 * mémoire. */
  while (parcours) {

    strcpy(Une_Ligne,"  ");
    strncat(Une_Ligne,parcours->nom,Une_Ligne_len);
    Ajoute_line(Une_Ligne); /* Ajoute_line est peut-être un peu court */
    				/* si on veut rajouter des signes...	  */
    if (parcours==actuel) avant=0;
    if (avant) act_row++; 
    num_elem++;
    parcours->changed=0;
    parcours->toggled=0;
    parcours=parcours->suiv;
  }
/* Si le courant n'est pas défini, on prend le premier élément...	*/
  if (avant) {
    act_row=1+Options.skip_line;
    courant=debut_menu;
  }
/* Puis on initialise le scroll...					*/
  Init_Scroll_window(num_elem, 1+Options.skip_line, 
  			Screen_Rows-3*Options.skip_line-2);
/* Et on fait l'update...						*/
/* Sans oublier d'effacer l'écran d'abord... 				*/
  Cursor_gotorc(1,0);
  Screen_erase_eos();
  Aff_help_line(Screen_Rows-1);
  if (act_row<Screen_Rows-1-Options.skip_line) Do_Scroll_Window(0,1);
  else {
    Do_Scroll_Window(act_row-(Screen_Rows-1)/2,1);
    act_row=(Screen_Rows-1)/2;
  }

/* Maintenant on peut faire des affichages et des choix.		*/
  last_act_row=act_row;
  {
    int res,p,deb;
    int no_change_last_line=0;
    while (!correct) {
    
      /* On update, mais le but est de garder la même ligne... */
      if ((act_row<1+Options.skip_line) || 
      (act_row>Screen_Rows-3-Options.skip_line)) {
	p=Do_Scroll_Window(act_row-last_act_row,0);
	act_row-=p;
      }
      last_act_row=act_row;
      if (action && courant && !no_change_last_line) {
	 Cursor_gotorc(Screen_Rows-2,0);
	 *Une_Ligne='\0';
	 action(courant->lobjet,Une_Ligne, Une_Ligne_len);
	 Screen_write_string(Une_Ligne);
	 Screen_erase_eol();
      } else if (!no_change_last_line) {
	 Cursor_gotorc(Screen_Rows-2,0);
	 Screen_erase_eol();
      }
      parcours=courant;
      for (deb=act_row;deb<Screen_Rows-2-Options.skip_line;deb++) {
	 if (parcours==NULL) break;
	 Cursor_gotorc(deb,1);
	 Screen_write_char(parcours->toggled ? '*' : ' ');
	 parcours=parcours->suiv;
      }
      parcours=courant->prec;
      for (deb=act_row-1;deb>Options.skip_line;deb--) {
	 if (parcours==NULL) break;
	 Cursor_gotorc(deb,1);
	 Screen_write_char(parcours->toggled ? '*' : ' ');
	 parcours=parcours->prec;
      }
      deb=0;
      Cursor_gotorc(act_row,0);
      Screen_write_char('>');
      no_change_last_line=0;
      Aff_fin(chaine);
      res=get_command_menu((action_select!=NULL),&number);
      if (KeyBoard_Quit) {
         courant=NULL; break;
      }
      Cursor_gotorc(act_row,0);
      Screen_write_char(' ');
      if ((res==-4) || (res==FLCMD_MENU_SELECT)) {
	  if (action_select) {
	     Liste_Menu *old_courant=courant;
	     *Une_Ligne='\0';
	     /* On suppose la libération de la mémoire SI pas FLCMD_MENU_SELECT
	        du ressort d'action_select */
	     res=action_select (debut_menu,&courant,Une_Ligne,Une_Ligne_len,
		     &une_commande,&no_change_last_line);
	     if ((courant) && (old_courant!=courant)) {
	        parcours=old_courant;
	        while (parcours && (parcours!=courant)) {
		   act_row++; parcours=parcours->suiv;
		}
		if (parcours==NULL) {
		   parcours=old_courant;
		   act_row=last_act_row;
		   while (parcours && (parcours!=courant)) {
		      act_row--; parcours=parcours->prec;
		   }
		}
	     }
             if (res==-1) break; 
	     if (res && (res!=-2)) {
	         int i=0;
		 char *Une_Ligne_bis;
                 Une_Ligne_bis=safe_malloc(Une_Ligne_len);
		 /* On a besoin de sauvegarder Une_ligne */
	         parcours=debut_menu;
		 while (parcours) {
		    if (parcours->changed) {
		       parcours->changed=0;
		       strcpy(Une_Ligne_bis,"  ");
		       strncat(Une_Ligne_bis,parcours->nom,Une_Ligne_len-3);
		       Une_Ligne_bis[Une_Ligne_len-1]='\0';
		       Change_line(i,Une_Ligne_bis);
		    }
		    i++;
		    parcours=parcours->suiv;
		 }
		 Do_Scroll_Window(0,1);
		 free(Une_Ligne_bis);
	     }
             if (no_change_last_line) {
 	         Cursor_gotorc(Screen_Rows-2,0);
		 Screen_write_string(Une_Ligne);
		 Screen_erase_eol();
	     }
	  } else if (res==FLCMD_MENU_SELECT) break; else
	  {
	     if (une_commande.after) free(une_commande.after);
	     if (une_commande.before) free(une_commande.before);
	  }
	  continue;
      }
      if ((res!=FLCMD_MENU_TOGGLE) && (number<1)) number=1;
      switch (res) {
	 case FLCMD_MENU_UP : while (number) { 
	     		        if (courant->prec!=NULL) {
			          act_row--; courant=courant->prec; }
				else break;
				number--;
		              }
			  break;
	 case FLCMD_MENU_DOWN : while (number) {
				    if (courant->suiv!=NULL) {
			              act_row++; courant=courant->suiv; }
				    else break;
				    number--;
				}
			        break;
	 case FLCMD_MENU_PGUP : while (number) {
				   for (p=0;(p<(Screen_Rows-3
					   -2*Options.skip_line))
					   && (courant->prec); p++) {
				      act_row--; 
				      courant=courant->prec; }
				   if (courant->prec==NULL) break;
				   number--;
				}
				break;
	 case FLCMD_MENU_PGDOWN:while (number) {
				    for (p=0;(p<(Screen_Rows-3
						    -2*Options.skip_line))
	 				    && (courant->suiv); p++) {
				       act_row++; 
				       courant=courant->suiv; }
				       if (courant->suiv==NULL) break;
				       number--;
				}
				break;
	 case FLCMD_MENU_QUIT : courant=NULL;
			   correct=1; break;
	 case FLCMD_MENU_SEARCH : {
                                  int ret;
                                  ret=New_regexp_scroll (pattern_search);
                                  if (ret) 
                                         Aff_error_fin(_(Messages[MES_REGEXP_BUG]),1,1);
                                  Do_Scroll_Window(0,1);
                                     /* pour forcer l'affichage des lignes */
				  if (ret) break;
				  deb=1;
                                } /* Continue */
         case FLCMD_MENU_NXT_SCH : { int ret, le_scroll;
				   while (number) {
                                      ret=Do_Search(deb,&le_scroll,
				   		act_row-1-Options.skip_line);
                                      if (ret==-1) {
                                         Aff_error_fin(_(Messages[MES_NO_SEARCH])
						 ,1,1);
					 break;
				      }
                                      else if (ret==-2) {
                                          Aff_error_fin(_(Messages[MES_NO_FOUND])
					       ,1,1);
					  break;
				      } else if (le_scroll<0) {
				         for (p=0;p<-le_scroll;p++) {
					    if (courant->prec==NULL) break; 
					               /* beurk !*/
					    act_row--;
					    courant=courant->prec;
				         }
				      } else if (le_scroll>0) {
				          for (p=0;p<le_scroll;p++) {
					     if (courant->suiv==NULL) break; 
					     act_row++;
					     courant=courant->suiv;
				          }
				      }
				      number--;
				   } }
                                   break;
         case FLCMD_MENU_TOGGLE : { int r=0;
	                            parcours=((number>0) || (number==-5) ?
	 					courant : debut_menu);
				    if (number==-3) r=Parcours_du_menu(0);
				    for (;parcours;
				            parcours=parcours->suiv) {
				       if (number==-2) {
				          if (parcours->toggled)
				             parcours->toggled=0;
					  continue;
				       }
				       if (number==-3) {
				          if (r) 
					  parcours->toggled=1-parcours->toggled;
					  r=Parcours_du_menu(1);
					  continue;
				       }
				       parcours->toggled=1-parcours->toggled;
				       if (number>0) {
					  if (courant->suiv==NULL) break;
					  courant=courant->suiv;
				          number--;
					  act_row++;
					  if (number==0) break;
				       } else 
				       if ((number==-4) && (parcours==courant))
				           break;
			           }
				   break;
				 }
      }
    }
  }
  free_text_scroll();
  free(Une_Ligne);
  if (Macro_menu_bef) free(Macro_menu_bef);
  if (Macro_menu_aft) free(Macro_menu_aft);
  return (courant ? courant->lobjet : NULL);
}

int distribue_action_in_menu(char *arg_prev, char *arg_suiv,
	Liste_Menu *debut, Liste_Menu **actuel, Action_on_menu action) {
   int number=0, r=0, res=0, res2=0;
   char *arg=arg_suiv;
   Liste_Menu *parcours;

   if (arg_prev) number=get_menu_signification(arg_prev);
   else if (arg_suiv) {
     number=get_menu_signification(arg_suiv);
     arg=strrchr(arg_suiv,',');
     if (arg==NULL) arg=arg_suiv;
   } else number=1;
   if (arg) {
      while ((*arg) && (isblank(*arg))) arg++;
      if (*arg=='\0') arg=NULL;
   }
   parcours=((number>0) || (number==-5) ? *actuel : debut);
   if (number==-3) r=Parcours_du_menu(0);
   for (;parcours; parcours=parcours->suiv) {
      if (res2==-1) return -1;
      if (res2>res) res=res2;
      if (number==-2) {
          if (parcours->toggled) 
	     res2=action(parcours, arg);
          continue;
      }
      if (number==-3) {
         if (r) 
	   res2=action(parcours,arg);
	 r=Parcours_du_menu(1);
	 continue;
      }
      res2=action(parcours,arg);
      if (number>0) {
	  if (parcours->suiv==NULL) break;
	  number--;
	  if (number==0) {
	    parcours=parcours->suiv;
	    break;
	  }
      } else 
      if ((number==-4) && (parcours==*actuel)) break;
   }
   if (res2==-1) return -1;
   if (res2>res) res=res2;
   if (number>=0) *actuel=parcours;
   return res;
}

/* Ici, on libere un menu alloue... c'est assez facile normalement */
/* ON NE LIBERE QUE LA STRUCTURE EXTERNE DU MENU... SI LES OBJETS */
/* POINTES SONT TEMPORAIRES AUSSI, IL FAUT LES LIBERER AVANT !!! */
void Libere_menu (Liste_Menu *debut) {

   while (debut) {
     if (debut->suiv) {
       debut=debut->suiv;
       free(debut->prec);
     } else {
       free(debut);
       debut=NULL;
     }
  }
}

void Libere_menu_noms (Liste_Menu *debut) {
  Liste_Menu *p=debut;
   while (debut) {
     free(debut->nom);
     debut=debut->suiv;
   }
   Libere_menu(p);
}

/* Ajoute un élément au menu dont base est le dernier élément */
Liste_Menu *ajoute_menu(Liste_Menu *base, char *nom, void *lobjet) {
  Liste_Menu *creation;

  creation=safe_malloc(sizeof(Liste_Menu));
  creation->suiv=NULL;
  creation->prec=base;
  if (base) 
    base->suiv=creation;
  creation->nom=nom;
  creation->lobjet=lobjet;
  creation->order=0;
  return creation;
}

/* Ajoute un élément à un menu dans l'ordre alphabétique... menu est */
/* le premier élément...					     */
/* la comparaison se fait à partir d'un éventuel décalage du nom pour*/
/* permettre de comparer sur des champs différents (oui je sais      */
/* c'est crade...						) pour -1 */
/* on prend order 						     */
/* On renvoie l'élément créé...					     */
Liste_Menu *ajoute_menu_ordre(Liste_Menu **menu, char *nom, void *lobjet, int
				decalage, int order) {
  Liste_Menu *creation, *parcours=*menu;
  int previous=0, by_order=(decalage==-1);

  while (parcours && parcours->suiv) {
    if (by_order==0) {
      if ((strlen(parcours->nom)<decalage) ||
          (strcmp(parcours->nom+decalage,nom+decalage)<0)) 
		  parcours=parcours->suiv; else break;
    } else 
      if (parcours->order<=order) parcours=parcours->suiv; else break;
  }
  if (by_order==0) {
     if (parcours && ((strlen(parcours->nom)<decalage) ||
        (strcmp(parcours->nom+decalage,nom+decalage)<0))) previous=1;
  } else if (parcours && (parcours->order<=order)) previous=1;
  /* previous=1 : on s'est arrete parce que parcours->suiv n'existait pas */
  /* mais on doit mettre nom après parcours */
  creation=safe_malloc(sizeof(Liste_Menu));
  if (previous) {
    creation->prec=parcours; 
    if (parcours) parcours->suiv=creation;
  } else {
     creation->prec=(parcours ? parcours->prec : NULL);
     if (parcours && (parcours->prec)) parcours->prec->suiv=creation;
  }
  if (previous) creation->suiv=NULL; else
  {
     creation->suiv=parcours;
     if (parcours) parcours->prec=creation;
  }
  creation->nom=nom;
  creation->lobjet=lobjet;
  creation->order=order;
  if ((*menu==NULL) || (creation->suiv==(*menu))) *menu=creation;
  return creation;
}
  
void init_Flcmd_menu_rev() {
   int i;
   for (i=0;i<MAX_FL_KEY;i++) Flcmd_menu_rev[i]=FLCMD_MENU_UNDEF;
   for (i=0;i<CMD_DEF_MENU;i++)
         Flcmd_menu_rev[Cmd_Def_Menu[i].key]=Cmd_Def_Menu[i].cmd;
   Flcmd_menu_rev[0]=FLCMD_MENU_UNDEF;
   return;
}

/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee */
/* -4 plus de macro disponible */
int Bind_command_menu(char *nom, int key, char *arg, int add) {
  int i;
  if ((key<1)  || key >= MAX_FL_KEY)
    return -2;
  if (arg ==NULL) {
    if (strncmp(nom, "undef", 5)==0) {
      Flcmd_menu_rev[key]=FLCMD_MENU_UNDEF;
      return 0;
    }
  }
  for (i=0;i<NB_FLCMD_MENU;i++)
    if (strcmp(nom, Flcmds_menu[i])==0) {
      if (Bind_command_new(key,i,arg,CONTEXT_MENU, add)<0) return -4;
      return 0;
    }
  return -1;
}
