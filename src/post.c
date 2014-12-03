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
#include "enc_strings.h"

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

struct post_body {
    struct post_body *suivant;
    flrn_char *ligne;
};

struct post_lecture *Deb_article;
struct post_body *Deb_body;
Article_List *Pere_post;
int par_mail, supersedes;
/* par_mail = 0 : juste les groupes. =1 : juste par mail. =2 : poste aussi */
int in_moderated_group;

Post_Headers *Header_post;

/* Obtention du sujet d'un post. Pas de formatage pour l'instant */
/* retour : 0 en cas de succes, -1 en cas d'annulation.		 */
static int get_Sujet_post(flrn_char *str) {
   int res,col;
   char affstr[MAX_SUJET_LEN+1];
   
   Cursor_gotorc(2,0);
   col=put_string_utf8(_("Sujet : "));
   do {
     str[0]=fl_static('\0');
     affstr[0]='\0';
     res=flrn_getline(str,MAX_SUJET_LEN,affstr,MAX_SUJET_LEN,2,col); 
   } while (res==-2);   /* un backspace en debut de ligne ne fait rien */
   return res; 
}

/* Copie dans le fichier temporaire le message tel qu'on suppose qu'il   */
/* sera.								 */
void Copie_prepost (FILE *tmp_file, int incl) {
   Header_List *liste;
   struct post_body *lecture_courant;
   int rc=1; char *trad;
  
   if (Options.edit_all_headers) {
      int i;
      for (i=NB_KNOWN_HEADERS-1; i>=0; i--) {
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
	 rc=conversion_to_editor(liste->header_body,&trad,
		 0,(size_t)(-1));
	 fputs(trad,tmp_file);
	 if (rc==0) free(trad);
	 putc('\n',tmp_file);
      }
   } else {
      int rc1,rc2;
      char *trad2;
      flrn_char *trad1;
      rc1=conversion_from_utf8(_("Groupes: "),&trad1,0,(size_t)(-1));
      rc2=conversion_to_file(trad1,&trad2,0,(size_t)(-1));
      fputs(trad2,tmp_file);
      if (rc2==0) free(trad2);
      if (rc1==0) free(trad1);
      if (Header_post->k_header[NEWSGROUPS_HEADER]) {
	  rc=conversion_to_editor(Header_post->k_header[NEWSGROUPS_HEADER],
		  &trad, 0,(size_t)(-1));
	  fputs(trad,tmp_file);
	  if (rc==0) free(trad);
      }
      putc('\n',tmp_file);
      rc1=conversion_from_utf8(_("Sujet: "),&trad1,0,(size_t)(-1));
      rc2=conversion_to_file(trad1,&trad2,0,(size_t)(-1));
      fputs(trad2,tmp_file);
      if (rc2==0) free(trad2);
      if (rc1==0) free(trad1);
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
	     rc=conversion_to_editor(liste->header_body,&trad,
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
      /* Par précaution */
      rc=conversion_to_editor(lecture_courant->ligne,&trad,0,(size_t)(-1));
      fputs(trad, tmp_file);
      if (rc==0) free(trad);
     /* J'utilise fprintf plutôt que fwrite par principe : fwrite est */
     /* a priori pour des fichiers binaires (et puis, j'ai commencé   */
     /* avec fprintf). Mais bon, on pourrait peut-être changer...     */
      lecture_courant=lecture_courant->suivant;
   } 
}

/* Lecture du fichier une fois l'édition finie */
/* Renvoie -1 si le fichier est VRAIMENT trop moche */
int Lit_Post_Edit (FILE *tmp_file) {
   char buf[1025]; /* On suppose qu'un ligne ne peut pas faire plus de 1024 */
   char *buf2, *buf3;
   char *hcur=NULL;
   flrn_char **header_courant=NULL;
   int header_connu=-1;
   int taille;
   int raw_head=0;
   struct post_body *lecture_courant,*lecture_courant2;
   int headers=1,read_something=0, i;
   Header_List *liste, *last_liste, **unk_header_courant=NULL;
   int rc; flrn_char *trad;

   lecture_courant=Deb_body;
   while (lecture_courant) {
       lecture_courant2=lecture_courant->suivant;
       if (lecture_courant->ligne) free(lecture_courant->ligne);
       free(lecture_courant);
       lecture_courant=lecture_courant2;
   }
   Deb_body=NULL;
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
			 || (header_connu==BCC_HEADER))) {
			par_mail=2;
			/* on libère l'ancien header To: , parce
			 * que le changement se fait au mieux */
			if (Header_post->k_header[TO_HEADER]) {
			    free(Header_post->k_header[TO_HEADER]);
			    Header_post->k_header[TO_HEADER]=NULL;
			}
		    }
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
	   /* un cas particulier : espace juste avant le ':' */
	   if ((buf2) && (buf3) && (*buf3==' ') && (buf2-buf3==1)) {
	       *buf2=' ';
	       *buf3=':';
	       buf3++;
	   }
	   if ((buf2==NULL) || ((buf3) && ((int)(buf3-buf2)<0)))
	      headers=0; else {
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
		 int rc1,rc2,rscmp;
		 flrn_char *trad1;
		 char *trad2;
		 if (!Options.edit_all_headers) {
		   rc1=conversion_from_utf8(_("sujet:"),&trad1,0,(size_t)(-1));
		   rc2=conversion_to_file(trad1,&trad2,0,(size_t)(-1));
		   rscmp=strncasecmp(buf,trad2, strlen(trad2));
		   if (rc2==0) free(trad2);
		   if (rc1==0) free(trad1);
	           if (rscmp==0) {
		     header_courant=&(Header_post->k_header[SUBJECT_HEADER]); 
		     header_connu=SUBJECT_HEADER;
		   } else {
		       rc1=conversion_from_utf8(_("groupes:"),
			       &trad1,0,(size_t)(-1));
		       rc2=conversion_to_file(trad1,&trad2,0,(size_t)(-1));
		       rscmp=strncasecmp(buf,trad2, strlen(trad2));
		       if (rc2==0) free(trad2);
		       if (rc1==0) free(trad1);
		       if (rscmp==0) {
			   header_courant=
			       &(Header_post->k_header[NEWSGROUPS_HEADER]);
			   header_connu=NEWSGROUPS_HEADER;
		       }
		   }
		 }
		 if (header_connu==i) {
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
      if (lecture_courant) {
	  lecture_courant->suivant=safe_calloc(1,sizeof(struct post_body));
	  lecture_courant=lecture_courant->suivant;
      } else {
	  lecture_courant=Deb_body=safe_calloc(1,sizeof(struct post_body));
      }
      if (rc==0) lecture_courant->ligne=trad;
         else lecture_courant->ligne=safe_flstrdup(trad);
   }
   return 0;
}

