/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      post.c : pour les posts et l'éditeur intégré
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "art_group.h"
#include "group.h"
#include "flrn_string.h"
#include "flrn_func.h"
#include "post.h"
#include "options.h"
#include "flrn_tcp.h"
#include "flrn_files.h"
#include "flrn_shell.h"
#include "flrn_format.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_menus.h"
#include "rfc2047.h"
#include "flrn_regexp.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

#define DEFAULT_SIZE_LINE 72

extern char short_version_string[];

struct post_lecture {
    struct post_lecture *suivant;
    char *ligne;
    int cas_mail; /* 0 : normal   1 : poster seulement
		   * 2 : mailer seulement  3 : poster normal, mailer+2
		   * 4 : mailer normal, poster+2 */
};

struct post_lecture *Deb_article;
Lecture_List *Deb_body;
Article_List *Pere_post;
int par_mail, supersedes;
/* par_mail = 0 : juste les groupes. =1 : juste par mail. =2 : poste aussi */
int in_moderated_group;

Post_Headers *Header_post;

/* Obtention du sujet d'un post. Pas de formatage pour l'instant */
/* retour : 0 en cas de succes, -1 en cas d'annulation.		 */
static int get_Sujet_post(flrn_char *str) {
   int res;
   char affstr[MAX_SUJET_LEN+1];
   
   Cursor_gotorc(2,0);
   /* FIXME : français et conversion, et la colonne pour le getline */
   Screen_write_string("Sujet : ");
   do {
     str[0]=fl_static('\0');
     affstr[0]='\0';
     res=flrn_getline(str,MAX_SUJET_LEN,affstr,MAX_SUJET_LEN,2,8); 
   } while (res==-2);   /* un backspace en debut de ligne ne fait rien */
   return res; 
}

#ifndef NO_INTERN_EDITOR
/* recalcule la colonne courante, avec un charactère de moins si flag=1 */
/* Renvoie -1 si la ligne est vide et flag=1 */
static int recalc_col (char *ligne, int flag) {
   int ret=0;

   if (flag && (*ligne=='\0')) return -1;
   ret=str_estime_width(ligne,0,strlen(ligne)-flag);
   return ret;
}
#endif

/* Copie dans le fichier temporaire le message tel qu'on suppose qu'il   */
/* sera.								 */
void Copie_prepost (FILE *tmp_file, Lecture_List *d_l, int place, int incl) {
   Header_List *liste;
   Lecture_List *lecture_courant;
   int rc=1; char *trad;
  
   if (Options.edit_all_headers) {
      int i;
      for (i=0; i<NB_KNOWN_HEADERS; i++) {
	if (i==SENDER_HEADER) continue;
	if (i==DATE_HEADER) continue;
	if (i==LINES_HEADER) continue;
	if (i==XREF_HEADER) continue;
	if (i==X_NEWSREADER_HEADER) continue;
	if (i==NB_DECODED_HEADERS+MESSAGE_ID_HEADER) continue;
	if (i==X_TRACE_HEADER) continue;
	if ((i==TO_HEADER) && (!par_mail)) continue;
	if ((i==CC_HEADER) && (!par_mail)) continue;
	if ((i==BCC_HEADER) && (!par_mail)) continue;
	fprintf(tmp_file, "%s ", Headers[i].header);
	if (i<NB_DECODED_HEADERS) {
	   if (Header_post->k_header[i]) {
              rc=conversion_to_editor(Header_post->k_header[i],&trad,
		      0,(size_t)(-1));
	      fputs(trad,tmp_file);
	      if (rc==0) free(trad);
	   } 
	} else if (Header_post->k_raw_header[i-NB_DECODED_HEADERS]) 
	    fputs(Header_post->k_raw_header[i-NB_DECODED_HEADERS],tmp_file);
	putc('\n',tmp_file);
      }
      for (liste=Header_post->autres;liste;liste=liste->next) {
	 if ((!par_mail) && (fl_strncasecmp(liste->header_head,
			 fl_static("In-Reply-To:"),12)==0)) continue;
	 rc=conversion_to_editor(liste->header_head,&trad,
		 0,(size_t)(-1));
	 fputs(trad,tmp_file);
	 if (rc==0) free(trad);
	 putc(' ',tmp_file);
	 rc=conversion_to_editor(liste->header_head,&trad,
		 0,(size_t)(-1));
	 fputs(trad,tmp_file);
	 if (rc==0) free(trad);
	 putc('\n',tmp_file);
      }
   } else {
       /* FIXME : français */
      fputs("Groupes: ",tmp_file);
      if (Header_post->k_header[NEWSGROUPS_HEADER]) {
	  rc=conversion_to_editor(Header_post->k_header[NEWSGROUPS_HEADER],
		  &trad, 0,(size_t)(-1));
	  fputs(trad,tmp_file);
	  if (rc==0) free(trad);
      }
      putc('\n',tmp_file);
       /* FIXME : français */
      fputs("Sujet: ",tmp_file);
      if (Header_post->k_header[SUBJECT_HEADER]) {
	  rc=conversion_to_editor(Header_post->k_header[SUBJECT_HEADER],
		  &trad, 0,(size_t)(-1));
	  fputs(trad,tmp_file);
	  if (rc==0) free(trad);
      }
      putc('\n',tmp_file);
      liste=Header_post->autres;
      while (liste) {
	 if (liste->num_af) {
	     rc=conversion_to_editor(liste->header_head,&trad,
		 0,(size_t)(-1));
	     fputs(trad,tmp_file);
	     if (rc==0) free(trad);
	     putc(' ',tmp_file);
	     rc=conversion_to_editor(liste->header_head,&trad,
		 0,(size_t)(-1));
	     fputs(trad,tmp_file);
	     if (rc==0) free(trad);
	     putc('\n',tmp_file);
	 }
	 liste=liste->next;
      }
   }
   putc('\n', tmp_file);
   if (Pere_post && ((incl==1) || ((incl==-1) && (Options.include_in_edit)))) { 
       if ((!supersedes) && (Options.attribution)) Copy_format (tmp_file,Options.attribution,Pere_post,NULL,0);
       if ((!supersedes) && (Options.index_string)) {
	   rc=conversion_to_editor(Options.index_string,&trad,0,(size_t)(-1));
       } else trad=NULL;
       Copy_article(tmp_file,Pere_post, Options.quote_all, 
	       trad, (supersedes) || (Options.quote_all) ||
	       (Options.quote_sig), 4);
       putc('\n', tmp_file);
       if ((trad) && (rc==0)) free(trad);
   }
   if ((incl==-1) && (Options.sig_file) && (!supersedes)) {
       FILE *sig_file;
       rc=conversion_to_file(Options.sig_file,&trad,0,(size_t)(-1));
       sig_file=open_flrnfile(trad,"r",0,NULL);
       if (rc==0) free(trad);
       if (sig_file) {
          char ligne[82];
	  int deb;
	  deb=1;
	  fputs("-- \n",tmp_file);
	  while (fgets(ligne,81,sig_file)) {
	     if ((deb) && (strcmp(ligne,"-- \n")==0)) {
	        deb=0;
	        continue;
	     } else deb=0;
	     fputs(ligne, tmp_file);
	  }
	  fclose(sig_file);
       }
   }
   lecture_courant=Deb_body;
   while (lecture_courant) {
      if (lecture_courant==d_l)  
	 lecture_courant->size=place;
      lecture_courant->lu[lecture_courant->size]=fl_static('\0');
      /* Par précaution */
      rc=conversion_to_editor(lecture_courant->lu,&trad,0,(size_t)(-1));
      fputs(trad, tmp_file);
      if (rc==0) free(trad);
     /* J'utilise fprintf plutôt que fwrite par principe : fwrite est */
     /* a priori pour des fichiers binaires (et puis, j'ai commencé   */
     /* avec fprintf). Mais bon, on pourrait peut-être changer...     */
      if (lecture_courant==d_l) break;
      lecture_courant=lecture_courant->suivant;
   } 
}

/* Lecture du fichier une fois l'édition finie */
/* Renvoie -1 si le fichier est VRAIMENT trop moche */
int Lit_Post_Edit (FILE *tmp_file, Lecture_List **d_l, int *place) {
   char buf[1025]; /* On suppose qu'un ligne ne peut pas faire plus de 1024 */
   char *buf2, *buf3;
   char *hcur=NULL;
   flrn_char **header_courant=NULL;
   int header_connu=-1;
   int taille;
   int raw_head=0;
   Lecture_List *lecture_courant;
   int headers=1,read_something=0, i;
   Header_List *liste, *last_liste, **unk_header_courant=NULL;
   int rc; flrn_char *trad;

   lecture_courant=Deb_body;
   lecture_courant->size=0;
   lecture_courant->lu[0]=fl_static('\0');
   while (fgets(buf,1025,tmp_file)) {
      if (strlen(buf)==1024) return -1; 
               /* TODO : faire sauter cette limitation */
      if (!read_something && (buf[0]=='\n')) continue; /* J'ignore les */
      		/* les lignes en debut fichier */
      read_something=1;
      if (headers) {
	 if ((buf[0]=='\n') || (!isblank(buf[0]))) {
	     if (header_courant) {
	         if (hcur) {
		    rc=conversion_from_editor(hcur,&trad,0,(size_t)(-1));
		    *header_courant=trad;
		    if ((par_mail==0) && 
		        ((header_connu==TO_HEADER) || (header_connu==CC_HEADER)
			 || (header_connu==BCC_HEADER)))
			par_mail=2;
		    if (rc==0) free(hcur);
		    hcur=NULL;
	         } else {
		    if (header_connu==-1) {
		       liste=(*unk_header_courant)->next;
		       free((*unk_header_courant)->header_head);
		       free(*unk_header_courant);
		       *unk_header_courant=liste;
	            }
		 }
		 header_courant=NULL;
		 raw_head=0;
	     } else if (raw_head) {
		 Header_post->k_raw_header[raw_head-NB_DECODED_HEADERS]=
		     hcur;
		 hcur=NULL;
		 raw_head=0;
	     }
             if (buf[0]=='\n') {headers=0; continue;}
	 }
	 if (isblank(buf[0])) {
	   if ((header_courant) || (raw_head)) {
	     if (hcur) {
	       hcur=safe_realloc(hcur,
		      (strlen(hcur)+3+strlen(buf))*sizeof(char));
	       strcat(hcur, "\n");
	       strcat(hcur, buf);
	       hcur[strlen(hcur)-1]='\0';
	     } else 
	       hcur=safe_strdup(buf);
	   } else headers=0; /* On recopiera après */
	 } else {
	     /* on a fini un header */
           buf2=strchr(buf,':');
	   buf3=strpbrk(buf, " \t");
	   if ((buf2==NULL) || ((buf3) && (buf3-buf2<0))) headers=0; else {
	      for (i=0; i<NB_KNOWN_HEADERS; i++) 
	        if (strncasecmp(buf,Headers[i].header,Headers[i].header_len)==0)
		{
		  if (i<NB_DECODED_HEADERS)
		      header_courant=&(Header_post->k_header[i]); 
		  else {
		      raw_head=i;
		      header_courant=NULL;
		  }
		  break;
		}
	      header_connu=i;
              if (i==NB_KNOWN_HEADERS) {
		  /* FIXME : francais */
	         if ((strncasecmp(buf,"sujet:", 6)==0) && 
		     !Options.edit_all_headers) {
		    header_courant=&(Header_post->k_header[SUBJECT_HEADER]); 
		    header_connu=SUBJECT_HEADER;
		 }
		 /* FIXME : français */
		 else if ((strncasecmp(buf,"groupes:", 8)==0) && 
		     !Options.edit_all_headers) {
		    header_courant=&(Header_post->k_header[NEWSGROUPS_HEADER]); 
		    header_connu=NEWSGROUPS_HEADER;
		 } else {
		   header_connu=-1;
		   taille=buf2+1-buf;
		   rc=conversion_from_editor(buf,&trad,0,taille);
                   liste=Header_post->autres;
		   last_liste=NULL;
		   while (liste) {
		      if (fl_strncasecmp(buf, liste->header_head,
				  taille)==0) break;
		      last_liste=liste;
		      liste=liste->next;
		   }
		   if (liste) {
		      header_courant=&(liste->header_body); 
		      if (last_liste) unk_header_courant=&(last_liste->next);
		       else unk_header_courant=&(Header_post->autres);
		      if (rc==0) free(trad);
		   } else
		   if (last_liste) {
		      last_liste->next=safe_calloc(1,sizeof(Header_List));
		      if (rc==0) last_liste->next->header_head=trad;
		      else last_liste->next->header_head=safe_flstrdup(trad);
		      header_courant=&(last_liste->next->header_body);
		      unk_header_courant=&(last_liste->next);
		   } else {
		      Header_post->autres=safe_calloc(1,sizeof(Header_List));
		      if (rc==0) Header_post->autres->header_head=trad;
		      else Header_post->autres->header_head=safe_flstrdup(trad);
		      header_courant=&(Header_post->autres->header_body);
		      unk_header_courant=&(Header_post->autres);
		   }
		   (*unk_header_courant)->num_af=1;
		 }
	      }
	      buf2++;
	      if (header_courant) {
		  if (*header_courant) {
		      free(*header_courant);
		      *header_courant=NULL;
		  }
	      } else if (raw_head) {
		  if (Header_post->k_raw_header[raw_head-NB_DECODED_HEADERS])
	            free (Header_post->k_raw_header
			    [raw_head-NB_DECODED_HEADERS]);
		  Header_post->k_raw_header[raw_head-NB_DECODED_HEADERS]=NULL;
	      }
	      while ((*buf2) && isblank(*buf2)) buf2++;
	      if ((*buf2=='\0') || (*buf2=='\n')) continue;
	      buf3=strchr(buf2,'\n'); if (buf3) *buf3='\0';
	      hcur=safe_strdup(buf2);
	   }
	}
        continue;
      }
      rc=conversion_from_editor(buf,&trad,0,(size_t)(-1));
      str_cat(&lecture_courant, trad);
      if (rc==0) free(trad);
   }
   *d_l=lecture_courant;
   *place=lecture_courant->size;
   return 0;
}

