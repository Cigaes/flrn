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
#include "flrn_command.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_slang.h"
#include "flrn_messages.h"

/* le tableau touche -> commande */
int *Flcmd_pager_rev = &Flcmd_rev[CONTEXT_PAGER][0];
/* pour les macros */
char *Macro_pager_bef, *Macro_pager_aft;
int nxt_pager_cmd;

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

/* retour : -4 : CONTEXT_COMMAND */
int get_command_pager(int une_touche, int endroit, int cmd, int *number) {
   int res, res2=-1, bef=0, num_macro=0;

   *number=0;
   if (cmd==0) endroit=1;
   if (nxt_pager_cmd==-1) {
     res=get_command(une_touche,(endroit ? CONTEXT_PAGER : CONTEXT_COMMAND),
	   	(cmd ? (endroit ? CONTEXT_COMMAND : CONTEXT_PAGER) : -1), 
		   &une_commande);
     if ((res==(1-endroit)) 
           && ((res2=une_commande.cmd[CONTEXT_PAGER]) & FLCMD_MACRO)) {
	num_macro=res2 ^ FLCMD_MACRO;
	if (Flcmd_macro[num_macro].next_cmd!=-1) {
	  if (une_commande.before) 
	     Macro_pager_bef=safe_strdup(une_commande.before);
	  if (une_commande.after) 
	     Macro_pager_aft=safe_strdup(une_commande.after);
	}
     }
   } else {
     res=1-endroit;
     if (Macro_pager_bef) une_commande.before=safe_strdup(Macro_pager_bef); else
       une_commande.before=NULL;
     if (Macro_pager_aft) une_commande.after=safe_strdup(Macro_pager_aft); else
       une_commande.after=NULL;
     res2=une_commande.cmd[CONTEXT_PAGER]=nxt_pager_cmd;
   }
   /* on autorise les macros pour le CONTEXT_COMMAND, ca ne pose aucun 
      probleme, contrairement a ce qui se passe dans le menu */
   if (res==-1) 
      Aff_error_fin(Messages[MES_UNKNOWN_CMD],1,1);
   if (res<0) {
     if (une_commande.before) free(une_commande.before);
     if (une_commande.after) free(une_commande.after);
     return res;
   }
   if (res==endroit) return -4;
   /* Le search */
   if (res2 & FLCMD_MACRO) {
      num_macro=res2 ^ FLCMD_MACRO;
      nxt_pager_cmd=Flcmd_macro[num_macro].next_cmd;
      res2=Flcmd_macro[num_macro].cmd;
      if (Flcmd_macro[num_macro].arg) {
         if (une_commande.after) free(une_commande.after);
	 une_commande.after=safe_strdup(Flcmd_macro[num_macro].arg);
      }
   } else nxt_pager_cmd=num_macro=-1;
   if (nxt_pager_cmd==-1) {
      if (Macro_pager_bef) free(Macro_pager_bef);
      if (Macro_pager_aft) free(Macro_pager_aft);
      Macro_pager_bef=Macro_pager_aft=NULL;
   }
   if (une_commande.before) {
      bef=1;
      *number=strtol(une_commande.before,NULL,10);
      /* la on ne peut avoir que des nombres */
      free(une_commande.before);
   }
   if (res2!=FLCMD_PAGER_SEARCH) {
      if (une_commande.after) {
         if (!bef) *number=strtol(une_commande.after,NULL,10);
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


/* 		Page_message						*/
/* La fonction principale du pager */
/* Init_Scroll_window a déjà été appelé. Si key!=0, c'est la première touche */
/* frappée... Si short_exit=1, en fin de scrolling on donne priorité à une   */
/* commande extérieure, et une commande extérieure peut provoquer une sortie */
/* num_elem est le nombre de lignes à scroll...       */
/* act_row est la première ligne du scroll à afficher, row_deb la première  */
/* ligne dans le cas du premier affichage...				    */
/* si key==0, on affiche le premier écran...				    */
/* Si exit_chars est différent de NULL, tout appui sur une des touches de la */
/* chaine provoque la sortie...	Si il est NULL, on sort toujours en fin de   */
/* message...								     */
/* retour : exit_chars=NULL 	short_exit=0   -->   -1 si ^C -2: QUIT, ou 0 */
/*	    exit_chars!=NULL	short_exit=0   -->   code de touche, -1,-2   */
/*	    exit_chars=NULL	short_exit=1   --> 0|MAX_FL_KEY->ext, -1,-2  */
/*	    exit_chars!=NULL	short_exit=1   --> 0|MAX_FL_KEY, code| -1,-2 */

int Page_message (int num_elem, int short_exit, int key, int act_row,
    			int row_deb, char *exit_chars, char *end_mesg,
			int (in_wait)(int)) {
  int percent, nll, deb=1, le_scroll, at_end, to_wait, res, number;
  char buf3[15], *buf=NULL;

  Macro_pager_bef=Macro_pager_aft=NULL; nxt_pager_cmd=-1;
  to_wait=(in_wait==NULL ? 0 : 1);
  at_end=(num_elem<Screen_Rows-row_deb);
  while (1) {
    le_scroll=Do_Scroll_Window(0,deb);
    if ((exit_chars) && (strchr(exit_chars,key))) {
        if (Macro_pager_bef) free(Macro_pager_bef);
	if (Macro_pager_aft) free(Macro_pager_aft);
 	return (key | (at_end ? MAX_FL_KEY : 0));
    }
    res=get_command_pager(key,(short_exit) && (at_end==0),short_exit, &number);
    if (number<1) number=1;
    if (res==-4) {
       if (short_exit) return (at_end ? MAX_FL_KEY : 0);
       /* pas besoin de Macro_pager... ici */
    }
    switch (res) {
       case FLCMD_PAGER_PGUP : 
	     le_scroll=Do_Scroll_Window(number*(-Screen_Rows+act_row+1),deb); 
	     break;
       case FLCMD_PAGER_PGDOWN : 
		 nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
		 le_scroll=Do_Scroll_Window(
		     ((!Options.scroll_after_end) && 
		      (num_elem-nll<number*(Screen_Rows-1)
		                    -(deb ? row_deb : act_row)
				    -(number-1)*act_row)) ?
		     num_elem-nll :
                     (number*(Screen_Rows-1)
		      -(deb ? row_deb : act_row)
		      -(number-1)*act_row),deb);
                 break;
      case FLCMD_PAGER_DOWN : le_scroll=Do_Scroll_Window(number,deb);
                 break;
      case FLCMD_PAGER_UP : le_scroll=Do_Scroll_Window(-number,deb);
                 break;
      case FLCMD_PAGER_SEARCH : { int ret;
      				 ret=New_regexp_scroll (pattern_search);
				 deb=1;
				 if (ret) {
				      Aff_error_fin(Messages[MES_REGEXP_BUG],1,1);
				      break;
				  }
				}  /* On continue */
      case FLCMD_PAGER_NXT_SCH : { int ret=0;
      				   while (number) {
      				      ret=Do_Search(deb,&le_scroll,0);
				      if (ret<0) break;
				      number--;
				   }
				   if (ret==-1) 
				       Aff_error_fin(Messages[MES_NO_SEARCH],1,1);
				   else if (ret==-2)
				       Aff_error_fin(Messages[MES_NO_FOUND],1,1);
				   else if (deb || le_scroll) 
				       le_scroll=Do_Scroll_Window(le_scroll,deb);
				   break;
				 }
      case FLCMD_PAGER_QUIT : {
      				if (Macro_pager_bef) free(Macro_pager_bef);
      				if (Macro_pager_aft) free(Macro_pager_aft);
        			return -2;
			      }
    }
    if (deb || le_scroll) {
      nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
      if (nll>=num_elem) {
	if ((!short_exit) && (exit_chars==NULL)) {
	   if (Macro_pager_bef) free(Macro_pager_bef);
      	   if (Macro_pager_aft) free(Macro_pager_aft);
	   return 0; 
	}
	else {
	  if (to_wait) {
	     Aff_fin("Patientez...");
	     Screen_refresh();
	     to_wait=in_wait(1);
	  }
	  if (end_mesg) buf=end_mesg; else buf="(100%%)-more-";
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
	buf=buf3;
      }
    }
    if (buf) Aff_fin(buf);
    deb=0;
    key=Attend_touche();
    if (KeyBoard_Quit) {
       if (Macro_pager_bef) free(Macro_pager_bef);
       if (Macro_pager_aft) free(Macro_pager_aft);
       return -1;
    }
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

int ajoute_pager(char *ligne, int row) {
   if (row<Screen_Rows-1) {
     Cursor_gotorc(row,0);
     Screen_write_string(ligne);
     Screen_erase_eol();
   } else if (row==Screen_Rows-1) {
      Aff_fin("Patientez...");
      Screen_refresh();
   }
   Ajoute_line(ligne);
   row++;
   return row;
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