/* Summon_Editor : programme de préparation, d'appel, et de relecture    */
/* de l'éditeur.							 */
/* Renvoie -1 en cas de bug majeur.					 */
/* Renvoie 2 en cas de message non modifié				 */
/* Renvoie 1 en cas de message vide					 */
/* Renvoie 0 si pas (trop) de problèmes...				 */
static int Summon_Editor (int incl) {
   FILE *tmp_file;
   int res;
   struct stat before, after;
   char name[MAX_PATH_LEN];

   free_text_scroll();
   Cursor_gotorc(1,0);
   Screen_erase_eos();
   tmp_file=open_postfile(TMP_POST_FILE,"w",name,1);
   if (tmp_file==NULL) {
       put_string_utf8(_("Echec dans l'ouverture du fichier temporaire."));
       return -1;
   }
   Copie_prepost (tmp_file, incl);
   fclose(tmp_file);
   if (stat(name,&before)<0) {
      put_string_utf8(_("Echec dans la crÃ©ation du fichier temporaire."));
      return -1;
   }
   res=Launch_Editor(0,name);  
   if (res==-1) {
       put_string_utf8(_("Echec dans le lancement de l'Ã©diteur."));
       return -1;
   }
   if (stat(name,&after)<0) {
      put_string_utf8(_("Erreur en lecture du fichier temporaire."));
      return -1;
   }
   if (after.st_size == 0) {
      put_string_utf8(_("Fichier temporaire vide."));
#if (defined(USE_MKSTEMP) || defined(USE_TMPEXT))
      unlink(name);
#endif
      return 1;
   }
   if (after.st_mtime == before.st_mtime) {
       put_string_utf8(_("Message non modifiÃ©."));
#if (defined(USE_MKSTEMP) || defined(USE_TMPEXT))
       unlink(name);
#endif
       return 2;
   }
   tmp_file=fopen(name,"r");
   if (tmp_file==NULL) {
       put_string_utf8(_("Echec dans la rÃ©ouverture du fichier temporaire"));
       return -1;
   }
   res=Lit_Post_Edit(tmp_file);
   fclose (tmp_file);
#if (defined(USE_MKSTEMP) || defined(USE_TMPEXT))
   unlink(name);
#endif
   return res;
}