/* Summon_Editor : programme de préparation, d'appel, et de relecture    */
/* de l'éditeur.							 */
/* Renvoie -1 en cas de bug majeur.					 */
/* Renvoie 2 en cas de message non modifié				 */
/* Renvoie 1 en cas de message vide					 */
/* Renvoie 0 si pas (trop) de problèmes...				 */
static int Summon_Editor (Lecture_List **d_l, int *place, int incl) {
   FILE *tmp_file;
   int res;
   struct stat before, after;
   char name[MAX_PATH_LEN];

   free_text_scroll();
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   tmp_file=open_postfile(TMP_POST_FILE,"w",name,1);
   if (tmp_file==NULL) {
       /* FIXME : français */
      Screen_write_string("Echec dans l'ouverture du fichier temporaire.");
      return -1;
   }
   Copie_prepost (tmp_file, *d_l, *place, incl);
   fclose(tmp_file);
   if (stat(name,&before)<0) {
     /* ca doit jamais arriver */
       /* FIXME : français */
     Screen_write_string("Echec dans la création du fichier temporaire.");
     return -1;
   }
   res=Launch_Editor(0,name);  
   if (res==-1) {
       /* FIXME : français */
      Screen_write_string("Echec dans le lancement de l'éditeur.");
      return -1;
   }
   if (stat(name,&after)<0) {
     /* ca doit jamais arriver */
       /* FIXME : français */
     Screen_write_string("Et le fichier temporaire louze.");
     return -1;
   }
   if (after.st_size == 0) {
       /* FIXME : français */
     Screen_write_string("Fichier temporaire vide.");
#ifdef USE_MKSTEMP
     unlink(name);
#endif
     return 1;
   }
   if (after.st_mtime == before.st_mtime) {
       /* FIXME : français */
     Screen_write_string("Message non modifié.");
#ifdef USE_MKSTEMP
     unlink(name);
#endif
     return 2;
   }
   tmp_file=fopen(name,"r");
   if (tmp_file==NULL) {
       /* FIXME : français */
      Screen_write_string("Echec dans la réouverture du fichier temporaire");
      return -1;
   }
   res=Lit_Post_Edit(tmp_file, d_l, place);
   fclose (tmp_file);
#ifdef USE_MKSTEMP
   unlink(name);
#endif
   return res;
}


/* Prend le message à poster à partir d'un fichier...			 */
/* renvoie 0 en cas d'echec...						 */
int charge_postfile (char *str) {
   int res;
   FILE *post_file;
   int size=0;
   Lecture_List *lecture_courant;

   Deb_body=lecture_courant=alloue_chaine();
   post_file=fopen(str,"r");
   if (post_file==NULL) {
       /* FIXME : français */
       Aff_error("Echec en ouverture du fichier");
       return 0;
   }
   res=Lit_Post_Edit(post_file,&lecture_courant, &size);
   if (res<0) {
       /* FIXME : français */
       Aff_error("Fichier incompréhensible...");
       return 0;
   }
   fclose(post_file);
   return 1;
}

#ifndef NO_INTERN_EDITOR
/* TODO TODO TODO TODO */
static void include_file(FILE *to_include, File_Line_Type **last, Lecture_List **d_l, int *row, int *inited, int *empty, int *size_max_line) {
   char buf[513]; /* On suppose qu'un ligne ne peut pas faire plus de 512 */
   int act_row=*row;
   int already_init=*inited;
   int len;

   while (fgets(buf,513,to_include)) {
      *empty=0;
      buf[512]='\0';
      len=strlen(buf);
      if (buf[len-1]=='\n') buf[--len]='\0';
      if (len+1>*size_max_line) *size_max_line=len+1; /* on aura besoin d'une longue ligne */
      (*last)=Ajoute_line(buf);
      str_cat(d_l,buf);
      Cursor_gotorc(act_row,0);
      Screen_write_string(buf);
      act_row++;
      ajoute_char(d_l,'\n');
      if (act_row>=Screen_Rows) {
          if (!already_init) {
	     Init_Scroll_window(Screen_Rows-4,4,Screen_Rows-5);
	     already_init=1;
	  }
	  Do_Scroll_Window(1,1);
	  Cursor_gotorc(Screen_Rows-1,0);
	  Screen_erase_eol();
	  act_row=Screen_Rows-1;
      }
   }
   *row=act_row;
   *inited=already_init;
}


/* Analyse le debut de la ligne tapée (pour voir les commandes spéciales */
/* Pour l'instant, on se contente de renvoyer 1 en cas de ".\n"		 */
/* On appelle aussi Summon_Editor en cas de "~e\n", puis on renvoie 2    */
/* ou 3 si le message est vide 						 */
/* 4 : ligne annulée... 						 */
/* On implémente aussi la possibilité de faire un ~r pipo\n...		 */
/* et aussi un ~p, pour plus tard....					 */
static int analyse_ligne (Lecture_List **d_l, int *place, FILE **a_inclure) {
   Lecture_List *sd_l=*d_l;
   int splace=*place;
   char debut[3];
   int i, res;
   
   for (i=0; i<3; i++) {
       debut[i]=sd_l->lu[splace];
       if (debut[i]=='\n') break;
       splace++; if (splace>=sd_l->size) { splace=0; sd_l=sd_l->suivant; }
   }

   if (strncmp(debut,".\n",2)==0) return 1;
   if ((strncmp(debut,"~e\n",3)==0) || (strncmp(debut,"~E\n",3)==0)) {
      res=Summon_Editor(d_l, place, (debut[1]=='E'));
      if ((res>=0)) return (res==1)?3:2; else return -1;
   }
   if ((strncmp(debut,"~r\n",3)==0) || (strncmp(debut,"~r ",3)==0)) {
     char nom[256];
     if (i==3) {
       for (i=0; i<255; i++) {
          nom[i]=sd_l->lu[splace];
	  if (nom[i]=='\n') break;
	  splace++; if (splace>=sd_l->size) { splace=0; sd_l=sd_l->suivant; }
       }
       nom[i]='\0';
       (*a_inclure)=fopen(nom,"r");
     } else if (Options.sig_file) {
       (*a_inclure)=open_flrnfile(Options.sig_file,"r",0,NULL);
     } else (*a_inclure)=NULL;
     return 4;
   }
   return 0;
}
#endif

/* Obtention du corps du post.					*/
/* La gestions des points doubles ne se fait pas.		*/
static int Screen_Start;

