/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_pager.c : pager pour flrn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

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
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* pour les macros */
char *Macro_pager_bef, *Macro_pager_aft;
int nxt_pager_cmd;

flrn_char pattern_search[SIZE_PATTERN_SEARCH]=fl_static("");

/* get_new_pattern */
/* retour 0 : bon -1 ou -2 : annulé */
int get_new_pattern() {
   int col, ret;
   const char *special;
   flrn_char line[SIZE_PATTERN_SEARCH];
   char affline[SIZE_PATTERN_SEARCH];
   special=_("Motif recherchÃ© : ");
   col=Aff_fin_utf8(special);
   line[0]=fl_static('\0');
   affline[0]='\0';
   ret=flrn_getline(line,SIZE_PATTERN_SEARCH-1,
	   affline,SIZE_PATTERN_SEARCH-1,Screen_Rows2-1,col);
   if ((ret==0) && (*line)) fl_strcpy(pattern_search,line);
   return ret;
}

/* retour : -4 : CONTEXT_COMMAND */
int get_command_pager(struct key_entry *une_touche,
	int endroit, int cmd, int *number) {
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
	     Macro_pager_bef=safe_flstrdup(une_commande.before);
	  if (une_commande.after) 
	     Macro_pager_aft=safe_flstrdup(une_commande.after);
	}
     }
   } else {
     res=1-endroit;
     if (Macro_pager_bef) une_commande.before=safe_flstrdup(Macro_pager_bef);
     else une_commande.before=NULL;
     if (Macro_pager_aft) une_commande.after=safe_flstrdup(Macro_pager_aft);
     else une_commande.after=NULL;
     res2=une_commande.cmd[CONTEXT_PAGER]=nxt_pager_cmd;
   }
   /* on autorise les macros pour le CONTEXT_COMMAND, ca ne pose aucun 
      probleme, contrairement a ce qui se passe dans le menu */
   if (res==-1) {
      if (une_commande.cmd_ret_flags & CMD_RET_KEEP_DESC)
	  save_command(&une_commande);
      /* FIXME  : conversion ? */
      Aff_error_fin_utf8(_(Messages[MES_UNKNOWN_CMD]),1,1);
   }
   if (res<0) {
     if (une_commande.before) free(une_commande.before);
     if (une_commande.after) free(une_commande.after);
     return res;
   }
   if (res==endroit) return -4;
   if (une_commande.len_desc>0) {
       free(une_commande.description);
       une_commande.len_desc=0;
   }
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
      *number=fl_strtol(une_commande.before,NULL,10);
      /* la on ne peut avoir que des nombres */
      free(une_commande.before);
   }
   if (res2!=FLCMD_PAGER_SEARCH) {
      if (une_commande.after) {
         if (!bef) *number=fl_strtol(une_commande.after,NULL,10);
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


/* 		Page_message						*/
/* La fonction principale du pager */
/* Init_Scroll_window a déjà été appelé. Si key!=NULL, c'est la first touche */
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

int Page_message (int num_elem, int short_exit, struct key_entry *key,
	int act_row, int row_deb, char *exit_chars, flrn_char *end_mesg,
	int (in_wait)(int), struct key_entry *keyr) {
  int percent, nll, deb=1, le_scroll, at_end, to_wait, res, number;
  flrn_char buf3[15], *buf=NULL;
  struct key_entry ke;
  int slkey;

  ke.entry=ENTRY_ERROR_KEY;
  Macro_pager_bef=Macro_pager_aft=NULL; nxt_pager_cmd=-1;
  to_wait=(in_wait==NULL ? 0 : 1);
  at_end=(num_elem<Screen_Rows-row_deb);
  while (1) {
    le_scroll=Do_Scroll_Window(0,deb);
    if ((key) && (key->entry==ENTRY_SLANG_KEY)) slkey=key->value.slang_key;
    else slkey=0;
    if ((exit_chars) && (strchr(exit_chars,slkey))) {
        if (Macro_pager_bef) free(Macro_pager_bef);
	if (Macro_pager_aft) free(Macro_pager_aft);
	if (keyr) memcpy(keyr,key,sizeof(struct key_entry));
 	return (at_end ? MAX_FL_KEY : 0);
    }
    res=get_command_pager(key,(short_exit) && (at_end==0),short_exit, &number);
    if (number<1) number=1;
    if (res==-4) {
       Free_key_entry(key);
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
				      Aff_error_fin_utf8(_(Messages[MES_REGEXP_BUG]),1,1);
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
				       Aff_error_fin_utf8(_(Messages[MES_NO_SEARCH]),1,1);
				   else if (ret==-2)
				       Aff_error_fin_utf8(_(Messages[MES_NO_FOUND]),1,1);
				   else if (deb || le_scroll) 
				       le_scroll=Do_Scroll_Window(le_scroll,deb);
				   break;
				 }
      case FLCMD_PAGER_QUIT : {
      				if (Macro_pager_bef) free(Macro_pager_bef);
      				if (Macro_pager_aft) free(Macro_pager_aft);
                                Free_key_entry(key);
        			return -2;
			      }
    }
    if (deb || le_scroll) {
      nll=Number_current_line_scroll()+Screen_Rows-act_row-2;
      if (nll>=num_elem) {
        num_help_line=(short_exit ? 3 : 4);
        Aff_help_line(Screen_Rows-1);
	if ((!short_exit) && (exit_chars==NULL)) {
	   if (Macro_pager_bef) free(Macro_pager_bef);
      	   if (Macro_pager_aft) free(Macro_pager_aft);
           Free_key_entry(key);
	   return 0; 
	}
	else {
	  if (to_wait) {
	     to_wait=in_wait(1);
	  }
	  if (end_mesg) buf=end_mesg; else buf=fl_static("(100%%)-more-");
	}
	at_end=1;
      } 
      else {
        num_help_line=(((exit_chars==NULL) && (short_exit)) ? 2 : 4);
        Aff_help_line(Screen_Rows-1);
        at_end=0;
	if (to_wait) {
	   to_wait=in_wait(0);
	}
	percent=(nll*100)/(num_elem+1);
        fl_snprintf(buf3,9,"(%d%%)",percent);
        fl_strcat(buf3,"-More-");
	buf=buf3;
      }
    }
    if (buf) Aff_fin(buf);
    deb=0;
    if (key==NULL) key=&ke;
    Attend_touche(key);
    if (KeyBoard_Quit) {
       if (Macro_pager_bef) free(Macro_pager_bef);
       if (Macro_pager_aft) free(Macro_pager_aft);
       Free_key_entry(key);
       return -1;
    }
  }
}


void init_Flcmd_pager_rev() {
   int i;
   for (i=0;i<CMD_DEF_PAGER;i++)
       if (Cmd_Def_Pager[i].key)
          add_cmd_for_slang_key(CONTEXT_PAGER,Cmd_Def_Pager[i].cmd,
	       Cmd_Def_Pager[i].key);
   return;
}

/* FIXME : Cette fonction ne semble pas utilisée */
int ajoute_pager(flrn_char *ligne, int row) {
   int rc;
   char *affligne;
   rc=conversion_to_terminal(ligne,&affligne,0,(size_t)(-1));
   if (row<Screen_Rows-1) {
     Cursor_gotorc(row,0);
     Screen_write_string(affligne);
     Screen_erase_eol();
   } else if (row==Screen_Rows-1) {
      const char *special;
      special=_("Patientez....");
      Aff_fin_utf8(special);
      Screen_refresh();
   }
   Ajoute_formated_line(ligne,affligne,0,0,0,0,0,-1,NULL);
   row++;
   if (rc==0) free(affligne);
   return row;
}


/* -1 commande non trouvee */
/* -2 touche invalide */
/* -3 touche verrouillee : n'existe pas dans le pager */
/* -4 plus de macro */
int Bind_command_pager(flrn_char *nom, struct key_entry *key,
	flrn_char *arg, int add) {
  int res;
  Cmd_return commande;

  memset (&commande,0,sizeof(Cmd_return)); /* pas forcément nécessaire */
#ifdef USE_SLANG_LANGUAGE
  commande.fun_slang=NULL;
#endif
  if (key==NULL)  return -2;
  if (arg ==NULL) {
    if (fl_strncmp(nom, fl_static("undef"), 5)==0) {
	del_cmd_for_key(CONTEXT_PAGER, key);
	return 0;
	/* FIXME : éventuellement faire un meilleur retour */
    }
  }
  commande.len_desc=0;
  res=Lit_cmd_explicite(nom, CONTEXT_PAGER, -1, &commande);
  if (res==-1) return -1;
#ifdef USE_SLANG_LANGUAGE
  res=Bind_command_new(key, commande.cmd[CONTEXT_PAGER], arg,
      commande.fun_slang, CONTEXT_PAGER, add);
  if (commande.fun_slang) free(commande.fun_slang);
#else
  res=Bind_command_new(key, commande.cmd[CONTEXT_PAGER], arg,
     CONTEXT_PAGER, add);
#endif
  return (res<0 ? -4 : 0);
}
