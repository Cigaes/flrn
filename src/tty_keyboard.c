/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      tty_keyboard.c : gestion du clavier
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "art_group.h"
#include "flrn_format.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* Gestion élémentaire des signaux */
/* Je me base sur ce que fait slrn, parce que je comprend pas grand chose */
void sigtstp_handler(int sig) {
    sigset_t mask;
  
    sigemptyset (&mask);
    sigaddset (&mask, SIGTSTP);
/*    SLsig_block_signals ();  */
    Reset_keyboard();
    Screen_suspend();
/*    SLsig_unblock_signals ();  */

    /*SL*/signal(SIGTSTP, SIG_DFL);
    kill (getpid (), SIGTSTP);
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
    
    Screen_resume();
    Put_tty_single_input(-1,0,1);
    /*SL*/signal(SIGTSTP, sigtstp_handler);
    React_suspend_char(1);
    screen_changed_size();
    return;
}

void sigterm_handler(int sig) {
   Reset_screen();
   Reset_keyboard();
   fprintf(stderr, "Killed by SIGTERM\n");
   exit (sig);
}

extern void save_and_free_all(int);

void sighup_handler(int sig) {
   /* L'idée est qu'un sighup sauve et quitte, sans faire aucune
      entrée-sortie clavier-écran */
   if (Newsgroup_courant && Article_deb && (Article_deb!=&Article_bidon))
      detruit_liste(0);
   save_and_free_all(1);
   exit(sig);
}

static void init_signals (void) {
#ifdef SIGTSTP
    /*SL*/signal (SIGTSTP, sigtstp_handler);
#endif
#ifdef SIGTERM
    /*SL*/signal (SIGTERM, sigterm_handler);
#endif
#ifdef SIGHUP
    /*SL*/signal (SIGHUP, sighup_handler);
    /* init_signals est appelé après la création des groupes, etc... */
#endif
}


/* Initialisation du clavier. On suppose que SLtt_get_terminfo a déjà 	*/
/* été lancé (appel à Init_screen).					*/
int Init_keyboard(int first) {
   int res;

/*   SLsig_block_signals (); */

   if (first) {
     res=init_clavier();
     if (res<0) {
        fprintf(stderr, "Echec dans le lancement des routines clavier\n");
/*      SLsig_unblock_signals ();  */
        return -1;
     }
   }
   
   init_signals ();  /* Signaux (de base) */

   res=Put_tty_single_input (-1, 0, 1);
   if (res<0) { 
      fprintf(stderr, "Echec dans l'initialisation du clavier...\n");
/*      SLsig_unblock_signals ();  */
      return -1;
   }
   React_suspend_char(1);  /* Cas du ^Z */

/*   SLsig_unblock_signals ();  */
   Set_Abort_Signal(NULL);
   Set_Ignore_User_Abort(1);
   return 0;
} 

/* libération d'une key_entry */
void Free_key_entry(struct key_entry *ke) {
    if (ke==NULL) return;
    if (ke->entry==ENTRY_ALLOCATED_STRING) free(ke->value.allocstr);
    ke->entry=ENTRY_ERROR_KEY;
}

int key_are_equal(struct key_entry *key1, struct key_entry *key2) {
    if (key1->entry==ENTRY_SLANG_KEY)
        return ((key2->entry==ENTRY_SLANG_KEY) &&
                (key1->value.slang_key==key2->value.slang_key));
    if (key1->entry==ENTRY_STATIC_STRING)
        return ((key2->entry==ENTRY_STATIC_STRING) &&
                (fl_strncmp(key1->value.ststr,key2->value.ststr,4)==0));
    return (fl_strcmp(key1->value.allocstr,key2->value.allocstr)==0);
}

const flrn_char *get_name_key(struct key_entry *key, int flag) {
    if (key==NULL) return NULL;
    if (key->entry==ENTRY_ERROR_KEY) return NULL;
    if (key->entry==ENTRY_SLANG_KEY) 
	return get_name_char(key->value.slang_key,flag);
    if (key->entry==ENTRY_STATIC_STRING) 
	return key->value.ststr;
    return key->value.allocstr;
}