static int get_Body_post() {
   Lecture_List *lecture_courant, *debut_ligne;
   int incl_in_auto=-1,key;
   struct key_entry ke;
#ifndef NO_INTERN_EDITOR
   int act_col=0, act_row=4, size_debligne, fin=0;
   int already_init=0;
   char chr;
   File_Line_Type *last_line=NULL;
   char *ligne_courante, *ptr_ligne_cou;
   int empty=1, size_max_line;
   int screen_start=0;
   FILE *to_include=NULL;
#endif

   Screen_Start=0;
   Deb_body=lecture_courant=debut_ligne=alloue_chaine();
#ifndef NO_INTERN_EDITOR
   if (Options.auto_edit) 
#endif
   {
      int place, res;
     
      ke.entry=ENTRY_ERROR_KEY;
      place=res=0;
      while (res>=0) {
        res=Summon_Editor(&lecture_courant,&place, incl_in_auto);
	if ((incl_in_auto==0) && (res==2)) res=0; 
	    		     /* C'est au moins la seconde édition, le */
	  		     /* message a deja ete modifié une fois...*/
	incl_in_auto=0;
	lecture_courant->size=place;
	lecture_courant->lu[place]=fl_static('\0');
        if (res==0) 
	  while (res==0) {
  	    Cursor_gotorc(Screen_Rows2-1,0);
	    /* FIXME : francais */
	    Screen_write_string("(P)oster, (E)diter, (A)nnuler ? ");
            Attend_touche(&ke);
	    if (KeyBoard_Quit) return 0;
	    if (ke.entry==ENTRY_SLANG_KEY) {
		key=ke.value.slang_key;
	        key=toupper(key);
	        if (key=='A') 
                   return 0;
	        if (key=='E') res=1;
	        if (key=='P' || key=='Y' || key=='\r' ) {
                    if (lecture_courant->suivant) 
			free_chaine(lecture_courant->suivant);
                    return 1;
	        }
	    }
	}
        else {
	  if (res>0) return -1; /* C'est un message non valide */
#ifdef NO_INTERN_EDITOR
	  Cursor_gotorc(2,0);
       /* FIXME : français */
	  Screen_write_string("(erreur dans l'édition, vérifier la variable d'environnement EDITOR)");
	  Cursor_gotorc(3,0);
       /* FIXME : français */
	  Screen_write_string("Pour poster, éditez le .article à la main, puis utilisez \\<cmd> .article");
	  return -1;
#else
	  Cursor_gotorc(2,0);
	  Screen_write_string("   ( erreur dans l'édition. option auto_edit ignorée )");
#endif
        }
      } 
   }
#ifdef NO_INTERN_EDITOR
   return -1; /* unreachable */
#else

   Cursor_gotorc(4,0);
   size_debligne=0;
   size_max_line=(Screen_Cols > DEFAULT_SIZE_LINE ? Screen_Cols+1 : DEFAULT_SIZE_LINE+1);
   ptr_ligne_cou=ligne_courante=safe_malloc(size_max_line*sizeof(char));
   *ptr_ligne_cou='\0';

   do {
      if (act_col-Screen_Start>=Screen_Cols) {
        screen_start=Screen_Start;
        Screen_Start=act_col-Screen_Cols+1;
	if (screen_start!=Screen_Start) {
	  screen_start=Screen_Start;
	  Screen_set_screen_start(NULL,&screen_start);
	  if (last_line) {
            if (!already_init) {
              Init_Scroll_window(act_row-4,4,Screen_Rows-5);
              already_init=1;
            }
            Do_Scroll_Window(0,1);
	  }
          Cursor_gotorc(act_row,0);
          Screen_write_string(ligne_courante);
          Screen_erase_eol();
	}
      } else
      if (act_col-Screen_Start<=1) {
        screen_start=Screen_Start;
        if (act_col<Screen_Cols) Screen_Start=0; else
	   Screen_Start=act_col-Screen_Cols+1;
	if (screen_start!=Screen_Start) {
	  screen_start=Screen_Start;
	  Screen_set_screen_start(NULL,&screen_start);
	  if (last_line) {
            if (!already_init) {
               Init_Scroll_window(act_row-4,4,Screen_Rows-5);
               already_init=1;
            }
            Do_Scroll_Window(0,1);
	  }
          Cursor_gotorc(act_row,0);
          Screen_write_string(ligne_courante);
          Screen_erase_eol();
	}
      }
      key=Attend_touche();
      if (sigwinch_received || KeyBoard_Quit) {
           lecture_courant->lu[lecture_courant->size]='\0';
	   free_text_scroll();
	   sigwinch_received=0;
	   free(ligne_courante);
           return 0;
      }
      if (key=='\r') key='\n'; /* On ne VEUT pas distinguer ^J et ^M */
      if ((key==FL_KEY_BACKSPACE) || (key==21) || (key==23)) {
	int mottrouve=0;
	int lignetrouvee=0;
	int retour_enleve=0;
	do {
	  
	  if (retour_enleve) break;
	  chr=get_char(lecture_courant,1);
	  if ((key==23) && (mottrouve) && (isblank(chr))) break;
	  if ((chr=='\n') && ((last_line==NULL) || (lignetrouvee))) break;
	  mottrouve=(!isblank(chr)) || (chr=='\n');
	  lignetrouvee=1; /* on ne poursuit jamais au-dela d'une ligne */
	  retour_enleve=(chr=='\n');
	  if (chr=='\0') break;
          enleve_char(&lecture_courant);
	  if (chr==9)  
	      act_col=recalc_col(ligne_courante,1);
	    else act_col--;
	  if (act_col<0) {
	      Read_Line(ligne_courante, last_line);
	      ptr_ligne_cou=index(ligne_courante, '\0');
	      /* On enleve le dernier caractere de la ligne si on n'a pas */
	      /* enleve le saut de ligne (logique, non ?) */
	      if (chr!='\n')
	        *(--ptr_ligne_cou)='\0';
	      if (last_line->prev) 
	      {
		 last_line=last_line->prev;
	         Retire_line(last_line);
              } else
	      {
	         Retire_line(last_line);
		 last_line=NULL;
		 already_init=0;
	      }
	      Cursor_gotorc(act_row, 0);
	      Screen_erase_eol();
              act_col=recalc_col(ligne_courante, 0);
	      act_row--;
	      if (act_row<4) {
		 if (already_init) Do_Scroll_Window(-1,0);
		 act_row=4;
		 Cursor_gotorc(act_row,0);
	         Screen_write_string(ligne_courante);
	      }
	      if (chr=='\n') { 
	         /* on doit recalculer deb_line et size_deb_line */
		 debut_ligne=lecture_courant;
		 cherche_char(&debut_ligne,&size_debligne,'\n');
	      }
	  } else *(--ptr_ligne_cou)='\0';
	  Cursor_gotorc(act_row,act_col);
	  Screen_erase_eol();
	}
	while ((key!=FL_KEY_BACKSPACE));
	continue;
      }
      if (key>255) continue;
      if (key=='\n') {
	 ajoute_char(&lecture_courant, '\n');
	 fin=analyse_ligne(&debut_ligne, &size_debligne, &to_include);
	 if (fin==-1) {
	     Cursor_gotorc(2,0);
	     Screen_write_string(" Ligne trop longue !!! ");
	     sleep(1);
	     free(ligne_courante);
	     return 0;
	 }
	 if (fin==0) {  /* Si rien de special, on encaisse la ligne */
	   last_line=Ajoute_line(ligne_courante);
	   empty=0;
	   debut_ligne=lecture_courant;
           size_debligne=lecture_courant->size;
           act_row++; act_col=0;
	   if (act_row>=Screen_Rows) {
	      if (!already_init) {
                Init_Scroll_window(Screen_Rows-4,4,Screen_Rows-5);
	        already_init=1;
	      }
	      Do_Scroll_Window(1,1);
/* On efface la ligne devenue superflue */
	      Cursor_gotorc(Screen_Rows-1,0);
	      Screen_erase_eol();
	      act_row=Screen_Rows-1;
	   }
	 } else { /* On annule la ligne rentree */
	    lecture_courant=debut_ligne;
	    lecture_courant->size=size_debligne;
	    act_col=0;
	    Cursor_gotorc(act_row,0);
	    Screen_erase_eol();
	 }
	 if (to_include) {
	    int old_size_max_line=size_max_line;
	    include_file(to_include, &last_line, &lecture_courant,&act_row,&already_init, &empty, &size_max_line);
	    debut_ligne=lecture_courant;
            size_debligne=lecture_courant->size;
	    fclose(to_include);
	    to_include=NULL;
	    if (size_max_line!=old_size_max_line) {
	         /* C'est lourd, une ligne longue */
	       free(ligne_courante);
	       ligne_courante=safe_malloc(size_max_line*sizeof(char));
	    }
	 }
	 ptr_ligne_cou=ligne_courante;
	 *ptr_ligne_cou='\0';
	 if ((fin==2)||(fin==3)) {
	   empty = (fin==3);
	   Cursor_gotorc(2,0);
	   if (fin ==2) {
	     Screen_write_string(" (continue) ");
	   } else {
	     Screen_write_string(" (début) ");
	   }
	   act_row=4; act_col=0;
	   already_init=0;
	   last_line=NULL;
	   Screen_Start=screen_start=0;
	   Screen_set_screen_start(NULL,NULL);
	 }
         Cursor_gotorc(act_row, act_col);
	 if (fin!=1) fin=0;
      } else
      {
	 if ((key==4)&&(act_col==0)) {fin=1; continue;}
         if (key==9) act_col=(act_col/Screen_Tab_Width+1)*Screen_Tab_Width; 
	   else if (key>31) act_col++; else continue;
         Screen_write_char(key);
	 *(ptr_ligne_cou++)=key;
	 *ptr_ligne_cou='\0';
	 ajoute_char(&lecture_courant, key);
         if (act_col>=DEFAULT_SIZE_LINE) {
/* Cette configuration tordue de l'éditeur intégrée a été suggérée par APO */
	     char *beuh1;
	     int on_change=0;
	     beuh1=strrchr(ligne_courante,' ');
	     if (beuh1) {
	       char *beuh2;
	       *(beuh1++)='\0';
	       last_line=Ajoute_line(ligne_courante);
	       Cursor_gotorc(act_row,0);
	       Screen_write_string(ligne_courante);
	       Screen_erase_eol();
               beuh2=safe_strdup(beuh1);
	       strcpy(ligne_courante,beuh2);
	       ptr_ligne_cou=index(ligne_courante,'\0');
	       Cursor_gotorc(act_row+1,0);
	       Screen_write_string(ligne_courante);
	       act_col=ptr_ligne_cou-ligne_courante;
	       free(beuh2);
	       while (get_char(lecture_courant,1)!=' ') 
	           enleve_char(&lecture_courant);
	       enleve_char(&lecture_courant);
	       ajoute_char(&lecture_courant, '\n');
	       str_cat(&lecture_courant,ligne_courante);
	       on_change=1;
	    } else if (act_col>=Screen_Cols) {
	       last_line=Ajoute_line(ligne_courante);
	       ptr_ligne_cou=ligne_courante;
	       *ptr_ligne_cou='\0';
	       act_col=0;
	       on_change=1;
	    }
	    if (on_change) {
              act_row++;
	      if (act_row>=Screen_Rows) {
	         if (!already_init) {
                   Init_Scroll_window(Screen_Rows-4,4,Screen_Rows-5);
		   already_init=1;
	         }
	         Do_Scroll_Window(1,1);
/* On efface la ligne devenue superflue */
	         Cursor_gotorc(Screen_Rows-1,0);
	         Screen_write_string(ligne_courante);
	         Screen_erase_eol();
	         act_row=Screen_Rows-1;
	      }
              Cursor_gotorc(act_row,act_col);
	   }
         }
      }
   } while (!fin);
   lecture_courant->lu[lecture_courant->size]='\0';
   free_text_scroll();
   if (lecture_courant->suivant) {
     free_chaine(lecture_courant->suivant);
     lecture_courant->suivant=NULL;
   }
   free(ligne_courante);
   return (empty)?0:1;
#endif
}

