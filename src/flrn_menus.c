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
#include "flrn_format.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* pour les macros */
flrn_char *Macro_menu_bef, *Macro_menu_aft;
int nxt_menu_cmd;

/* Cette fonction est définie dans flrn_pager.c */
/* Je préfère la déclarer en extern ici */
extern int get_new_pattern();

int get_menu_signification(flrn_char *str) {
   int a;
   while ((*str) && (fl_isblank(*str))) str++;
   if (*str==fl_static('\0')) return 1;
   switch (*str) {
         case fl_static('*') : return -1; /* tout */
         case fl_static('.') : return -2; /* selected */
         case fl_static('_') : return -3; /* tout ce qui matche */
         case fl_static('<') : return -4; /* tout ce qui precede */
         case fl_static('>') : return -5; /* tout ce qui suit */
         default :  a=fl_strtol(str,NULL,10);
                    return (a<1 ? 1 : a);
   }
}


/* retour : -4 : CONTEXT_COMMAND */
int get_command_menu(int with_command, int *number)  {
   int res, res2=-1, bef=0, num_macro=0;

   if (nxt_menu_cmd==-1) {
     res=get_command(0,CONTEXT_MENU, (with_command ? CONTEXT_COMMAND : -1), &une_commande);
     if (une_commande.len_desc>0) {
	 free(une_commande.description);
	 une_commande.len_desc=0;
     }
     if ((res==0) && ((res2=une_commande.cmd[CONTEXT_MENU]) & FLCMD_MACRO)) {
         num_macro=res2 ^ FLCMD_MACRO;
	 if (Flcmd_macro[num_macro].next_cmd!=-1) {
           if (une_commande.before) 
	      Macro_menu_bef=safe_flstrdup(une_commande.before);
           if (une_commande.after) 
	      Macro_menu_aft=safe_flstrdup(une_commande.after);
	 }
     }
   } else {
     res=0;
     if (Macro_menu_bef) une_commande.before=safe_flstrdup(Macro_menu_bef);
     else une_commande.before=NULL;
     if (Macro_menu_aft) une_commande.after=safe_flstrdup(Macro_menu_aft);
     else une_commande.after=NULL;
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
	  une_commande.after=safe_flstrdup(Flcmd_macro[num_macro].arg);
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
         flrn_char *buf=Flcmd_macro[num_macro].arg;
	 while ((*buf) && (isblank(*buf))) buf++;
	 une_commande.len_desc=0;
         res=Lit_cmd_explicite(buf, CONTEXT_COMMAND,-1,&une_commande);
	 buf=fl_strchr(buf,fl_static(' '));
	 if (buf!=NULL) {
	   if (une_commande.after) free(une_commande.after);
	   une_commande.after=safe_flstrdup(buf);
	 }
      } else if (une_commande.after) {
         flrn_char *buf=une_commande.after;
	 while ((*buf) && (fl_isblank(*buf))) buf++;
	 une_commande.len_desc=0;
         res=Lit_cmd_explicite(buf,CONTEXT_COMMAND,-1,&une_commande);
	 buf=fl_strchr(buf,fl_static(' '));
	 if (buf==NULL) {
	    free(une_commande.after);
	    une_commande.after=NULL;
	 } else {
	    buf=safe_flstrdup(buf);
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
      fl_strncpy(pattern_search,une_commande.after,SIZE_PATTERN_SEARCH-1);
      pattern_search[SIZE_PATTERN_SEARCH-1]=fl_static('\0');
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
	      int (action)(void *,flrn_char **),
	      int (action_select)(Liste_Menu *, Liste_Menu **,
		  flrn_char **,  int *, Cmd_return *),
	      flrn_char *chaine) 
{
  int act_row=1+Options.skip_line, last_act_row, num_elem=0, number=1;
  int correct=0; /* Pour savoir si on s'arrete ou pas... */
  Liste_Menu *courant=actuel, *parcours=debut_menu;
  flrn_char *Une_Ligne=NULL;
  int tofree;
  
  Macro_menu_bef=NULL; Macro_menu_aft=NULL; nxt_menu_cmd=-1;
  if (parcours==NULL) return NULL; /* Ouais ouais ouais... */
  
  /* Le scrolling est, on suppose, déjà créé */
#if 0
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
#else
  /* mais on doit quand même trouver act_row */
  if (courant) {
     while (parcours) {
	 if (parcours==courant) break;
	 act_row++;
	 parcours=parcours->suiv;
     }
  } else courant=debut_menu;
#endif

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
	 tofree=action(courant->lobjet,&Une_Ligne);
	 if (Une_Ligne) {
	     int rc;
	     char *trad;
	     rc=conversion_to_terminal(Une_Ligne, &trad, 0, (size_t)(-1));
	     Screen_write_string(trad);
	     if (tofree) free(Une_Ligne);
	     if (rc==0) free(trad);
	 }
	 Screen_erase_eol();
      } else if (!no_change_last_line) {
	 Cursor_gotorc(Screen_Rows-2,0);
	 Screen_erase_eol();
      }
      parcours=courant;
      for (deb=act_row;deb<Screen_Rows-2-Options.skip_line;deb++) {
	 if (parcours==NULL) break;
	 Cursor_gotorc(deb,1);
	 Screen_write_char((parcours->laligne.flags & 2) ? '*' : ' ');
	 parcours=parcours->suiv;
      }
      parcours=courant->prec;
      for (deb=act_row-1;deb>Options.skip_line;deb--) {
	 if (parcours==NULL) break;
	 Cursor_gotorc(deb,1);
	 Screen_write_char((parcours->laligne.flags & 2) ? '*' : ' ');
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
	     /* On suppose la libération de la mémoire SI pas FLCMD_MENU_SELECT
	        du ressort d'action_select */
	     res=action_select (debut_menu,&courant,&Une_Ligne, &tofree,
		     &une_commande);
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
		 /* c'est maintenant du ressort d'action_select de modifier */
#if 0
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
		 free(Une_Ligne_bis);
#endif
		 Do_Scroll_Window(0,1);
	     }
             if (Une_Ligne) {
                 int rc;
		 char *trad;
		 rc=conversion_to_terminal(Une_Ligne, &trad, 0, (size_t)(-1));
 	         Cursor_gotorc(Screen_Rows-2,0);
		 Screen_write_string(trad);
		 if (tofree) free(Une_Ligne);
		 if (rc==0) free(trad);
		 Screen_erase_eol();
		 no_change_last_line=1;
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
	 case FLCMD_MENU_LUP : while (number) {
				   if (courant->prec!=NULL) {
				       Liste_Menu *c2=courant->prec;
				       if (c2->prec) 
					   c2->prec->suiv=courant;
                                       if (courant->suiv)
					   courant->suiv->prec=c2;
				       c2->suiv=courant->suiv;
				       courant->prec=c2->prec;
				       c2->prec=courant;
				       courant->suiv=c2;
				       Swap_lines(courant->laligne.line,
					       c2->laligne.line);
				       act_row--;
				   } else break;
				   number--;
			       }
			       if ((act_row<1+Options.skip_line) || 
		               (act_row>Screen_Rows-3-Options.skip_line))
		                 p=Do_Scroll_Window(act_row-last_act_row,1);
			       else 
			         p=Do_Scroll_Window(0,1);
		               act_row-=p;
			       break;
	 case FLCMD_MENU_DOWN : while (number) {
				    if (courant->suiv!=NULL) {
			              act_row++; courant=courant->suiv; }
				    else break;
				    number--;
				}
			        break;
	 case FLCMD_MENU_LDOWN : while (number) {
				   if (courant->suiv!=NULL) {
				       Liste_Menu *c2=courant->suiv;
				       if (c2->suiv) 
					   c2->suiv->prec=courant;
                                       if (courant->prec)
					   courant->prec->suiv=c2;
				       c2->prec=courant->prec;
				       courant->suiv=c2->suiv;
				       c2->suiv=courant;
				       courant->prec=c2;
				       Swap_lines(courant->laligne.line,
					       c2->laligne.line);
				       act_row++;
				   } else break;
				   number--;
			       }
			       if ((act_row<1+Options.skip_line) || 
		               (act_row>Screen_Rows-3-Options.skip_line))
		                 p=Do_Scroll_Window(act_row-last_act_row,1);
			       else 
			         p=Do_Scroll_Window(0,1);
		               act_row-=p;
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
					  parcours->laligne.flags ^= 2;
					  continue;
				       }
				       if (number==-3) {
				          if (r) 
					      parcours->laligne.flags ^= 2;
					  r=Parcours_du_menu(1);
					  continue;
				       }
				       parcours->laligne.flags ^= 2;
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
  if (Macro_menu_bef) free(Macro_menu_bef);
  if (Macro_menu_aft) free(Macro_menu_aft);
  return (courant ? courant->lobjet : NULL);
}

int distribue_action_in_menu(flrn_char *arg_prev, flrn_char *arg_suiv,
	Liste_Menu *debut, Liste_Menu **actuel, Action_on_menu action) {
   int number=0, r=0, res=0, res2=0;
   flrn_char *arg=arg_suiv;
   Liste_Menu *parcours;

   if (arg_prev) number=get_menu_signification(arg_prev);
   else if (arg_suiv) {
     number=get_menu_signification(arg_suiv);
     arg=fl_strrchr(arg_suiv,fl_static(','));
     if (arg==NULL) arg=arg_suiv;
   } else number=1;
   if (arg) {
      while ((*arg) && (fl_isblank(*arg))) arg++;
      if (*arg==fl_static('\0')) arg=NULL;
   }
   parcours=((number>0) || (number==-5) ? *actuel : debut);
   if (number==-3) r=Parcours_du_menu(0);
   for (;parcours; parcours=parcours->suiv) {
      if (res2==-1) return -1;
      if (res2>res) res=res2;
      if (number==-2) {
          if (parcours->laligne.flags & 2) 
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

   free_text_scroll();
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

#if 0
void Libere_menu_noms (Liste_Menu *debut) {
  Liste_Menu *p=debut;
   while (debut) {
     free(debut->nom);
     debut=debut->suiv;
   }
   Libere_menu(p);
}
#endif

void change_menu_line(Liste_Menu *courant, int i, flrn_char *elem) {
    File_Line_Type *line=courant->laligne.line;
    struct format_menu *format=courant->format;
    char *toaff;
    size_t bla;
    Colored_Char_Type *cstr;
    if (format->tab[i].stoaff==0) return;
    toaff=safe_malloc(format->tab[i].stoaff);
    memset(toaff,0,format->tab[i].stoaff);
    if (elem)
      format_flstring(toaff,elem,
	    (format->tab[i].maxwidth==-1 ? Screen_Cols-2-format->tab[i].coldeb
	     : format->tab[i].maxwidth),
	    format->tab[i].stoaff, format->tab[i].justify);
    else toaff[0]=fl_static(' ');
    cstr=cree_chaine_mono(toaff,0,(size_t)(-1),&bla);
    free(toaff);
    Change_line(line,i,elem,cstr,bla);
    free(cstr);
}


static void remplit_ajout_menu(Liste_Menu *creation,
	 struct format_menu *format, flrn_char **lelem, void *lobjet,
	 int place) {
  File_Line_Type *line=NULL;
  char *toaff;
  int i;
  size_t reallen;
  creation->lobjet=lobjet;
  creation->format=format;
  creation->laligne.flags=0;
  for (i=0;i<format->nbelem;i++) {
      if (format->tab[i].stoaff) reallen=format->tab[i].stoaff;
      else reallen=(fl_strlen(lelem[i])+1)*4;
      toaff=safe_malloc(reallen);
      format_flstring(toaff,lelem[i],
	      ( format->tab[i].maxwidth == -1 ? 
		  Screen_Cols-1-format->tab[i].coldeb :
		  format->tab[i].maxwidth),
	      reallen, format->tab[i].justify);
      line=Ajoute_formated_line
	  (lelem[i],toaff,format->tab[i].sbase,format->tab[i].stoaff,
	   ((format->tab[i].flags & 4)==0),format->tab[i].coldeb,0,place,line);
      free(toaff);
  }
  creation->laligne.line=line;
}


/* Ajoute un élément au menu dont base est le dernier élément */
Liste_Menu *ajoute_menu(Liste_Menu *base, struct format_menu *format,
	flrn_char **lelem, void *lobjet) {
  Liste_Menu *creation;

  creation=safe_malloc(sizeof(Liste_Menu));
  creation->suiv=NULL;
  creation->prec=base;
  creation->order=0;
  if (base) 
    base->suiv=creation;
  remplit_ajout_menu(creation,format,lelem,lobjet,-1);
  return creation;
}

static int comp_with_decalage (Liste_Menu *parcours, flrn_char *elem,
	int decalage) {
    Line_Elem_Type *bla;
    int i=0;
    if (parcours->format->nbelem<decalage) return -1;
    bla=parcours->laligne.line->line;
    while (bla && (i<decalage)) {
	bla=bla->next;
	i++;
    }
    if ((bla==NULL) || (bla->data_base==NULL)) return -1;
    return fl_strcmp(bla->data_base,elem);
}

/* Ajoute un élément à un menu dans l'ordre alphabétique... menu est */
/* le premier élément...					     */
/* la comparaison se fait à partir d'un éventuel décalage du nom pour*/
/* permettre de comparer sur des champs différents                   */
/* pour -1  on prend order                                           */
/* On renvoie l'élément créé...					     */
Liste_Menu *ajoute_menu_ordre(Liste_Menu **menu, struct format_menu *format,
	flrn_char **lelem, void *lobjet, int decalage, int order) {
  Liste_Menu *creation, *parcours=*menu;
  int previous, by_order=(decalage==-1);
  int place=0;

  if (format->nbelem<decalage) /* BUG ! */ 
      by_order=1;
  while (parcours && parcours->suiv) {
    if (by_order==0) {
      if (comp_with_decalage(parcours,lelem[decalage],decalage)<0)
		  parcours=parcours->suiv; else break;
    } else 
      if (parcours->order<=order) parcours=parcours->suiv; else break;
    place++;
  }
  if (by_order==0) 
     previous=(parcours && 
	     (comp_with_decalage(parcours,lelem[decalage],decalage)<0));
  else previous=(parcours && (parcours->order<=order));
  /* previous=1 : on s'est arrete parce que parcours->suiv n'existait pas */
  /* mais on doit mettre nom après parcours */
  if (previous) place++;
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
  creation->order=order;
  remplit_ajout_menu(creation, format, lelem, lobjet, place);
  if ((*menu==NULL) || (creation->suiv==(*menu))) *menu=creation;
  return creation;
}
  
void init_Flcmd_menu_rev() {
   int i;
   for (i=0;i<CMD_DEF_MENU;i++)
       if (Cmd_Def_Menu[i].key)
          add_cmd_for_slang_key(CONTEXT_MENU,Cmd_Def_Menu[i].cmd,
	       Cmd_Def_Menu[i].key);
   return;
}

/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee */
/* -4 plus de macro disponible */
int Bind_command_menu(flrn_char *nom, struct key_entry *key,
	flrn_char *arg, int add) {
  int res;
  Cmd_return commande;

  memset (&commande,0,sizeof(Cmd_return)); /* pas forcément nécessaire */
#ifdef USE_SLANG_LANGUAGE
  commande.fun_slang=NULL;
#endif
  if (key==NULL) return -2;
  if (arg ==NULL) {
    if (fl_strncmp(nom, fl_static("undef"), 5)==0) {
	del_cmd_for_key(CONTEXT_MENU, key);
	return 0;
	/* FIXME : éventuellement faire un meilleur retour */
    }
  }
  commande.len_desc=0;
  res=Lit_cmd_explicite(nom, CONTEXT_MENU, -1, &commande);
  if (res==-1) return -1;
#ifdef USE_SLANG_LANGUAGE
    res=Bind_command_new(key, commande.cmd[CONTEXT_MENU], arg,
       commande.fun_slang, CONTEXT_MENU, add);
  if (commande.fun_slang) free(commande.fun_slang);
#else
    res=Bind_command_new(key, commande.cmd[CONTEXT_MENU], arg,
       CONTEXT_MENU, add);
#endif
  return (res<0 ? -4 : 0);
}
