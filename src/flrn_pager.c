/* flrn v 0.3                                                           */
/*              flrn_pager.c            22/06/98                        */
/*                                                                      */
/* 	Un VRAI pager pour flrn, un jour...				*/
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "flrn.h"
#include "options.h"
#include "flrn_pager.h"

/* le tableau touche -> commande */
int Flcmd_pager_rev[MAX_FL_KEY];
/* pour les macros */

#define MAX_FL_MACRO_PAGER 32 /* Pas exagerer non plus */
struct {
  int cmd;
  char *arg;
} Flcmd_macro_pager[MAX_FL_MACRO_PAGER];

int Flcmd_num_macros_pager=0;


/* 		Page_message						*/
/* La fonction principale du pager */
/* Init_Scroll_window a déjà été appelé. Si key!=0, c'est la première touche */
/* frappée... Si short_exit=1, on sort en fin de scrolling, ou si un touche */
/* n'est pas reconnue... num_elem est le nombre de lignes à scroll...       */
/* act_row est la première ligne du scroll à afficher, row_deb la première  */
/* ligne dans le cas du premier affichage...				    */
/* si key==0, on affiche le premier écran...				    */
/* Si exit_chars est différent de NULL, tout appui sur une des touches de la */
/* chaine provoque la sortie...	Si il est NULL, on sort toujours en fin de   */
/* message...								     */
/* On renvoie le code de la touche de sortie (-1 si ^C ou FLCMD_PAGER_QUIT). */
/* On renvoie 0 si on est sorti par fin de l'ecran...			     */

int Page_message (int num_elem, int short_exit, int key, int act_row,
    			int row_deb, char *exit_chars, char *end_mesg) {
  int percent, nll, deb=1, le_scroll, at_end;
  char buf3[15];

  at_end=(num_elem<Screen_Rows-row_deb);
  while (1) {
    switch (((key<MAX_FL_KEY) && (key>0)) ? Flcmd_pager_rev[key] : FLCMD_PAGER_UNDEF) {
       case FLCMD_PAGER_PGUP : 
	     le_scroll=Do_Scroll_Window(-Screen_Rows+act_row+1,deb); break;
       case FLCMD_PAGER_PGDOWN : 
		 nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
		 le_scroll=Do_Scroll_Window(
		     ((!Options.scroll_after_end) && 
		      (num_elem-nll<=Screen_Rows-(deb ? row_deb : act_row))) ?
		     num_elem-nll :
                     (Screen_Rows- (deb ? row_deb : act_row)-1),deb);
                 break;
      case FLCMD_PAGER_DOWN : le_scroll=Do_Scroll_Window(1,deb);
                 break;
      case FLCMD_PAGER_UP : le_scroll=Do_Scroll_Window(-1,deb);
                 break;
      case FLCMD_PAGER_QUIT : le_scroll=Do_Scroll_Window(0,deb);
					/* pour l'update */
			      /* on fait un cas particulier grotesque :-( */
			      /* mais obligatoire pour l'aide...	  */
			      if (at_end && 
				  (exit_chars && strchr(exit_chars,key)))
				return key;
			      return (short_exit ? -1 : 0);
      default : le_scroll=Do_Scroll_Window(0,deb);
		if (short_exit || (exit_chars && strchr(exit_chars,key))) 
		  		return key; 
    }
    if (deb || le_scroll) {
      nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
      if (debug) fprintf(stderr, "%d / %d\n", nll, num_elem);
      if (nll>=num_elem) {
	if (short_exit || (exit_chars==NULL)) return 0; 
	else {
	  if (end_mesg) Aff_fin(end_mesg); else Aff_fin("(100%%)-more-");
	}
	at_end=1;
      } 
      else {
        at_end=0;
	percent=(nll*100)/(num_elem+1);
        sprintf(buf3,"(%d%%)",percent);
        strcat(buf3,"-More-");
        Aff_fin(buf3);
      }
    }
    deb=0;
    key=Attend_touche();
    if (KeyBoard_Quit) return -1;
  }
}


void init_Flcmd_pager_rev() {
   int i;
   for (i=0;i<MAX_FL_KEY;i++) Flcmd_pager_rev[i]=FLCMD_PAGER_UNDEF;
   for (i=0;i<CMD_DEF_PAGER;i++) 
         Flcmd_pager_rev[Cmd_Def_Pager[i].key]=Cmd_Def_Pager[i].cmd;
   Flcmd_pager_rev[0]=FLCMD_PAGER_UNDEF;
   return;
}


/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee : n'existe pas dans le pager */
/* -4 plus de macro */
int Bind_command_pager(char *nom, int key, char *arg) {
  int i;
  if ((key<1)  || key >= MAX_FL_KEY)
    return -2;
  if (arg ==NULL) {
    if (strncmp(nom, "undef", 5)==0) {
      Flcmd_pager_rev[key]=FLCMD_PAGER_UNDEF;
      return 0;
    }
    for (i=0;i<NB_FLCMD_PAGER;i++)
      if (strcmp(nom, Flcmds_pager[i])==0) {
        Flcmd_pager_rev[key]=i;
        return 0;
      }
  } else {
    if (Flcmd_num_macros_pager == MAX_FL_MACRO_PAGER) return -4;
    for (i=0;i<NB_FLCMD_PAGER;i++)
      if (strcmp(nom, Flcmds_pager[i])==0) {
        Flcmd_macro_pager[Flcmd_num_macros_pager].cmd = i;
        Flcmd_macro_pager[Flcmd_num_macros_pager].arg = safe_strdup(arg);
        Flcmd_pager_rev[key]=Flcmd_num_macros_pager | FLCMD_MACRO_PAGER;
        Flcmd_num_macros_pager++;
        return 0;
      } 
  } 
  return -1;
}