/* Regarde l'existence d'un groupe dans un header... essaie une correction */
/* sinon...								   */
static flrn_char *check_group_in_header(flrn_char *nom, int *copy_pre, 
	char *header, int in_newsgroup) {
   flrn_char *nom2=nom;
   Newsgroup_List *groupe;
   int key,alloue=0,ret,col,to_test=1,rc;
   struct key_entry ke;
   char *trad;

   ke.entry=ENTRY_ERROR_KEY;
   if (fl_strlen(nom)>=MAX_NEWSGROUP_LEN-(*copy_pre ? 
	       fl_strlen(Options.prefixe_groupe) : 0))
           return nom;
     /* Tant pis. Si le mec se fout de ma gueule, c'est réciproque */
   while (1) {
     if (to_test) {
       groupe=cherche_newsgroup(nom2,1,*copy_pre);
       if (groupe!=NULL) {
         if (in_newsgroup && (groupe->flags & GROUP_MODERATED_FLAG)) 
	     in_moderated_group=1;
         return nom2;
       }
       if (*copy_pre) {
         groupe=cherche_newsgroup(nom2,1,0);
         if (groupe!=NULL) {
            if (in_newsgroup && (groupe->flags & GROUP_MODERATED_FLAG)) 
	        in_moderated_group=1;
            *copy_pre=0;
	    return nom2;
         }
       }
       Cursor_gotorc(1,0);
       Screen_erase_eos();
     }
     Cursor_gotorc(Screen_Rows2-2,0);
     Screen_erase_eol();
     /* FIXME : francais */
     Screen_write_string("Groupe inconnu dans ");
     Screen_write_string(header);
     Screen_write_string(" : ");
     rc=conversion_to_terminal(nom2,&trad,0,(size_t)(-1));
     Screen_write_string(trad);
     if (rc==0) free(trad);
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_erase_eol();
     /* FIXME : francais */
     Screen_write_string("(L)aisser,(S)upprimer,(R)emplacer,(M)enu ? ");
     Attend_touche(&ke);
     if (KeyBoard_Quit) {
	 if (alloue) free(nom2); 
	 return NULL; 
     }
     if (ke.entry==ENTRY_SLANG_KEY) {
	 key=ke.value.slang_key;
         key=toupper(key);
	 if (key=='S') {
	     if (alloue) free(nom2);
	     return NULL;
	 }
         if (key=='L') 
            return nom2;
	 if ((key=='R') || (key=='M')) {
	     Cursor_gotorc(Screen_Rows2-1,0); 
	     if (!alloue) {
		 nom2=safe_calloc(MAX_NEWSGROUP_LEN,sizeof(flrn_char));
		 alloue=1;
	     } else nom2[0]=fl_static('\0');
	     Screen_erase_eol();
	     if (key=='R') {
		 if (*copy_pre) strcpy(nom2,Options.prefixe_groupe);
		 fl_strcat(nom2,nom);
		 /* FIXME : francais et colonne */
		 Screen_write_string("Nom du groupe : ");
		 trad=safe_malloc(MAX_NEWSGROUP_LEN+1);
		 rc=conversion_to_terminal(nom2,&trad,MAX_NEWSGROUP_LEN,
			 (size_t)(-1));
		 Screen_write_string(trad);
		 ret=flrn_getline(nom2,MAX_NEWSGROUP_LEN,
			 trad,MAX_NEWSGROUP_LEN,Screen_Rows2-1,16);
		 free(trad);
		 if (ret<0) {
		     fl_strcpy(nom2,nom);
		     to_test=0;
		 } else {
		     to_test=1;
		     *copy_pre=0;
		 }
		 continue;
	     } 
	     /* key=='M' */
	     col=(Options.use_regexp ? 9 : 14);
	     /* FIXME : francais et colonne */
	     Screen_write_string(Options.use_regexp ? "Regexp : " :
		"Sous-chaîne : ");
	     trad=safe_malloc(MAX_NEWSGROUP_LEN+1);
	     trad[0]='\0';
	     ret=flrn_getline(nom2,MAX_NEWSGROUP_LEN,
		     trad,MAX_NEWSGROUP_LEN,Screen_Rows2-1,col);
	     free(trad);
	     if (ret<0) {
		 fl_strcpy(nom2,nom);
		 to_test=0;
		 continue;
	     }
	     ret=get_group(&groupe,1+(*copy_pre)*2+Options.use_regexp*4+8,
		     nom2);
	     if ((ret==0) && (groupe)) {
		 if (in_newsgroup && (groupe->flags & GROUP_MODERATED_FLAG))
		     in_moderated_group=1;
		 fl_strcpy(nom2,groupe->name);
		 *copy_pre=0;
		 return nom2;
	     }
	     fl_strcpy(nom2,nom);
	 }
	 to_test=0;
      }
   }
}
	

/* Prepare les headers pour le post */
/* Pour l'instant, on s'occupe juste de sender et de X-newsreader */
/* On pourrait aussi supprimer Control */
/* 0 : bon, -1 : annuler */
static int Format_headers() {
   int len1, len2, copy_pre, i,j, nb_groups;
   char *real_name=safe_strdup(flrn_user->pw_gecos), *buf;
   static flrn_char *delim2=fl_static(" ,;\t");
   flrn_char *dummy, *flb1, *flb2, *flb3;

   buf=strchr(real_name,','); if (buf) *buf='\0';

   /* Sender */
   len1=257+strlen(flrn_user->pw_name);
   if (Options.post_name)
     len2=4+fl_strlen(Options.post_name);
   else
     len2=4+strlen(real_name);
   Header_post->k_header[SENDER_HEADER]=
        safe_realloc(Header_post->k_header[SENDER_HEADER],
		(len1+len2)*sizeof(flrn_char));
   Get_user_address(Header_post->k_header[SENDER_HEADER]);
   fl_strcat(Header_post->k_header[SENDER_HEADER],fl_static(" ("));
   if (Options.post_name)
     fl_strcat(Header_post->k_header[SENDER_HEADER], Options.post_name);
   else {
       int rc; flrn_char *trad;
       rc=conversion_from_utf8(real_name,&trad,0,(size_t)(-1));
       fl_strcat(Header_post->k_header[SENDER_HEADER], trad);
       if (rc==0) free(trad);
   }
   fl_strcat(Header_post->k_header[SENDER_HEADER],fl_static(")"));
   if (fl_strcmp(Header_post->k_header[SENDER_HEADER], 
              Header_post->k_header[FROM_HEADER])==0) {
      free(Header_post->k_header[SENDER_HEADER]);
      Header_post->k_header[SENDER_HEADER]=NULL;
   }
   free(real_name);

   /* X-NEWSREADER */
   Header_post->k_header[X_NEWSREADER_HEADER]=
        safe_realloc(Header_post->k_header[X_NEWSREADER_HEADER], (strlen(short_version_string)+1)*sizeof(flrn_char));
   {
       int rc; flrn_char *trad;
       rc=conversion_from_utf8(short_version_string,&trad,0,(size_t)(-1));
       fl_strcpy(Header_post->k_header[X_NEWSREADER_HEADER], 
	       trad);
       if (rc==0) free(trad);
   }

   /* Newsgroups , Followup-To*/
   in_moderated_group=0;
   for (i=0; i<2; i++) {
     j=(i==0 ? NEWSGROUPS_HEADER : FOLLOWUP_TO_HEADER);
     if (Header_post->k_header[j]) {
       if ((j==FOLLOWUP_TO_HEADER) && (fl_strcasecmp(Header_post->k_header[j],
		       fl_static("poster"))==0)) continue;
       flb1=fl_strtok_r(Header_post->k_header[j],delim2,&dummy);
       flb3=NULL;
       len1=0;
       while (flb1) {
         copy_pre=((Options.prefixe_groupe) && 
		 (fl_strncmp(flb1,Options.prefixe_groupe,fl_strlen
			  (Options.prefixe_groupe))!=0));
	 flb2=check_group_in_header(flb1,&copy_pre,Headers[j].header,(i==0));
	 if (flb2==NULL) {
	   flb1=strtok(NULL,delim2);
	   continue;
	 }
         flb3=safe_realloc(flb3,len1+(copy_pre?fl_strlen(Options.prefixe_groupe):0)+fl_strlen(flb2)+2);
         if (len1==0) flb3[0]=fl_static('\0');
         len1+=(copy_pre?fl_strlen(Options.prefixe_groupe):0)+
	     fl_strlen(flb2)+1;
         if (copy_pre) fl_strcat(flb3,Options.prefixe_groupe);
         fl_strcat(flb3,flb2);
         fl_strcat(flb3,fl_static(","));
	 if (flb2!=flb1) free(flb2);
         flb1=fl_strtok_r(NULL,delim2,&dummy);
       }
       if (len1>0) flb3[len1-1]='\0'; /* Enlevons la dernière , */
       free(Header_post->k_header[j]);
       Header_post->k_header[j]=flb3;
    }
  }
  if (par_mail==1) return 0; /* on ne teste pas le fu2 si il n'y a pas
				de post */
  /* Test du newsgroups */
  flb1=Header_post->k_header[NEWSGROUPS_HEADER];
  nb_groups=0;
  while (flb1) {
     flb1=strchr(flb1,',');
     if (flb1) flb1++;
     nb_groups++;
  }
  if (nb_groups>1) {
     struct key_entry ke;
     int key;
     Liste_Menu *lemenu,*courant;
     ke.entry=ENTRY_ERROR_KEY;
     while (Header_post->k_header[FOLLOWUP_TO_HEADER]==NULL) {
       Cursor_gotorc(Screen_Rows2-2,0);
       Screen_erase_eol();
       /* FIXME : français */
       Screen_write_string("Attention : crosspost sans suivi-à.");
       Cursor_gotorc(Screen_Rows2-1,0);
       Screen_erase_eol();
       /* FIXME : français */
       Screen_write_string("(L)aisser, (A)nnuler, (C)hoisir un suivi-à ? ");
       while (1) {
          Attend_touche(&ke);
	  if (KeyBoard_Quit) return -1;
	  if (ke.entry==ENTRY_SLANG_KEY) {
	      key=ke.value.slang_key;
	      break;
	  }
       }
       key=toupper(key);
       if (key=='L') return 0;
       if (key=='A') return -1;
       {
         lemenu=NULL; courant=NULL;
	 flb2=Header_post->k_header[NEWSGROUPS_HEADER];
	 while (flb2) {
	    flb1=flb2;
	    flb2=fl_strchr(flb2,fl_static(','));
	    if (flb2) *flb2='\0';
            courant=ajoute_menu(courant,&fmt_option_menu,&flb1,flb1);
	    if (lemenu==NULL) lemenu=courant;
	    if (flb2) *(flb2++)=fl_static(',');
	 }
	 num_help_line=11;
		 /* FIXME : francais */
	 flb1=(flrn_char *)Menu_simple(lemenu,lemenu,NULL,NULL,
  	     fl_static("Choix du groupe. <entrée> pour choisir, q pour quitter..."));
	 Libere_menu(lemenu);
	 if (flb1) {
	     flb2=fl_strchr(flb1,fl_static(','));
	     if (flb2) *flb2='\0';
	     Header_post->k_header[FOLLOWUP_TO_HEADER]=safe_strdup(flb1);
	     if (flb2) *flb2=fl_static(',');
	 }

       }
     }
  } 
  if (in_moderated_group) {
     Cursor_gotorc(Screen_Rows2-3,0);
     /* FIXME : français */
     Screen_write_string("Vous postez dans un groupe modéré : il peut se passer un certain temps avant");
     Cursor_gotorc(Screen_Rows2-2,0);
     /* FIXME : français */
     
     Screen_write_string("que votre message n'apparaisse.");
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_erase_eol();
     /* FIXME : français */
     Screen_write_string("Appuyez sur une touche pour envoyer le message, ^C pou annuler.");
     Attend_touche(NULL);
     if (KeyBoard_Quit) return -1;
  }
  return 0;
}