/* Prend le message à poster à partir d'un fichier...			 */
/* renvoie 0 en cas d'echec...						 */
int charge_postfile (char *str) {
   int res;
   FILE *post_file;

   Deb_body=NULL;
   post_file=fopen(str,"r");
   if (post_file==NULL) {
       Aff_error_utf8(_("Echec en ouverture du fichier"));
       return 0;
   }
   res=Lit_Post_Edit(post_file);
   if (res<0) {
       Aff_error_utf8(_("Fichier incomprÃ©hensible..."));
       return 0;
   }
   fclose(post_file);
   return 1;
}


/* Afficher des informations sur le message à venir */
static void aff_info_messages() {
    int row;
    int i;
    flrn_char *str;
    Cursor_gotorc(1,0);
    Screen_erase_eos();
    str=safe_malloc((12+(Header_post->k_header[SUBJECT_HEADER] ? 
		fl_strlen(Header_post->k_header[SUBJECT_HEADER]) : 0))
	    *sizeof(flrn_char));
    fl_strcpy(str,fl_static_tran(Headers[SUBJECT_HEADER].header)); 
    fl_strcat(str,fl_static(" "));
    if (Header_post->k_header[SUBJECT_HEADER]) 
	fl_strcat(str,Header_post->k_header[SUBJECT_HEADER]);
    Aff_header(1,0,3,0,str,0);
    free(str);
    str=safe_malloc((14+(Header_post->k_header[NEWSGROUPS_HEADER] ? 
		fl_strlen(Header_post->k_header[NEWSGROUPS_HEADER]) : 0))
	    *sizeof(flrn_char));
    fl_strcpy(str,fl_static_tran(Headers[NEWSGROUPS_HEADER].header));
    fl_strcat(str,fl_static(" "));
    if (Header_post->k_header[NEWSGROUPS_HEADER]) 
	fl_strcat(str,Header_post->k_header[NEWSGROUPS_HEADER]);
    Aff_header(1,0,5,0,str,0);
    free(str);
    if ((Header_post->k_header[TO_HEADER]) ||
	    (Header_post->k_header[CC_HEADER]) ||
	    (Header_post->k_header[BCC_HEADER])) {
      if (par_mail) {
         for (i=TO_HEADER, row=7; i<X_CENSORSHIP; i++, row++) {
             str=safe_malloc((4+Headers[i].header_len+(Header_post->k_header[i]
			    ?  fl_strlen(Header_post->k_header[i]) : 0))
	      *sizeof(flrn_char));
             fl_strcpy(str,fl_static_tran(Headers[i].header));
             fl_strcat(str,fl_static(" "));
             if (Header_post->k_header[i]) 
	         fl_strcat(str,Header_post->k_header[i]);
             Aff_header(1,0,row,0,str,0);
             free(str);
	 }
      }
      Cursor_gotorc(11,0);
      put_string_utf8
           ((par_mail == 0 ? _("Choix actuel :  Poster uniquement.") :
	     (par_mail == 1 ? _("Choix actuel : Mail uniquement.") :
	      _("Choix actuel : Poster et envoyer par mail."))));
      Cursor_gotorc(13,0);
      put_string_utf8(_("      (0) post   (1) mail   (2) post/mail"));
    } else 
	par_mail=0;
}



/* Obtention du corps du post.					*/
/* La gestions des points doubles ne se fait pas.		*/
static int Screen_Start;