/* Attente d'une touche clavier... Renvoie le code de la touche.	*/
/* Cette fontion remet a zéro la variable SLKeyBoard_Quit.		*/
/* On fait un refresh automatique si key=^L, et on reprend attend-touche*/
/* si ke==NULL, on cherche juste à savoir s'il y a un ^C */
void Attend_touche(struct key_entry *ke) {
   int key;
   unsigned char entree[10];
   size_t entree_len=0;

   memset(entree,0,10);
   if (ke && (ke->entry==ENTRY_ALLOCATED_STRING)) free(ke->value.allocstr);
   if (error_fin_displayed==0)
      Screen_refresh(); /* Quasiment le seul endroit où on le met */
   error_fin_displayed=0; /* en particulier, ^L élimine le message d'erreur */
   KeyBoard_Quit=0;
   if (ke) ke->entry=ENTRY_SLANG_KEY;
   /* ^L n'est pas une commande autre que refresh */
   while(1) {
     while((key=Keyboard_getkey())==12) {
        Screen_touch_lines (0, Screen_Rows2);
        Screen_refresh();
     }
     if (key==8) key=FL_KEY_BACKSPACE;
     if (key=='\177') key=FL_KEY_DELETE;
     if (key==FL_KEY_DELETE) key=FL_KEY_BACKSPACE;
     if (key==0xFFFF) {
	 if (ke) ke->entry=ENTRY_ERROR_KEY;
	 return;
     }
     if ((key<0x80) || (key>=0x100)) {
	 if (entree_len!=0) {
             if (ke) ke->entry=ENTRY_ERROR_KEY;
	     return;
	 }
	 if (ke) ke->value.slang_key=key;
	 return;
     }
     if ((key<0x100) && (key>=0x80)) {
	 int res;
	 flrn_char *bla;
	 if (entree_len>=10) {
	     if (ke) ke->entry=ENTRY_ERROR_KEY;
	     return;
	 }
	 entree[entree_len++]=(unsigned char)key;
	 if (ke) {
	     bla=ke->value.ststr;
	     memset(bla,0,4);
	 }
	 res=convert_termchar(entree,entree_len,(ke ? &bla : NULL),
		 (ke ? 4 : 0)); 
	    /* 0 : non  >=0 : ok, donne la longueur  -1 : mauvais */
	    /* si trop long, on change bla */
	    /* si on passe NULL, on ne cherche pas à retourner quelque chose */
	 if (res<0) {
	     if (ke) ke->entry=ENTRY_ERROR_KEY;
	     return;
	 }
	 if (res==0) continue;
	 if (res<=4) {
	     if (ke) ke->entry=ENTRY_STATIC_STRING;
	     return;
	 }
	 if (ke) {
	     ke->entry=ENTRY_ALLOCATED_STRING;
	     ke->value.allocstr=bla;
	 }
	 return;
     }
  }
}

size_t parse_key_string (flrn_char *buf, struct key_entry *key) {
    size_t i;
    i=next_flch(buf,0);
    if ((int)i<=0) {
	key->entry=ENTRY_ERROR_KEY;
	return 0;
    }
    if ((i==1) && (*buf>>7==0)) { 
	key->entry=ENTRY_SLANG_KEY;
	key->value.slang_key=(int)*buf;
    } else if (i<4) {
	key->entry=ENTRY_STATIC_STRING;
	memset(key->value.ststr,0,sizeof(key->value.ststr));
	fl_strncpy(key->value.ststr,buf,i);
    } else {
	key->entry=ENTRY_ALLOCATED_STRING;
	key->value.allocstr=safe_calloc(i+1,sizeof(flrn_char));
	fl_strncpy(key->value.allocstr,buf,i);
    }
    return i;
}