/* Formatage du message à des fins de post */
/* On fait la gestion des points doubles, l'encodage des headers */
/* et les autres encodages */
/* Si to_cancel n'est pas NULL, on rajoute le header Control */
/* ou supersedes, selon la valeur de supersedes */
static int Format_article(char *to_cancel) {
   Lecture_List *lecture_courant;
   struct post_lecture *ecriture_courant;
   int place_lu=0, deb_ligne=1, i;
   Header_List *liste;
   size_t elen;

   if (debug) fprintf(stderr, "Appel a format_article\n");
   lecture_courant=Deb_body;
   ecriture_courant=Deb_article=safe_calloc(1,sizeof(struct post_lecture));

  /* Ecriture des headers */
  /* Attention : ici les headers sont plutôt buggués : il faudrait */
  /* formater, et remplacer les \n par des \r\n... Mais bon...     */
   i=Format_headers();
   if (i<0) return i;
   for (i=0; i<NB_DECODED_HEADERS; i++) 
      if (Header_post->k_header[i]) {
	  int crit, rc;
	  char *trad;
	  size_t len;
	  if ((i==TO_HEADER) || (i==CC_HEADER) || (i==BCC_HEADER)) {
	      if (par_mail==0) continue;
	      if (par_mail==2) 
		  ecriture_courant->cas_mail=(i==BCC_HEADER ? 2 : 3);
	  }
	  if (((i==NEWSGROUPS_HEADER) || (i==FOLLOWUP_TO_HEADER)) && 
		  (par_mail))
	      ecriture_courant->cas_mail=4;
	  /* FIXME : nombre absurde */
	  len=Headers[i].header_len+145+
	      fl_strlen(Header_post->k_header[i])*2; 
	  ecriture_courant->ligne=safe_malloc(len+1);
	  if (ecriture_courant->cas_mail>2) 
	      strcpy(ecriture_courant->ligne,"X-");
	  else (ecriture_courant->ligne)[0]='\0';
	  strcat(ecriture_courant->ligne,Headers[i].header);
	  strcat(ecriture_courant->ligne," ");
	  trad=ecriture_courant->ligne+Headers[i].header_len+1+
	      (ecriture_courant->cas_mail>2 ? 2 : 0);
	  len-=Headers[i].header_len+1;
	  if (i<NB_UTF8_HEADERS) {
	      rc=conversion_to_utf8(Header_post->k_header[i],
		      &trad, len-3, (size_t)(-1));
	  } else {
	      crit=(i==FROM_HEADER) || (i==SENDER_HEADER) ||
		  (i==REPLY_TO_HEADER) || ((i>=TO_HEADER) && (i<=BCC_HEADER));
	      rfc2047_encode_string(trad,Header_post->k_header[i],
		      len-3,crit);
	  }
	  strcat(ecriture_courant->ligne,"\r\n");
	  len=strlen(ecriture_courant->ligne);
	  ecriture_courant->ligne=safe_realloc(ecriture_courant->ligne,
		  len+1);
	  ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
	  ecriture_courant=ecriture_courant->suivant;
      }
   for (i=0; i<NB_RAW_HEADERS; i++) 
      if (Header_post->k_raw_header[i]) {
	  size_t len;
	  if (i==REFERENCES_HEADER) {
	      if (par_mail==1) continue;
	      if (par_mail==2) ecriture_courant->cas_mail=4;
	  }
	  len=Headers[i+NB_DECODED_HEADERS].header_len+8+
	      strlen(Header_post->k_raw_header[i]);
	  ecriture_courant->ligne=safe_malloc(len+1);
	  snprintf(ecriture_courant->ligne,len+1,
		  "%s%s %s\r\n",(ecriture_courant->cas_mail>2 ? "X-" : ""),
		  Headers[i+NB_DECODED_HEADERS].header,
		  Header_post->k_raw_header[i]);
	  ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
	  ecriture_courant=ecriture_courant->suivant;
      }
   liste=Header_post->autres;
   while (liste) {
	  int rc;
	  char *trad;
	  size_t len;
#ifndef MODE_EXPERT
          if ((fl_strncasecmp(liste->header_head,
		      fl_static("Control:"),8)==0) ||
	      (fl_strncasecmp(liste->header_head,
	  	      fl_static("Also-Control:"),13)==0) ||
	      (fl_strncasecmp(liste->header_head,
		      fl_static("Supersedes:"),11)==0)) {
	     	  liste=liste->next;
	  	  continue; 
	  }
      				/* pas abuser non plus */
#endif
	  if (fl_strncasecmp(liste->header_head,
		      fl_static("In-Reply-To:"),12)==0) {
	      if (par_mail==0) { liste=liste->next; continue; }
	      if (par_mail==2) ecriture_courant->cas_mail=3;
	  }
	  /* FIXME: nombre absurde */
	  len=fl_strlen(liste->header_head)*2+145+
	      fl_strlen(liste->header_body)*2;
	  ecriture_courant->ligne=safe_malloc(len+1);
	  if (ecriture_courant->cas_mail>2) {
	      strcpy(ecriture_courant->ligne,"X-");
	      trad=ecriture_courant->ligne+2;
	  } else trad=ecriture_courant->ligne;
	  rc=conversion_to_utf8(liste->header_head,
		  &trad, len-6, (size_t)(-1));
	  {
	      size_t l=strlen(trad);
	      trad+=l;
	      len-=l;
	  }
	  strcpy(trad," "); trad++; len--;
	  rfc2047_encode_string(trad,liste->header_body,
		      len-3,0);
	  strcat(ecriture_courant->ligne,"\r\n");
	  len=strlen(ecriture_courant->ligne);
	  ecriture_courant->ligne=safe_realloc(ecriture_courant->ligne,
		  len+1);
	  ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
	  ecriture_courant=ecriture_courant->suivant;
          liste=liste->next;
   }
   if (to_cancel) {
      ecriture_courant->ligne=safe_malloc(20+strlen(to_cancel));
      strcpy(ecriture_courant->ligne,
	      (supersedes ? "Supersedes: " : "Control: cancel "));
      strcat(ecriture_courant->ligne, to_cancel);
      strcat(ecriture_courant->ligne, "\r\n");
      ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
      ecriture_courant=ecriture_courant->suivant;
      liste=liste->next;
   }
   
   ecriture_courant->ligne=safe_strdup("\r\n");
   ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
   ecriture_courant=ecriture_courant->suivant;

   if (debug) {
       fprintf(stderr, "Fin de l'écriture des headers : \n");
       ecriture_courant=Deb_article;
       while (ecriture_courant->suivant) {
	   fprintf(stderr,"%s",ecriture_courant->ligne);
	   ecriture_courant=ecriture_courant->suivant;
       }
   }


  /* Copie du corps du message */
   lecture_courant=Deb_body;
   elen=0;
   while (lecture_courant->lu[place_lu]!='\0') {
      if (lecture_courant->lu[place_lu]=='\n') {
	  deb_ligne=1;
	  if (ecriture_courant->ligne) {
	    ecriture_courant->ligne=
		safe_realloc(ecriture_courant->ligne,elen+3);
	    strcat(ecriture_courant->ligne,"\r\n");
	  } else ecriture_courant->ligne=safe_strdup("\r\n");
	  ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
	  ecriture_courant=ecriture_courant->suivant;
	  elen=0;
	  place_lu++; if (place_lu==lecture_courant->size) {
	     lecture_courant=lecture_courant->suivant;
	     if (lecture_courant==NULL) break;
	     place_lu=0;
	  }
	  continue;
      }
      else if ((lecture_courant->lu[place_lu]==fl_static('.')) &&
	      (deb_ligne)) {
	  ecriture_courant->ligne=safe_strdup(".");
	  elen=1;
      }	
      {
	 flrn_char *buf;
	 int rc; char *trad;
	 size_t bla;
	 buf=fl_strchr(lecture_courant->lu+place_lu,fl_static('\n'));
	 if (buf) 
	    bla=buf-(lecture_courant->lu+place_lu);
	 else bla=strlen(lecture_courant->lu+place_lu);
   	 rc=conversion_to_message(lecture_courant->lu+place_lu,
			&trad,0,bla);
	 elen+=strlen(trad)+(buf ? 3 : 1);
	 if (ecriture_courant->ligne) {
		ecriture_courant->ligne=
		    safe_realloc(ecriture_courant->ligne,elen+1);
		strcat(ecriture_courant->ligne,
			trad);
	 } else {
		ecriture_courant->ligne=safe_malloc(elen+1);
		strcpy(ecriture_courant->ligne, trad);
	 }
	 if (buf) {
	    strcat(ecriture_courant->ligne,"\r\n");
	    ecriture_courant->suivant=safe_calloc
		    (1,sizeof(struct post_lecture));
	    ecriture_courant=ecriture_courant->suivant;
	    elen=0;
	    if (*(buf+1)) place_lu+=bla+1;
	    else {
		lecture_courant=lecture_courant->suivant;
		place_lu=0;
		if (lecture_courant==NULL) break;
	    }
	    deb_ligne=1;
	 } else {
	     lecture_courant=lecture_courant->suivant;
	     place_lu=0;
	     if (lecture_courant==NULL) break;
	     deb_ligne=0;
	 }
     }	
   }
   if (ecriture_courant->ligne) {
       ecriture_courant->ligne=
	           safe_realloc(ecriture_courant->ligne,elen+3);
       strcat(ecriture_courant->ligne,"\r\n");
       ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
       ecriture_courant=ecriture_courant->suivant;
   }
   ecriture_courant->ligne=safe_strdup(".\r\n");
   if (debug) fprintf(stderr, "Fin de l'appel... \n");
   return 0;
}

static void Save_failed_initial_article() {
    FILE *tmp_file;
    Lecture_List *lecture_courant;
    char name[MAX_PATH_LEN];

    tmp_file=open_postfile(REJECT_POST_FILE,"w",name,0);
    if (tmp_file==NULL) return;
    lecture_courant=Deb_body;
    while (lecture_courant->suivant) lecture_courant=lecture_courant->suivant;
    Copie_prepost(tmp_file, lecture_courant, lecture_courant->size, 0);
    fclose(tmp_file);
}

static void Save_failed_article() {
    FILE *tmp_file;
    struct post_lecture *lecture_courant;
    char name[MAX_PATH_LEN];
    
    tmp_file=open_postfile(REJECT_POST_FILE,"w",name,0);
    if (tmp_file==NULL) return;
    lecture_courant=Deb_article;
    while (lecture_courant) {
	if (lecture_courant->cas_mail!=2) 
	    fputs(lecture_courant->ligne+
		    2*(lecture_courant->cas_mail==4),tmp_file);
	lecture_courant=lecture_courant->suivant;
    }
    fclose(tmp_file);
}