static int get_Body_post() {
   int incl_in_auto=-1,key;
   struct key_entry ke;

   Screen_Start=0;
   Deb_body=NULL;
   {
      int place, res;
     
      ke.entry=ENTRY_ERROR_KEY;
      place=res=0;
      while (res>=0) {
        res=Summon_Editor(incl_in_auto);
	if ((incl_in_auto==0) && (res==2)) res=0; 
	    		     /* C'est au moins la seconde édition, le */
	  		     /* message a deja ete modifié une fois...*/
	incl_in_auto=0;
        if (res==0) 
	  while (res==0) {
	    aff_info_messages();
  	    Cursor_gotorc(Screen_Rows2-1,0);
	    put_string_utf8(_("(P)oster, (E)diter, (A)nnuler ? "));
            Attend_touche(&ke);
	    if (KeyBoard_Quit) return 0;
	    if (ke.entry==ENTRY_SLANG_KEY) {
		key=ke.value.slang_key;
	        key=toupper(key);
	        if (key=='A') 
                   return 0;
	        if (key=='E') { res=1; continue; }
	        if (key=='P' || key=='Y' || key=='\r' ) {
                    return 1;
	        }
		if (key=='0') par_mail=0; else
		    if (key=='1') par_mail=1; else
			if (key=='2') par_mail=2;
	    }
	}
        else {
	  if (res>0) return -1; /* C'est un message non valide */
	  Cursor_gotorc(2,0);
	  put_string_utf8(_("(erreur dans l'Ã©dition, vÃ©rifier la variable d'environnement EDITOR)"));
	  Cursor_gotorc(3,0);
	  put_string_utf8(_("Pour poster, Ã©ditez le .article Ã  la main, puis utilisez \\<cmd> .article"));
	  return -1;
        }
      } 
   }
   return -1; /* unreachable */
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
         if (in_newsgroup && (groupe->grp_flags & GROUP_MODERATED_FLAG)) 
	     in_moderated_group=1;
         return nom2;
       }
       if (*copy_pre) {
         groupe=cherche_newsgroup(nom2,1,0);
         if (groupe!=NULL) {
            if (in_newsgroup && (groupe->grp_flags & GROUP_MODERATED_FLAG)) 
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
     /* FIXME : format */
     put_string_utf8(_("Groupe inconnu dans "));
     Screen_write_string(header);
     Screen_write_string(" : ");
     rc=conversion_to_terminal(nom2,&trad,0,(size_t)(-1));
     Screen_write_string(trad);
     if (rc==0) free(trad);
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_erase_eol();
     put_string_utf8(_("(L)aisser,(S)upprimer,(R)emplacer,(M)enu ? "));
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
		 int col2;
		 if (*copy_pre) strcpy(nom2,Options.prefixe_groupe);
		 fl_strcat(nom2,nom);
		 col2=put_string_utf8(_("Nom du groupe : "));
		 trad=safe_malloc(MAX_NEWSGROUP_LEN+1);
		 conversion_to_terminal(nom2,&trad,MAX_NEWSGROUP_LEN,
			 (size_t)(-1));
		 Screen_write_string(trad);
		 ret=flrn_getline(nom2,MAX_NEWSGROUP_LEN,
			 trad,MAX_NEWSGROUP_LEN,Screen_Rows2-1,col2);
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
	     col=put_string_utf8(Options.use_regexp ?
		     _("Regexp : ") : _("Sous-chaÃ®ne : "));
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
		 if (in_newsgroup && (groupe->grp_flags & GROUP_MODERATED_FLAG))
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
       put_string_utf8(_("Attention : crosspost sans suivi-Ã ."));
       Cursor_gotorc(Screen_Rows2-1,0);
       Screen_erase_eol();
       put_string_utf8(_("(L)aisser, (A)nnuler, (C)hoisir un suivi-Ã ?"));
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
	 int rc;
	 flrn_char *trad;
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
	 rc=conversion_from_utf8(_("Choix du groupe. <entrÃ©e> pour choisir, q pour quitter..."),&trad,0,(size_t)(-1));
	 flb1=(flrn_char *)Menu_simple(lemenu,lemenu,NULL,NULL,
		 trad);
	 if (rc==0) free(trad);
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
     /* FIXME : message coupé en 2 */
     put_string_utf8(_("Vous postez dans un groupe modÃ©rÃ©."));
     Cursor_gotorc(Screen_Rows2-2,0);
     put_string_utf8(_("Votre message n'apparaÃ®tra pas tout de suite."));
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_erase_eol();
     put_string_utf8(_("Appuyez sur une touche pour envoyer le message, ^C pou annuler."));
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
   struct post_body *lecture_courant;
   struct post_lecture *ecriture_courant;
   int deb_ligne=1, i;
   Header_List *liste;
   size_t elen;

   if (debug) fprintf(stderr, "Appel a format_article\n");
   ecriture_courant=Deb_article=safe_calloc(1,sizeof(struct post_lecture));

  /* Ecriture des headers */
  /* Attention : ici les headers sont plutôt buggués : il faudrait */
  /* formater, et remplacer les \n par des \r\n... Mais bon...     */
   i=Format_headers();
   if (i<0) return i;
   for (i=0; i<NB_DECODED_HEADERS; i++) 
      if (Header_post->k_header[i]) {
	  int crit;
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
	      conversion_to_utf8(Header_post->k_header[i],
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
	  conversion_to_utf8(liste->header_head,
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
   for (lecture_courant=Deb_body;lecture_courant;
	   lecture_courant=lecture_courant->suivant) {
	 flrn_char *buf;
	 int rc; char *trad;
	 size_t bla;
	 if (lecture_courant->ligne==NULL) continue;
	 buf=fl_strchr(lecture_courant->ligne,fl_static('\n'));
	 if (buf) 
	    bla=buf-lecture_courant->ligne;
	 else bla=fl_strlen(lecture_courant->ligne);
   	 rc=conversion_to_message(lecture_courant->ligne,
			&trad,0,bla);
	 elen=strlen(trad)+6;
	 ecriture_courant->ligne=safe_malloc(elen);
	 if ((deb_ligne) && (lecture_courant->ligne[0]=='.')) 
	     strcpy(ecriture_courant->ligne,".");
	 else ecriture_courant->ligne[0]='\0';
	 strcat(ecriture_courant->ligne,trad);
	 if (buf) {
	     strcat(ecriture_courant->ligne,"\r\n");
	     deb_ligne=1;
	 } else deb_ligne=0;
	 ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
	 ecriture_courant=ecriture_courant->suivant;
	 if (rc==0) free(trad);
   }

   if (deb_ligne==0) {
       ecriture_courant->ligne=safe_strdup("\r\n");
       ecriture_courant->suivant=safe_calloc(1,sizeof(struct post_lecture));
       ecriture_courant=ecriture_courant->suivant;
   }
   ecriture_courant->ligne=safe_strdup(".\r\n");
   if (debug) fprintf(stderr, "Fin de l'appel... \n");
   return 0;
}

static void Save_failed_initial_article() {
    FILE *tmp_file;
    char name[MAX_PATH_LEN];

    tmp_file=open_postfile(REJECT_POST_FILE,"w",name,0);
    if (tmp_file==NULL) return;
    Copie_prepost(tmp_file, 0);
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
#if (defined(USE_MKSTEMP) || defined(USE_TMPEXT))
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
   struct post_body *pb=Deb_body,*pb2;

   while (pc) {
       if (pc->ligne) free(pc->ligne);
       pc2=pc;
       pc=pc->suivant;
       free(pc2);
   }
   while (pb) {
       if (pb->ligne) free(pb->ligne);
       pb2=pb;
       pb=pb->suivant;
       free(pb2);
   }
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
   if (article->headers->k_raw_headers[REFERENCES_HEADER])
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
   int res, len1, len2=0, len3, i, from_perso=0;
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
	for (i=0;i<NB_KNOWN_HEADERS;i++) 
	  if (fl_strncasecmp(parcours->str,Headers[i].header,
	                          Headers[i].header_len)==0) break;
        if ((i<NB_DECODED_HEADERS) && (is_modifiable(i))) 
	  /* c'est a revoir... */
	{
	   Header_post->k_header[i]=safe_malloc(513*sizeof(flrn_char));
	   Copy_format (NULL, buf+2, article, Header_post->k_header[i], 512);
	   if (*(Header_post->k_header[i])==fl_static('\0')) {
	       free(Header_post->k_header[i]);
	       Header_post->k_header[i]=NULL;
	   } else {
	   /* on reformate */
	       buf2=Header_post->k_header[i];
	       while ((buf2=fl_strchr(buf2,fl_static('\n')))) 
	           *buf2=fl_static(' ');
	       if (i==FROM_HEADER) from_perso=1;
	       if (((i==TO_HEADER) || (i==CC_HEADER) || (i==BCC_HEADER)) &&
		   (par_mail==0)) par_mail=2;
	   }
	}
	else if (i==NB_KNOWN_HEADERS) {
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
	   parcours2->header_body=safe_calloc(513,sizeof(flrn_char));
	   Copy_format (NULL, buf+2, article, 
	                   parcours2->header_body, 512);
	   /* on reformate */
	   buf2=parcours2->header_body;
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
	    int ke;
	    ke = Ask_yes_no_utf8(_("RÃ©pondre par mail (O/N/A) ? "));
	    if (ke==-1) return -1;
	    if (ke==1) flag=1;
	    if (ke==0) break;
	 }
	  par_mail=flag; /* 0 ou 1 */
        } else if (fl_strcasecmp(Newsgroup_courant->name,
		    Pere_post->headers->k_headers[FOLLOWUP_TO_HEADER])!=0) { 
			/* On ne demande rien si on est dans le bon groupe */
	  int ke;
	  while (1) {
	    ke = Ask_yes_no_utf8(_("Suivre le followup (O/N/A) ? "));
	    if (ke==-1) return -1;
	    if (ke==1) {
		    Header_post->k_header[NEWSGROUPS_HEADER]=
			safe_flstrdup(Pere_post->headers->k_headers
				[FOLLOWUP_TO_HEADER]);
		    break;
	    }
	    if (ke==0) break;
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
      /* if (Pere_post->headers->k_headers[SUBJECT_HEADER]) { */
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
      /* } */
   }
   /* Newsgroups */
   if (Header_post->k_header[NEWSGROUPS_HEADER]==NULL) {
#ifdef GNKSA_NEWSGROUPS_HEADER
	 if  ((!Pere_post) ||
	         (!(Pere_post->headers->k_headers[NEWSGROUPS_HEADER])))
#endif
         {
	    if (Newsgroup_courant->grp_flags & GROUP_READONLY_FLAG) 
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

/* Cancel un message (fonction secondaire de ce fichier).		*/
/* Renvoie : >0 si le message est effectivement cancelé... 0 si le      */
/* cancel est annulé, et <0 si le cancel est refusé, ou bug...		*/
/* si confirm=1, alors on ne demande pas confirmation et on renvoit 2   */
/* sinon, on revonoie 2 si la reponse à la confirmation est 'T'         */
int cancel_message (Article_List *origine, int confirm) {
   int res,key;
   char line[100];

   if (origine->numero==-1) return -1;
   supersedes=0;
   Pere_post=NULL; /* Pas besoin de References */
   res=Est_proprietaire(origine,1);
   if (res!=1) return -1;
   /* On pourrait demander confirmation */
   /* L'équivalent d'un Aff_fin */
   if (confirm==0) {
     struct key_entry ke;
     ke.entry=ENTRY_ERROR_KEY;
     snprintf(line,99,_("Canceler le message %d (O/N/T) : "),origine->numero);
     Cursor_gotorc(Screen_Rows2-1,0);
     Screen_set_color(FIELD_ERROR);
     put_string_utf8(line);
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
   Deb_body=safe_calloc(1,sizeof(struct post_body));
   Deb_body->ligne=safe_flstrdup(
	   fl_static("This message is canceled by flrn.\n"));
   Change_message_conversion("utf-8");
   res=Format_article(origine->msgid);
   res=Post_article();
   Free_post_headers();
   Libere_listes();
   if (res<0) return -1;
   return 1+confirm;
}

static Header_List *Create_mime_headers(Header_List **prev)
{
    Header_List *mv, *cte, *ct;

    mv = safe_calloc(1, sizeof(Header_List));
    cte = safe_calloc(1, sizeof(Header_List));
    ct = safe_calloc(1, sizeof(Header_List));
    mv->next = cte;
    cte->next = ct;
    mv->header_head=safe_flstrdup(fl_static("MIME-Version:"));
    mv->header_body=safe_flstrdup(fl_static("1.0"));
    cte->header_head=safe_flstrdup(fl_static("Content-Transfer-Encoding:"));
    cte->header_body=safe_flstrdup(fl_static("8bit"));
    if(prev != NULL)
	*prev = mv;
    return(ct);
}

void Create_header_encoding () {
    struct post_body *lecture_courant;
    Header_List *liste;
    const char *result;
    /* 1ere chose, existe-t-il une entête ContentType */
    liste=Header_post->autres;
    if (liste==NULL) {
	liste = Create_mime_headers(&Header_post->autres);
    } else
    while (liste) {
	if (fl_strncasecmp(liste->header_head,
		    fl_static("content-type:"),13)==0) break;
	if (liste->next==NULL) {
	    liste = Create_mime_headers(&liste->next);
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
	    if (lecture_courant->ligne==NULL) {
		lecture_courant=lecture_courant->suivant;
		continue;
	    }
	    find_best_conversion(lecture_courant->ligne,
		    fl_strlen(lecture_courant->ligne),
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
	if (lecture_courant->ligne==NULL) {
	    lecture_courant=lecture_courant->suivant;
	    continue;
	}
	find_best_conversion(lecture_courant->ligne,
		fl_strlen(lecture_courant->ligne),
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
    find_best_conversion(NULL,0,NULL,NULL);
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
       Aff_error_fin_utf8(_("L'article est hors du newsgroup !"),1,-1);
       return -1;
    }
    if (flag==-1) {
      res=Est_proprietaire(origine,1);
      if (res!=1) {
	Aff_error_fin_utf8(_("Supersedes interdit !"),1,-1);
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
      put_string_utf8(_("Ce message n'a pas de sujet. Post annulÃ©."));
      Cursor_gotorc(Screen_Rows/2,3);
      /* FIXME : format */
      put_string_utf8(_("Message sauvé dans ~/"));
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
      /* FIXME : format */
      put_string_utf8(_("Post refusÃ©... L'article est sauvÃ© dans ~/"));
      Screen_write_string(REJECT_POST_FILE);
      Cursor_gotorc(Screen_Rows/2,3);
      put_string_utf8(_("Raison du refus : "));
      switch (res_post) {
	case -1 : put_string_utf8(_("erreur fatale en lecture/ecriture.")); 
		  break;
	case -2 : put_string_utf8(_("pas le droit de poster."));
		  break;
	case -3 : put_string_utf8(_("les newsgroups ne sont pas valides"));
		  break;
	case -4 : put_string_utf8(_("header From manquant."));
		  break;
	case -5 : put_string_utf8(_("header Newsgroups manquant ou  invalide."));
		  break;
	case -6 : put_string_utf8(_("pas de sujet."));
		  break;
	case -7 : put_string_utf8(_("message sans corps."));
		  break;
	case -8 : put_string_utf8(_("le message est vide."));
		  break;
	case -9 : put_string_utf8(_("une ligne est trop longue."));
		  break;
        case -10: put_string_utf8(_("Un header contient une adresse invalide."));
		  break;
        case -11: put_string_utf8(_("Post non autorisÃ©."));
		  break;
	case -12: put_string_utf8(_("problÃ¨me de date avec le serveur"));
		  break;
	default : put_string_utf8(_("inconnu : "));
		  break;
      }
      Cursor_gotorc(Screen_Rows/2+1,3);
      Screen_write_nstring(tcp_line_read,strlen(tcp_line_read)-2);
      Screen_set_color(FIELD_NORMAL);
    } else if (res_mail<0) {
	 /* FIXME :format */
	 Aff_error_utf8(_("Mail refusÃ©... L'article est sauvÃ© dans "));
         Screen_write_string(REJECT_POST_FILE);
    }
    Free_post_headers();
    Libere_listes();
    return ((res_post<0) || (res_mail<0) ? -1 : res_post+res_mail*2);
/* Bon, la fin est un peu rapide. Mais on verra bien. */
}
