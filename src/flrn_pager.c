/* flrn v 0.3                                                           */
/*              flrn_pager.c            22/06/98                        */
/*                                                                      */
/* 	Un VRAI pager pour flrn, un jour...				*/
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#define IN_FLRN_PAGER_C

#include "flrn.h"
#include "options.h"
#include "flrn_pager.h"
#include "flrn_macros.h"
#include "flrn_command.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_slang.h"

/* le tableau touche -> commande */
int *Flcmd_pager_rev = &Flcmd_rev[CONTEXT_PAGER][0];
/* pour les macros */

char pattern_search[SIZE_PATTERN_SEARCH]="";

/* get_new_pattern */
/* retour 0 : bon -1 ou -2 : annulé */
int get_new_pattern() {
   int col, ret;
   char line[SIZE_PATTERN_SEARCH];
   col=Aff_fin("Search : ");
   line[0]='\0';
   ret=getline(line,SIZE_PATTERN_SEARCH-1,Screen_Rows-1,col);
   if ((ret==0) && (*line)) strcpy(pattern_search,line);
   return ret;
}

/* 		Page_message						*/
/* La fonction principale du pager */
/* Init_Scroll_window a déjà été appelé. Si key!=0, c'est la première touche */
/* frappée... Si short_exit=1, en fin de scrolling on donne priorité à une   */
/* commande extérieure, et on sort si un touche */
/* n'est pas reconnue... num_elem est le nombre de lignes à scroll...       */
/* act_row est la première ligne du scroll à afficher, row_deb la première  */
/* ligne dans le cas du premier affichage...				    */
/* si key==0, on affiche le premier écran...				    */
/* Si exit_chars est différent de NULL, tout appui sur une des touches de la */
/* chaine provoque la sortie...	Si il est NULL, on sort toujours en fin de   */
/* message...								     */
/* On renvoie le code de la touche de sortie (-1 si ^C ou FLCMD_PAGER_QUIT). */
/* éventuellement, on renvoie ce code avec MAX_FL_KEY 			     */
/* pour dire qu'on était en fin.					     */
/* On renvoie 0 si on est sorti par fin de l'ecran...			     */
/* (ie short_exit=0 et exit_chars==NULL)				     */

int Page_message (int num_elem, int short_exit, int key, int act_row,
    			int row_deb, char *exit_chars, char *end_mesg,
			int (in_wait)(int)) {
  int percent, nll, deb=1, le_scroll, at_end, to_wait;
  char buf3[15];

  to_wait=(in_wait==NULL ? 0 : 1);
  at_end=(num_elem<Screen_Rows-row_deb);
  while (1) {
    if ((short_exit) && 
        (at_end==1) && (key<MAX_FL_KEY) && (Flcmd_rev[CONTEXT_COMMAND][key]!=FLCMD_UNDEF))
           return (key | MAX_FL_KEY); 
	   	/* note : on ne teste pas '0-9,-<>.', qui ne doivent */
	   	/* PAS être bindés				    */
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
      case FLCMD_PAGER_SEARCH : {
      				  int ret;
				  Do_Scroll_Window(0,deb);
                                  ret=get_new_pattern();
				  if (ret==0) {
      				    ret=New_regexp_scroll (pattern_search);
				    if (ret) 
				         Aff_error_fin("Regexp invalide !",1,1);
				  }
				  deb=1;
      			          le_scroll=Do_Scroll_Window(0,deb);
				     /* pour forcer l'affichage de la ligne */
				}
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
		  		return (key | ((short_exit && at_end) ? 
							MAX_FL_KEY : 0)); 
    }
    if (deb || le_scroll) {
      nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
      if (debug) fprintf(stderr, "%d / %d\n", nll, num_elem);
      if (nll>=num_elem) {
	if ((!short_exit) && (exit_chars==NULL)) return 0; 
	else {
	  if (to_wait) {
	     Aff_fin("Patientez...");
	     Screen_refresh();
	     to_wait=in_wait(1);
	  }
	  if (end_mesg) Aff_fin(end_mesg); else Aff_fin("(100%%)-more-");
	}
	at_end=1;
      } 
      else {
        at_end=0;
	if (to_wait) {
	   Aff_fin("Patientez...");
	   Screen_refresh();
	   to_wait=in_wait(0);
	}
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
int Bind_command_pager(char *nom, int key, char *arg, int add) {
  int i;
  if ((key<1)  || key >= MAX_FL_KEY)
    return -2;
  if (arg ==NULL) {
    if (strncmp(nom, "undef", 5)==0) {
      Flcmd_pager_rev[key]=FLCMD_PAGER_UNDEF;
      return 0;
    }
  }
  for (i=0;i<NB_FLCMD_PAGER;i++)
    if (strcmp(nom, Flcmds_pager[i])==0) {
      if (Bind_command_new(key,i,arg,CONTEXT_PAGER, add)< 0) return -4;
      return 0;
    }
  return -1;
}