/* Poste un mail */
/* Retour : 1 : bon */
/* 	   -1 : pas bon */
static int Mail_article() {
   struct post_lecture *a_ecrire=Deb_article;
   int fd, res;
   size_t len;
   char name[MAX_PATH_LEN];
   char *bla;

   fd=Pipe_Msg_Start (1,0,MAILER_CMD,name); 
   	/* pas de stdout : sendmail envoie tout */
   	/* par mail...				*/
   if (fd==-1) return -1;
   do {
     if (a_ecrire->cas_mail==1) {
	 a_ecrire=a_ecrire->suivant;
	 if (a_ecrire==NULL) break;
	 continue;
     } else if (a_ecrire->cas_mail==3) {
	 bla=a_ecrire->ligne+2;
     } else bla=a_ecrire->ligne;
     len=strlen(bla);
     res=write(fd,bla,len);
     if (res!=len) return -1;
     a_ecrire=a_ecrire->suivant;
   } while (a_ecrire);
   Pipe_Msg_Stop(fd);
#ifdef USE_MKSTEMP
   unlink(name);
#endif
   return 1;
}

/* Poste franchement l'article         */
/* Peut-être à placer dans flrn_tcp ?  */
/* retour en cas d'échec :	       */
/* -1 : échec en lecture/écriture      */
/* -2 : post interdit		       */
/* -3 : pas de newsgroups valides      */
/* -4 : manque From		       */
/* -5 : manque Newsgroups	       */
/* -6 : manque Subject		       */
/* -7 : pas de corps (impossible)      */
/* -8 : article vide		       */
/* -9 : ligne trop longue	       */
/* -10 : adresse sous un format invalide */
/* -11 : pas autorisé */
/* -12 : post dans le futur */
/* -13 : erreur inconnue	       */
static int Post_article() {
    struct post_lecture  *a_ecrire=Deb_article;
    int res;
    size_t len;
    char *buf,*bla;
    
    Screen_set_color(FIELD_NORMAL);
    res=write_command(CMD_POST, 0, NULL);
    if (res<0) return -1;

    res=return_code();
    if ((res<0) || (res>400)) return ((res<0) || (res>499) ? -1 : -2);
    
    do {
       if (a_ecrire->cas_mail==2) {
	   a_ecrire=a_ecrire->suivant;
	   if (a_ecrire==NULL) break;
	   continue;
       } else if (a_ecrire->cas_mail==4) {
	   bla=a_ecrire->ligne+2;
       } else bla=a_ecrire->ligne;
       len=strlen(bla);
       res=raw_write_server(bla, len);
       if (res!=len) return -1; /* on laisse tomber de toute facon */
       a_ecrire=a_ecrire->suivant;
    } while (a_ecrire);

    /* On n'utilise pas return_code pour le parsing */
    /* de toute facon, un timeout est exclu */
    res=read_server_with_reconnect(tcp_line_read, 3, MAX_READ_SIZE-1);
    if (res<3) return -1;
    if (debug) fprintf(stderr, "Lecture : %s", tcp_line_read);
    res=strtol(tcp_line_read, &buf, 10);
    if (res==240) return 1; 
    /* On suppose que res==441 */
    if (strstr(buf, "future")) return -12;
    if (strstr(buf, "allowed")) return -11;
    if (strstr(buf, "Internet")) return -10;
    if (strstr(buf, "too long")) return -9;
    if (strstr(buf, "empty")) return -8;
    if (strstr(buf, "no body")) return -7;
    if (strstr(buf, "Subject")) return -6;
    if ((strstr(buf, "valid")!=NULL) || (strstr(buf,"such")!=NULL)) return -3;
    if (strstr(buf, "Newsgroups")) return -5;
    if (strstr(buf, "From")) return -4;
    return -13;
}


/* Libere la memoire allouee */
static void Libere_listes() {
   struct post_lecture  *pc=Deb_article, *pc2;

   while (pc) {
       if (pc->ligne) free(pc->ligne);
       pc2=pc;
       pc=pc->suivant;
       free(pc2);
   }
   free_chaine(Deb_body);
}


/* Place l'adresse e-mail de l'utilisateur dans str*/
/* taille maxi admise : 256 + length(nom) + 1 */
void Get_user_address(flrn_char *str) {
   flrn_char hostname[256];
   char *buf1;
 
   strcpy(str, flrn_user->pw_name);
   strcat(str, "@");
#ifdef DEFAULT_HOST
#ifdef DOMAIN
   strncpy(hostname, DEFAULT_HOST, 255- strlen(DOMAIN));
#else
   strncpy(hostname, DEFAULT_HOST, 255-
      (Options.default_domain==NULL ? 12 : strlen(Options.default_domain)));
#endif
#else
#ifdef HAVE_GETHOSTNAME
   gethostname(hostname,255-
#ifdef DOMAIN
      strlen(DOMAIN)
#else
      (Options.default_domain==NULL ? 12 : strlen(Options.default_domain))
#endif
   );  /* PAS de test d'erreur */
#else
#error Vous devez definir DEFAULT_HOST dans flrn_config.h
#endif
#endif
  /* hostname, sur naoned au moins, renvoie naoned.ens.fr */
  /* Pour les autres machines, je sais pas. Ca semble pas */
  /* clair sur fregate.					  */
#ifdef DOMAIN
   buf1=strchr(hostname,'.');
   if (buf1) *(++buf1)='\0'; else strcat(hostname,".");
   strcat(hostname, DOMAIN);  
#else
   if (Options.default_domain) {
      buf1=strchr(hostname,'.');
      if (buf1) *(++buf1)='\0'; else strcat(hostname,".");
      strcat(hostname, Options.default_domain);
   } else
   if (strchr(hostname,'.')==NULL) strcat(hostname,".invalid");
       /* hostname incorrect */
#endif
   strcat(str, hostname);
}

/* Extration des references pour le header References du post */
/* len_id donne la taille à réserver en plus pour le msgid du pere */
/* get_unbroken_id est une fonction utilisée dans extract_post_references */
static char *get_unbroken_id (char *buf, char **end) {
   char *buf2;

   while ((buf=strchr(buf,'<'))) {
     buf2=strpbrk(buf+1,">< \t\r\n");
     if (buf2==NULL) return NULL; /* Broken MID */
     if (*buf2!='>') {  /* Broken MID */
         buf=buf2;      /* we try a new one */
	 continue; 
     }
     *end=buf2;
     return buf;
   }
   return NULL;
}
     
static char *extract_post_references (char *str, int len_id) {
   int len=0;
#ifndef GNKSA_REFERENCES_HEADER
   int i,nb_ref=0;
#endif
   char *prem_mid=NULL, *end_prem_mid=NULL;
   char *buf, *buf2=str, *res;
   
   /* On calcule le nombre de references */
   while ((buf=get_unbroken_id(buf2, &buf2))) {
     len+=buf2-buf+2; /* L'espace en plus */
#ifndef GNKSA_REFERENCES_HEADER
     nb_ref++;
#endif
     if (prem_mid==NULL) {
        prem_mid=buf;
	end_prem_mid=buf2;
     }
   }
#ifndef GNKSA_REFERENCES_HEADER
   if (nb_ref<MAX_NB_REF) {  /* On se pose pas de questions */
     res=safe_malloc((len+len_id+2)*sizeof(char));
     *res='\0';
     buf2=str;
     while ((buf=get_unbroken_id(buf2, &buf2))) {
       strncat(res,buf,buf2-buf+1);
       strcat(res," ");
     }
     return res;
   }
   buf2=end_prem_mid;
   for (i=1; i<nb_ref+2-MAX_NB_REF; i++) { 
     buf=get_unbroken_id (buf2, &buf2);
     if (buf==NULL) /* Argh ! */
        break;
     len-=buf2-buf+2;
   }
#else
   if (prem_mid==NULL) {
     res=safe_malloc((len_id+2)*sizeof(char));
     *res='\0';
     return res;
   }
   buf=buf2=end_prem_mid;
   while (len+len_id+2>986) { /* 998 - 12 pour la chaîne "References: " */
      buf=get_unbroken_id (buf2, &buf2);
      if (buf==NULL) /* Argh ! */
         break;
      len-=buf2-buf+2;
   }
#endif
   res=safe_malloc((len+len_id+2)*sizeof(char));
   strncpy(res,prem_mid,end_prem_mid-prem_mid+1);
   res[end_prem_mid-prem_mid+1]='\0';
   strcat(res," ");
   if (buf==NULL) return res;
   while ((buf=get_unbroken_id(buf2, &buf2))) {
      strncat(res,buf,buf2-buf+1);
      strcat(res," ");
   }
   return res;

}


/* Creation des headers du futur article */
/* Flag : 0 : normal 1 : mail -1 : cancel ou supersedes */
static int Get_base_headers_supersedes (Article_List *article) {
   int i;
   Header_List *tmp, *tmp2, *liste;

   if (Last_head_cmd.Article_vu!=article) {
      cree_header(article,0,1,0);
      if (article->headers==NULL) {
	  if (debug) fprintf(stderr,"Cree-headers a renvoye un resultat beurk !\n");
	  return -1;
      }
   }
   /* on garde tout, sauf X-ref, Path, NNTP-Posting_Host,
      	     X-Trace, NNTP-Posting-Date, X-Complaints-To et Message-ID, To, Cc, Bcc */
   for (i=0;i<NB_DECODED_HEADERS;i++) {
      if (i==XREF_HEADER) continue;
      if (i==TO_HEADER) continue;
      if (i==CC_HEADER) continue;
      if (i==BCC_HEADER) continue;
      if (i==X_TRACE_HEADER) continue;
      if (article->headers->k_headers[i])
          Header_post->k_header[i]=safe_strdup(article->headers->k_headers[i]);
   }
   Header_post->k_raw_header[REFERENCES_HEADER]=
       safe_strdup(article->headers->k_raw_headers[REFERENCES_HEADER]);

   tmp=Last_head_cmd.headers;
   liste=Header_post->autres;
   if (liste) while (liste->next) liste=liste->next;
   while (tmp) {
      tmp2=tmp; tmp=tmp->next;
      if (fl_strncasecmp(tmp2->header_head,
		  fl_static("Path:"),5)==0) continue;
      if (fl_strncasecmp(tmp2->header_head,
		  fl_static("NNTP-Posting-Host:"),18)==0) continue;
      if (fl_strncasecmp(tmp2->header_head,
		  fl_static("NNTP-Posting-Date:"),18)==0) continue;
      if (fl_strncasecmp(tmp2->header_head,
		  fl_static("X-Complaints-To:"),16)==0) continue;
      if (liste==NULL) 
         liste=Header_post->autres=safe_malloc(sizeof(Header_List));
      else {
         liste->next=safe_malloc(sizeof(Header_List));
	 liste=liste->next;
      }
      liste->next=NULL;
      liste->header_head=safe_flstrdup(tmp2->header_head);
      liste->header_body=safe_flstrdup(tmp2->header_body);
      liste->num_af=0;
   }
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   return 0;
}
   