/* Prend une ligne. Renvoie -1 si ^C, -2 avec backspace en debut de ligne */
/* -3 si plus de place */
/* 0 sans retour, 1 avec.						*/
/* buf DOIT etre initialise en le debut de la ligne...			*/
/* renvoie qqchose >0 si on a appuye sur une des touches magiques et */
/*                                                           flag & 1=0 */
/* le code de la touche si ce n'est pas une touche magique et flag & 1 */
/* si non-ascii et flag=2, renvoie le code de la touche dans key_returned
 * si nécessaire */
int magic_flrn_getline(flrn_char *buf, size_t buffsize,
	char *termbuf, size_t termbufsize, int row, int col,
	char *magic, int flag, struct key_entry *key_pressed,
	struct key_entry *key_returned)
{
   size_t place,placeact,sprevflch;
   size_t tplace,tplaceact,tpdeb=0,temp,ptplaceact;
   int colact,colini=col,coldec=0;
   char *ptr;
   struct key_entry key;

   place=placeact=fl_strlen(buf);
   tplace=tplaceact=strlen(termbuf);
   col+=str_estime_width(termbuf,col,(size_t)-1);
   colact=col;
   if (col>=Screen_Cols-1) {
       coldec=((col-Screen_Cols+1)/(4*Screen_Tab_Width)+1)
	   *(4*Screen_Tab_Width);
       col=colini;
       tpdeb=to_make_width(termbuf,coldec,&col,0);
       col=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
       colact=col;
   }
   Cursor_gotorc(row,colini);
   Screen_write_string(termbuf+tpdeb);

