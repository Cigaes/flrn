/* flrn v 0.1                                                           */
/*              flrn_menus.c        15/05/98                            */
/*                                                                      */
/* Routines de manipulation de menus pour une interface méga-torche.    */
/* C'est très proche des librairies I/O, mais on va essayer de faire    */
/* un truc indépendant desdites librairies.				*/

#define IN_FLRN_MENUS_C

#include <stdlib.h>
#include "flrn.h"
#include "options.h"
#include "flrn_menus.h"
#include "flrn_command.h"
#include "tty_display.h"
#include "flrn_slang.h"
#include "flrn_messages.h"

/* le tableau touche -> commande */
int *Flcmd_menu_rev = &Flcmd_rev[CONTEXT_MENU][0];
/* pour les macros */

/* Cette fonction est définie dans flrn_pager.c */
/* Je préfère la déclarer en extern ici */
extern int get_new_pattern();

/* retour : -4 : CONTEXT_COMMAND */
int get_command_menu(int with_command)  {
   int res, res2;

   res=get_command(0,CONTEXT_MENU, (with_command ? CONTEXT_COMMAND : -1), &une_commande);
   if (res==-1)
      Aff_error_fin(Messages[MES_UNKNOWN_CMD],1,1);
   if (res<0) return res;
   if (res==1) return -4;
   /* Le search */
   if (une_commande.before) free(une_commande.before);
   res2=une_commande.cmd[CONTEXT_MENU];
   if (res2!=FLCMD_MENU_SEARCH) {
      if (une_commande.after) free(une_commande.after);
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
  int act_row=1+Options.skip_line, last_act_row, num_elem=0;
  int correct=0; /* Pour savoir si on s'arrete ou pas... */
  Liste_Menu *courant=actuel, *parcours=debut_menu;
  int avant=1;
  char *Une_Ligne;
  int Une_Ligne_len;
  
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
      deb=0;
    
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
      Cursor_gotorc(act_row,0);
      Screen_write_char('>');
      no_change_last_line=0;
      Aff_fin(chaine);
      res=get_command_menu((action_select!=NULL));
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
      switch (res) {
	 case FLCMD_MENU_UP :  if (courant->prec!=NULL) {
			    act_row--; courant=courant->prec; }
			  break;
	 case FLCMD_MENU_DOWN : if (courant->suiv!=NULL) {
			    act_row++; courant=courant->suiv; }
			   break;
	 case FLCMD_MENU_PGUP : for (p=0;(p<(Screen_Rows-3-2*Options.skip_line))
	 				    && (courant->prec); p++) {
				    act_row--; 
				    courant=courant->prec; }
				break;
	 case FLCMD_MENU_PGDOWN:for (p=0;(p<(Screen_Rows-3-2*Options.skip_line))
	 				    && (courant->suiv); p++) {
				    act_row++; 
				    courant=courant->suiv; }
				break;
	 case FLCMD_MENU_QUIT : courant=NULL;
			   correct=1; break;
	 case FLCMD_MENU_SEARCH : {
                                  int ret;
                                  ret=New_regexp_scroll (pattern_search);
                                  if (ret) 
                                         Aff_error_fin(Messages[MES_REGEXP_BUG],1,1);
                                  Do_Scroll_Window(0,1);
                                     /* pour forcer l'affichage des lignes */
				  if (ret) break;
				  deb=1;
                                } /* Continue */
      case FLCMD_MENU_NXT_SCH : { int ret, le_scroll;
                                   ret=Do_Search(deb,&le_scroll,
				   		act_row-1-Options.skip_line);
                                   if (ret==-1)
                                       Aff_error_fin(Messages[MES_NO_SEARCH],1,1);
                                   else if (ret==-2)
                                       Aff_error_fin(Messages[MES_NO_FOUND],1,1);
				   else if (le_scroll<0) {
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
                                   break;
                                 }

      }
    }
  }
  free_text_scroll();
  free(Une_Ligne);
  return (courant ? courant->lobjet : NULL);
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
  return creation;
}

/* Ajoute un élément à un menu dans l'ordre alphabétique... menu est */
/* le premier élément...					     */
/* la comparaison se fait à partir d'un éventuel décalage du nom pour*/
/* permettre de comparer sur des champs différents (oui je sais      */
/* c'est crade...						)    */
/* On renvoie l'élément créé...					     */
Liste_Menu *ajoute_menu_ordre(Liste_Menu **menu, char *nom, void *lobjet, int
				decalage) {
  Liste_Menu *creation, *parcours=*menu;
  int previous=0;

  while (parcours && parcours->suiv) {
    if ((strlen(parcours->nom)<decalage) ||
        (strcmp(parcours->nom+decalage,nom+decalage)<0)) 
		parcours=parcours->suiv; else break;
  }
  if (parcours && ((strlen(parcours->nom)<decalage) ||
        (strcmp(parcours->nom+decalage,nom+decalage)<0))) previous=1;
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