static int Get_base_headers(int flag, Article_List *article) {
   int res, len1, len2=0, len3, key, i, from_perso=0;
   char *real_name;
   flrn_char *buf, *buf2;
   string_list_type *parcours;

   Header_post=safe_calloc(1,sizeof(Post_Headers));
   memset(Header_post,0,sizeof(Post_Headers));
   par_mail=(flag==1);
   /* le supersedes est traité à part */
   if (supersedes) {
      return Get_base_headers_supersedes(article);
   }
   /* On va d'abord gérer les headers persos... */
   /* Comme ca ils vont se faire écraser par les autres... */
   /* ces headers sont bien formatés... mais ils peuvent etre "reformates" */
   if (flag!=-1) {
      parcours=Options.user_header;
      while (parcours) {
        buf=fl_strchr(parcours->str,fl_static(':'));
	if (buf==NULL) { parcours = parcours->next; continue; }
	len1=buf-parcours->str+1;
	for (i=0;i<NB_DECODED_HEADERS;i++) 
	  if (fl_strncasecmp(parcours->str,Headers[i].header,
	                          Headers[i].header_len)==0) break;
        if ((i!=NB_DECODED_HEADERS) && (is_modifiable(i))) 
	  /* c'est a revoir... */
	{
	   Header_post->k_header[i]=safe_malloc(513*sizeof(flrn_char));
	   Copy_format (NULL, buf+2, article, Header_post->k_header[i], 512);
	   /* on reformate */
	   buf2=Header_post->k_header[i];
	   while ((buf2=fl_strchr(buf2,fl_static('\n')))) 
	       *buf2=fl_static(' ');
	   if (i==FROM_HEADER) from_perso=1;
	}
	else if (i==NB_DECODED_HEADERS) {
	   Header_List *parcours2=Header_post->autres;
	   while (parcours2 && (parcours2->next)) parcours2=parcours2->next;
	   if (Header_post->autres==NULL) 
	      Header_post->autres=parcours2=safe_malloc(sizeof(Header_List));
	   else { 
	     parcours2->next=safe_malloc(sizeof(Header_List));
	     parcours2=parcours2->next;
	   }
	   parcours2->next=NULL;
	   parcours2->header_head=safe_malloc((len1+1)*sizeof(flrn_char));
	   strncpy(parcours2->header_head,parcours->str,len1);
	   parcours2->header_head[len1]=fl_static('\0');
	   parcours2->header_head=safe_malloc(513*sizeof(flrn_char));
	   Copy_format (NULL, buf+2, article, 
	                   parcours2->header_head, 512);
	   /* on reformate */
	   buf2=parcours2->header_head;
	   while ((buf2=fl_strchr(buf2,fl_static('\n')))) 
	       *buf2=fl_static(' ');
	   parcours2->num_af=0;
	}
	parcours=parcours->next;
      }
   }
   if (Pere_post) { 
      if ((Pere_post->headers==NULL)||(Pere_post->headers->all_headers==0))
	cree_header(Pere_post,0,0,0);
      if (Pere_post->headers==NULL) {
	   if (debug) fprintf(stderr,"Cree-headers a renvoye un resultat beurk !\n");
	   return -1;
      }
      /* Determiner si on doit faire la réponse par mail en priorité */
      if ((flag==0) && (Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER])) {
        if  (fl_strcasecmp(Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER],
		    fl_static("poster"))==0) {
	  while (flag==0) {
	    struct key_entry ke;
	    ke.entry=ENTRY_ERROR_KEY;
	    Cursor_gotorc(Screen_Rows2-1,0);
	    /* FIXME : français */
	    Screen_write_string("Répondre par mail (O/N/A) ? ");
	    Attend_touche(&ke);
	    if (KeyBoard_Quit) return -1;
	    if (ke.entry==ENTRY_SLANG_KEY) {
		key=ke.value.slang_key;
	        key=toupper(key);
		if (key=='A') return -1;
		if (key=='O') flag=1;
		if (key=='N') break;
	    }
	  }
	  par_mail=flag; /* 0 ou 1 */
        } else if (fl_strcasecmp(Newsgroup_courant->name,
		    Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER])!=0) { 
			/* On ne demande rien si on est dans le bon groupe */
	  while (1) {
	    struct key_entry ke;
	    ke.entry=ENTRY_ERROR_KEY;
	    Cursor_gotorc(Screen_Rows2-1,0);
	    /* FIXME: français */
	    Screen_write_string("Suivre le followup (O/N/A) ? ");
	    Attend_touche(&ke);
	    if (KeyBoard_Quit) return -1;
	    if (ke.entry==ENTRY_SLANG_KEY) {
		key=ke.value.slang_key; 
		key=toupper(key);
		if (key=='A') return -1;
		if (key=='O') {
		    Header_post->k_header[NEWSGROUPS_HEADER]=
			safe_flstrdup(Pere_post->headers->k_headers
				[FOLLOWUP_TO_HEADER]);
		    break;
		}
		if (key=='N') break;	  
	    }
	  }
        } 
      }
      /* References */
      len3=strlen(Pere_post->msgid);
      if (Pere_post->headers->k_raw_headers[REFERENCES_HEADER]) 
	     Header_post->k_raw_header[REFERENCES_HEADER]=extract_post_references(Pere_post->headers->k_raw_headers[REFERENCES_HEADER], len3);
      else { 
	  Header_post->k_raw_header[REFERENCES_HEADER]=safe_malloc(len3+1);
	  *(Header_post->k_raw_header[REFERENCES_HEADER])='\0';
      }
      strcat(Header_post->k_raw_header[REFERENCES_HEADER], Pere_post->msgid);
      /* Sujet */
      if (Pere_post->headers->k_headers[SUBJECT_HEADER]) {
	  len1=fl_strlen(Pere_post->headers->k_headers[SUBJECT_HEADER]);
	  if (fl_strncasecmp(Pere_post->headers->k_headers[SUBJECT_HEADER],
		      fl_static("re: "),4)!=0)
	    len2=4; else len2=0;
	  Header_post->k_header[SUBJECT_HEADER]=
		       safe_malloc((len1+len2+1)*sizeof(flrn_char));
	  if (len2==4)
	    fl_strcpy(Header_post->k_header[SUBJECT_HEADER], 
		    fl_static("Re: ")); else
	      Header_post->k_header[SUBJECT_HEADER][0]=fl_static('\0');
	  fl_strcat(Header_post->k_header[SUBJECT_HEADER],
	      Pere_post->headers->k_headers[SUBJECT_HEADER]);
      }
   }
   /* Newsgroups */
   if (Header_post->k_header[NEWSGROUPS_HEADER]==NULL) {
#ifdef GNKSA_NEWSGROUPS_HEADER
	 if  ((!Pere_post) ||
	         (!(Pere_post->headers->k_headers[NEWSGROUPS_HEADER])))
#endif
         {
	    if (Newsgroup_courant->flags & GROUP_READONLY_FLAG) 
	        Header_post->k_header[NEWSGROUPS_HEADER]=safe_flstrdup(
			fl_static("junk"));
	    else
	 	Header_post->k_header[NEWSGROUPS_HEADER]=safe_flstrdup
		    (Newsgroup_courant->name);
	 } 
#ifdef GNKSA_NEWSGROUPS_HEADER
	 else
	    Header_post->k_header[NEWSGROUPS_HEADER]=safe_strdup(Pere_post->headers->k_headers[NEWSGROUPS_HEADER]);
#endif
   }
   /* Headers To: et In-Reply-To: */
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   if (Pere_post) { /* on crée un header REPLY_TO de toute façon */
     int rc;
     char *trad;
     Header_List *parcours2=Header_post->autres;

     if (Pere_post->headers->k_headers[REPLY_TO_HEADER])
       Header_post->k_header[TO_HEADER]=safe_flstrdup(Pere_post->headers->k_headers[REPLY_TO_HEADER]); else
       Header_post->k_header[TO_HEADER]=safe_flstrdup(Pere_post->headers->k_headers[FROM_HEADER]); 
     while (parcours2 && (parcours2->next)) parcours2=parcours2->next;
     if (parcours2) {
       parcours2->next=safe_malloc(sizeof(Header_List));
       parcours2=parcours2->next;
     } else Header_post->autres=parcours2=safe_malloc(sizeof(Header_List));
     parcours2->next=NULL;
     parcours2->header_head=safe_flstrdup(fl_static("In-Reply-To:"));
     parcours2->header_body=safe_flstrdup(fl_static_tran(Pere_post->msgid));
     parcours2->num_af=0;
     Screen_write_string("To : ");
     rc=conversion_to_terminal(Header_post->k_header[TO_HEADER],
	     &trad,0,(size_t)(-1));
     Screen_write_string(trad);
     if (rc==0) free(trad);
   } else {
     int rc;
     char *trad;
     Screen_write_string("Groupe : ");
     rc=conversion_to_terminal(Header_post->k_header[NEWSGROUPS_HEADER],
	     &trad,0,(size_t)(-1));
     Screen_write_string(trad);
     if (rc==0) free(trad);
   }
   /* Sujet */
   if (Header_post->k_header[SUBJECT_HEADER]==NULL) {
       Header_post->k_header[SUBJECT_HEADER]=
	   safe_calloc((MAX_SUJET_LEN+1),sizeof(flrn_char));
     if (flag==-1) fl_strcpy(Header_post->k_header[SUBJECT_HEADER],
	     fl_static("Cancel")); else
     {
       res=get_Sujet_post(Header_post->k_header[SUBJECT_HEADER]); 
       if (res==-1) return -1;
     }
   } 
   /* From */
   if (from_perso==0)
   {
     char *bf;
     real_name=safe_strdup(flrn_user->pw_gecos);
     bf=strchr(real_name,','); if (bf) *bf='\0';
     len1=257+strlen(flrn_user->pw_name);
     if (Options.post_name)
       len2=4+fl_strlen(Options.post_name);
     else
       len2=4+strlen(real_name)*2;
     Header_post->k_header[FROM_HEADER]=safe_malloc((len1+len2)*
	     sizeof(flrn_char));
     Get_user_address(Header_post->k_header[FROM_HEADER]);
     fl_strcat(Header_post->k_header[FROM_HEADER],fl_static(" ("));
     if (Options.post_name) 
       fl_strcat(Header_post->k_header[FROM_HEADER], Options.post_name);
     else {
	 int rc;
	 flrn_char *trad;
	 rc=conversion_from_utf8(real_name,&trad,0,(size_t)(-1));
         fl_strcat(Header_post->k_header[FROM_HEADER], trad);
	 if (rc==0) free(trad);
     }
     fl_strcat(Header_post->k_header[FROM_HEADER],fl_static(")"));
     free(real_name);
   }
   return 0;
}


/* Libération des headers de l'article */
static void Free_post_headers() {
   Header_List *liste1, *liste2;
   int i;

   if (Header_post==NULL) return;
   liste1=Header_post->autres;
   while (liste1) {
     liste2=liste1->next;
     if (liste1->header_head) free(liste1->header_head);
     if (liste1->header_body) free(liste1->header_body);
     free(liste1);
     liste1=liste2;
   }
   for (i=0; i<NB_DECODED_HEADERS; i++) {
     if (Header_post->k_header[i]) 
        free(Header_post->k_header[i]);
   }
   for (i=0; i<NB_RAW_HEADERS; i++) {
     if (Header_post->k_raw_header[i]) 
        free(Header_post->k_raw_header[i]);
   }
   free(Header_post);
}