   memset(&key,0,sizeof(struct key_entry));
   do
   {
     buf[place]=fl_static('\0');
     termbuf[tplace]='\0';
     Cursor_gotorc(row,colact);
     if (key_pressed) {
       memcpy(&key,key_pressed,sizeof(struct key_entry));
       key_pressed=NULL;
     } else Attend_touche(&key);
     /* if (key=='\r') key='\n'; */
     if (KeyBoard_Quit) return -1; /* on n'a rien fait... */
     if ((key.entry==ENTRY_SLANG_KEY) &&
         ((key.value.slang_key==FL_KEY_BACKSPACE) ||
	  (key.value.slang_key==21) ||
	  (key.value.slang_key==23))) {
        int mot_trouve=0;
	size_t retour=0;
        if (place==0) return -2;
	if (placeact==0) continue;
	do {
	  sprevflch=previous_flch(buf,placeact,0);
	  if ((int)sprevflch<0) sprevflch=1;
	  retour+=sprevflch;
	  placeact-=sprevflch;
	  if (placeact==0) break;
	  if (key.value.slang_key==FL_KEY_BACKSPACE) break;
	  if (!fl_isblank(buf[placeact])) mot_trouve=1;
	  if ((key.value.slang_key==23) && (mot_trouve) &&
		  (isblank(buf[placeact-1]))) break;
	} while (1);
	/* on s'est contenté, pour l'instant, de modifier placeact */
	/* à présent, il faut changer le reste */
	place-=retour;
	memmove(buf+placeact,buf+placeact+retour,
		   (place-placeact+1)*sizeof(flrn_char));
	col=colini;
	conversion_to_terminal(buf,&termbuf,termbufsize-1,placeact);
	tplaceact=strlen(termbuf);
	if (tpdeb && (tplaceact<=tpdeb+1)) {
	    colact=colini+str_estime_width(termbuf,colini,(size_t)-1);
            if (colact>=Screen_Cols-1) {
               coldec=((colact-Screen_Cols+1)/(4*Screen_Tab_Width)+1)
	           *(4*Screen_Tab_Width);
               colact=colini;
               tpdeb=to_make_width(termbuf,coldec,&colact,0);
               colact=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
	    }
	} else colact=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
	ptr=termbuf+tplaceact;
	conversion_to_terminal(buf+placeact,&ptr,termbufsize-tplaceact-1,
		(size_t)(-1));
	tplace=tplaceact+strlen(ptr);
	Cursor_gotorc(row,colini);
	col=colini;
	temp=to_make_width(termbuf+tpdeb,Screen_Cols-colini,&col,0);
	Screen_write_nstring(termbuf+tpdeb,temp);
	col=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
	Screen_erase_eol();
        continue;
     }
     if ((key.entry==ENTRY_SLANG_KEY) && (key.value.slang_key==FL_KEY_LEFT)) {
	 if (placeact>0) {
	     char *bla;
	     int resconv;
	     sprevflch=previous_flch(buf,placeact,0);
	     if ((int)sprevflch<0) sprevflch=1;
	     resconv=conversion_to_terminal(buf+placeact-sprevflch,&bla,0,
		     sprevflch);
	     tplaceact-=strlen(bla);
	     placeact-=sprevflch;
	     if (resconv==0) free(bla);
	     if (tpdeb && (tplaceact<=tpdeb+1)) {
		 colact=colini+str_estime_width(termbuf,colini,tplaceact);
		 if (colact>=Screen_Cols-1) {
		     coldec=((colact-Screen_Cols+1)/(4*Screen_Tab_Width)+1)
			 *(4*Screen_Tab_Width);
		     colact=colini;
		     tpdeb=to_make_width(termbuf,coldec,&colact,0);
		     colact=colini+str_estime_width(termbuf+tpdeb,colini,
			     placeact);
		     resconv=colini;
		     Cursor_gotorc(row,colini);
		     temp=to_make_width(termbuf+tpdeb,Screen_Cols-colini,
			     &resconv,0);
		     Screen_write_nstring(termbuf+tpdeb,temp);
		     Screen_erase_eol();
		 }
	     } else {
		 colact=colini+str_estime_width(termbuf+tpdeb,colini,tplaceact);
	     }
	 }
	 continue;
     }
     if ((key.entry==ENTRY_SLANG_KEY) && (key.value.slang_key==FL_KEY_RIGHT)) {
	 if (placeact<place) {
	     char *bla;
	     int resconv;
	     ptplaceact=tplaceact;
	     sprevflch=next_flch(buf,placeact);
	     if ((int)sprevflch<=0) sprevflch=1;
	     placeact+=sprevflch;
	     resconv=conversion_to_terminal(buf+placeact-sprevflch,&bla,0,
		     sprevflch);
	     tplaceact+=strlen(bla);
	     if (resconv==0) free(bla);
	     colact+=str_estime_width(termbuf+ptplaceact,colact,
		      tplaceact-ptplaceact);
	     if (colact>=Screen_Cols-1) {
		   coldec=((colact-Screen_Cols+1)/(4*Screen_Tab_Width)+1)
			 *(4*Screen_Tab_Width);
		   colact=colini;
		   tpdeb=to_make_width(termbuf,coldec,&colact,0);
		   colact=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
		   resconv=colini;
		   Cursor_gotorc(row,colini);
		   temp=to_make_width(termbuf+tpdeb,Screen_Cols-colini,
			     &resconv,0);
		   Screen_write_nstring(termbuf+tpdeb,temp);
		   Screen_erase_eol();
	     }
	 }
	 continue;
     }
     if ((flag & 2) && 
	     ((key.entry==ENTRY_SLANG_KEY)
	      && (key.value.slang_key>0xFF))) {
	 memcpy(key_returned,&key,sizeof(struct key_entry));
	 return 1;
     }
     if (!(flag & 1)) {
        if ((key.entry==ENTRY_SLANG_KEY) &&
		((ptr=strchr(magic,key.value.slang_key))!=NULL)) {
          return 1+ptr-magic;
        }
     } else if ((key.entry!=ENTRY_SLANG_KEY) ||
	     ((ptr=strchr(magic,key.value.slang_key))==NULL)) {
	  if (key_returned==NULL) {
	      Free_key_entry(&key);
	      return 0;
	  }
	  else {
	      memcpy(key_returned,&key,sizeof(struct key_entry));
              return 1;
	  }
     }
     ptplaceact=tplaceact;
     if (key.entry==ENTRY_ERROR_KEY) continue;
     if (key.entry==ENTRY_SLANG_KEY) {
	 if (key.value.slang_key>0xFF) continue;
	 if (key.value.slang_key=='\r') break;
	 if ((key.value.slang_key<' ') && (key.value.slang_key!='\t'))
	     continue;
	 /* normalement, key.value.slang_key<0x80 ici, sauf cas '\t' */
	 if (place+1>=buffsize) return -3;
	 if (tplace+1>=termbufsize) return -3;
	 memmove(buf+placeact+1,buf+placeact,(place-placeact+1)
		                                *sizeof(flrn_char));
	 memmove(termbuf+tplaceact+1,termbuf+tplaceact,tplace-tplaceact+1);
	 buf[placeact++]=(flrn_char) key.value.slang_key; place++;
	 termbuf[tplaceact++]=(char) key.value.slang_key; tplace++;
	 colact+=str_estime_width(termbuf+tplaceact-1,colact,1);
	 temp=1;
	 /* on reteste colact, ou on affiche le texte, ensuite */
     } else {
	 char *bla;
	 int resconv;
	 flrn_char *str;
	 flrn_char strbis[5];
	 if (key.entry==ENTRY_STATIC_STRING) {
	     str=key.value.ststr;
	     temp=0; while ((temp<4) && (str[temp])) temp++;
	 } else {
	     str=key.value.allocstr;
	     temp=fl_strlen(key.value.allocstr);
	 }
	 if (place+temp>=buffsize) {
	     Free_key_entry(&key);
	     return -3;
	 }
	 memmove(buf+placeact+temp,buf+placeact,place-placeact+1);
	 fl_strncpy(buf+placeact,str,temp);
	 placeact+=temp; place+=temp;
	 buf[place]=fl_static('\0');
	 if ((key.entry==ENTRY_STATIC_STRING) && (temp==4)) {
	     fl_strncpy(strbis,str,4); strbis[4]=fl_static('\0');
	     str=strbis;
	 }
	 resconv=conversion_to_terminal(str,&bla,0,(size_t)(-1));
	 temp=strlen(bla);
	 if (tplace+temp>=termbufsize) {
	     if (resconv==0) free(bla);
	     Free_key_entry(&key);
	     return -3;
	 }
	 memmove(termbuf+tplaceact+temp,termbuf+tplaceact,tplace-tplaceact+1);
	 strncpy(termbuf+tplaceact,bla,temp);
	 if (resconv==0) free(bla);
	 colact+=str_estime_width(termbuf+tplaceact,colact,temp);
	 tplaceact+=temp; tplace+=temp;
     }
     if (colact>=Screen_Cols-1) {
	   int newcol;
	   coldec=((colact-Screen_Cols+1)/(4*Screen_Tab_Width)+1)
		 *(4*Screen_Tab_Width);
	   colact=colini;
	   tpdeb=to_make_width(termbuf,coldec,&colact,0);
	   colact=colini+str_estime_width(termbuf+tpdeb,colini,(size_t)-1);
	   newcol=colini;
	   Cursor_gotorc(row,colini);
	   temp=to_make_width(termbuf+tpdeb,Screen_Cols-colini,
		     &newcol,0);
	   Screen_write_nstring(termbuf+tpdeb,temp);
	   Screen_erase_eol();
	   continue;
     } else {
	 Screen_write_string(termbuf+tplaceact-temp);
	 Screen_erase_eol();
     }
   } while (1);
   Free_key_entry(&key);
   return 0;
}

int flrn_getline(flrn_char *buf, size_t buffsize, 
	char *termbuf, size_t termbufsize,
	int row, int col)
{
  return magic_flrn_getline(buf,buffsize,termbuf,termbufsize,row,col,
	  "",0,NULL,NULL);
}