/* encode un header selon la rfc2047, sans se poser la question		 */
/* des caractère spéciaux... (donc ne peut pour l'instant ne s'appliquer */
/* qu'au sujet...)							 */
#if 0
static void encode_headers(char **header_to_encode) {
   char *place_to_put, *header_content=*header_to_encode;

   /* On suppose la taille max a 140+2*strlen */
   place_to_put=safe_malloc(140+strlen(header_content)*2);
   rfc2047_encode_string(place_to_put, 
       (unsigned char*) header_content, 75+strlen(header_content)*2);
   *header_to_encode=safe_realloc(*header_to_encode, strlen(place_to_put)+1);
   strcpy(*header_to_encode, place_to_put);
   free(place_to_put);
}
#endif

/* Cancel un message (fonction secondaire de ce fichier).		*/
/* Renvoie : >0 si le message est effectivement cancelé... 0 si le      */
/* cancel est annulé, et <0 si le cancel est refusé, ou bug...		*/
/* si confirm=1, alors on ne demande pas confirmation et on renvoit 2   */
/* sinon, on revonoie 2 si la reponse à la confirmation est 'T'         */
int cancel_message (Article_List *origine, int confirm) {
   int res,key;
   char line[80];

   if (origine->numero==-1) return -1;
   supersedes=0;
   Pere_post=NULL; /* Pas besoin de References */
   res=Est_proprietaire(origine);
   if (res!=1) return -1;
   /* On pourrait demander confirmation */
   /* L'équivalent d'un Aff_fin */
   if (confirm==0) {
     struct key_entry ke;
     ke.entry=ENTRY_ERROR_KEY;
       /* FIXME: français */
     sprintf(line,"Canceler le message %d (O/N/T) : ",origine->numero);
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_set_color(FIELD_ERROR);
     Screen_write_string(line);
     Screen_set_color(FIELD_NORMAL);
     Screen_erase_eol();
     Cursor_gotorc(Screen_Rows2-1, strlen(line));
     Attend_touche(&ke);
     if (ke.entry==ENTRY_SLANG_KEY) {
	 key=ke.value.slang_key;
         if (key=='T') confirm=1; else if (toupper(key)!='O') return 0;
     } else return 0;
   }
   /* Bon... Tant pis */
   res=Get_base_headers(-1,origine);
   if (res==-1) {
     Free_post_headers();
     return -1;
   }
   /* On remplie un corps bidon */
   Deb_body=alloue_chaine();
   fl_strcpy(Deb_body->lu,fl_static("This message is canceled by flrn.\n"));
   Deb_body->size=fl_strlen(Deb_body->lu);
   Change_message_conversion("utf-8");
   res=Format_article(origine->msgid);
   res=Post_article();
   Free_post_headers();
   Libere_listes();
   if (res<0) return -1;
   return 1+confirm;
}


void Create_header_encoding () {
    Lecture_List *lecture_courant;
    Header_List *liste;
    const char *result;
    /* 1ere chose, existe-t-il une entête ContentType */
    liste=Header_post->autres;
    if (liste==NULL) {
	liste=Header_post->autres=safe_calloc(1,sizeof(Header_List));
    } else
    while (liste) {
	if (fl_strncasecmp(liste->header_head,
		    fl_static("content-type:"),13)==0) break;
	if (liste->next==NULL) {
	    liste->next=safe_calloc(1,sizeof(Header_List));
	    liste=liste->next;
	    break;
	}
	liste=liste->next;
    }
    if (liste->header_head==NULL) 
	liste->header_head=safe_flstrdup(fl_static("Content-Type:"));
    if (liste->header_body) {
	const char *tent;
	tent=parse_ContentType_header(liste->header_body);
	lecture_courant=Deb_body;
	while (lecture_courant) {  
	    find_best_conversion(lecture_courant->lu,lecture_courant->size,
		    &result,tent);
	    if (result==NULL) break;
	    lecture_courant=lecture_courant->suivant;
	}
	find_best_conversion(NULL,0,NULL,NULL);
	if (result) return; /* parse_ContentType_header a mis le
			       message au bon encoding */
	free(liste->header_body);
	liste->header_body=NULL;
    }
    lecture_courant=Deb_body;
    while (lecture_courant) {
	find_best_conversion(lecture_courant->lu,lecture_courant->size,
		&result,NULL);
	if (result==NULL) break;
	lecture_courant=lecture_courant->suivant;
    }
    if (result==NULL) result="utf-8";
    {
	char *bla;
	size_t len;
	bla=strchr(result,':');
	if (bla) len=bla-result; else len=strlen(result);
	bla=safe_malloc(len+1);
	strncpy(bla,result,len); bla[len]='\0';
	Change_message_conversion(bla);
	liste->header_body=safe_malloc((25+strlen(bla))*sizeof(flrn_char));
	fl_strcpy(liste->header_body,"text/plain; charset=");
	fl_strcat(liste->header_body,fl_static_tran(bla));
	free(bla);
    }
}

/* Poste un message (fonction principale de ce fichier).		*/
/* Renvoie : >0 en cas de message posté, 0 en cas d'annulation, <0 en   */
/* en cas d'erreur.							*/
/* renvoie positifs : & 1 : post envoyé... & 2 : mail envoyé...		*/
/* flag est à 1 si demande de réponse par mail, a -1 si supersedes	*/
int post_message (Article_List *origine, flrn_char *name_file,int flag) {
    int res=1, res_post=0, res_mail=0;
    flrn_char *str=name_file;

    if (origine && (origine->numero<0)) {
	/* FIXME : français */
       Aff_error_fin(fl_static("L'article est hors du newsgroup !"),1,-1);
       return -1;
    }
    if (flag==-1) {
      res=Est_proprietaire(origine);
      if (res!=1) {
	/* FIXME : français */
	Aff_error_fin(fl_static("Supersedes interdit !"),1,-1);
        return -1;
      }
    }
    supersedes=(flag==-1);
    Pere_post=origine;
    sigwinch_received=0;
    Deb_article=NULL;
    Deb_body=NULL;
    /* On détermine les headers par défaut de l'article */
    /* D'ou l'intérêt de Followup-to			*/
    res=Get_base_headers(flag,origine);
    if (res==-1) {
      Free_post_headers();
       return 0;
    }
    while ((*str) && (fl_isblank(*str))) str++;
    if (*str!=fl_static('\0')) { 
       char *trad;
       int rc;
       rc=conversion_to_file(str,&trad,0,(size_t)(-1));
       res=charge_postfile(trad);
       if (rc==0) free(trad);
       if (res==0) return -1;
    } else {
      if (Pere_post) {
        Cursor_gotorc(2,0);
	/* FIXME : français, mais de toute façon si l'éditeur */
	/* est supprimé ça peut sauter */
        if (flag!=-1) Screen_write_string("Votre réponse : "); else
	  Screen_write_string("Votre article de remplacement : ");
      }
      res=get_Body_post();
      Screen_set_screen_start(NULL,NULL);
    }
#ifdef GNKSA_ANALYSE_POSTS
    if (!Header_post->k_header[SUBJECT_HEADER]) {
      Save_failed_initial_article();
      Cursor_gotorc(1,0);
      Screen_erase_eos();
      Screen_set_color(FIELD_ERROR);
      Cursor_gotorc(Screen_Rows/2-1,3); /* Valeurs au pif */
      /* FIXME : français */
      Screen_write_string("Ce message n'a pas de sujet. Post annulé.");
      Cursor_gotorc(Screen_Rows/2,3);
      /* FIXME : français */
      Screen_write_string("Message sauvé dans ~/");
      Screen_write_string(REJECT_POST_FILE);
      res=-1;
    }
#endif
    if (res<=0) { 
	if (res==0) Save_failed_initial_article(); /* on sauve un 
					  éventuel article annulé */
	/* TODO ajouter un message là-dessus */
	Free_post_headers(); Libere_listes(); return res; 
    }
    Create_header_encoding();
    /* maintenant, il faut trouver le bon encodage */
    res=Format_article((supersedes ? origine->msgid : NULL));
    if (res<0) { Save_failed_initial_article();
	Free_post_headers(); Libere_listes(); return 0; }
    if ((par_mail) && ((Header_post->k_header[TO_HEADER] ||
	Header_post->k_header[CC_HEADER] ||
	Header_post->k_header[BCC_HEADER]))) res_mail=Mail_article();
    if ((par_mail!=1) && (Header_post->k_header[NEWSGROUPS_HEADER])) 
         res_post=Post_article(); 
	 /* Si on ne veut pas répondre par mail, on ne répond pas par mail...*/
    if ((res_post<0) || (res_mail<0)) Save_failed_article();
    if (res_post<0) {
      /* on reecrit un Aff_error :( */
      Cursor_gotorc(1,0);
      Screen_erase_eos();
      Screen_set_color(FIELD_ERROR);
      Cursor_gotorc(Screen_Rows/2-1,3); /* Valeurs au pif */
      /* FIXME : français */
      Screen_write_string("Post refusé... L'article est sauvé dans ~/");
      Screen_write_string(REJECT_POST_FILE);
      Cursor_gotorc(Screen_Rows/2,3);
      Screen_write_string("Raison du refus : ");
      switch (res_post) {
	case -1 : Screen_write_string("erreur GRAVE en lecture/ecriture."); 
		  break;
	case -2 : Screen_write_string("pas le droit de poster.");
		  break;
	case -3 : Screen_write_string("les newsgroups ne sont pas valides");
		  break;
	case -4 : Screen_write_string("header From manquant.");
		  break;
	case -5 : Screen_write_string("header Newsgroups manquant ou  invalide.");
		  break;
	case -6 : Screen_write_string("pas de sujet.");
		  break;
	case -7 : Screen_write_string("que des headers ???");
		  break;
	case -8 : Screen_write_string("le message est vide.");
		  break;
	case -9 : Screen_write_string("une ligne est trop longue.");
		  break;
        case -10: Screen_write_string("Un header contient une adresse invalide.");	break;
        case -11: Screen_write_string("Post non autorisé.");	
		  break;
	case -12: Screen_write_string("problème de date avec le serveur");
	default : Screen_write_string("inconnu : ");
		  break;
      }
      Cursor_gotorc(Screen_Rows/2+1,3);
      Screen_write_nstring(tcp_line_read,strlen(tcp_line_read)-2);
      Screen_set_color(FIELD_NORMAL);
    } else if (res_mail<0) {
	 Aff_error("Mail refusé... L'article est sauvé dans ");
         Screen_write_string(REJECT_POST_FILE);
    }
    Free_post_headers();
    Libere_listes();
    return ((res_post<0) || (res_mail<0) ? -1 : res_post+res_mail*2);
/* Bon, la fin est un peu rapide. Mais on verra bien. */
}
