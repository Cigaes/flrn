/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_files.c : gestion des fichiers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/types.h>
#include <signal.h>

#include "flrn.h"
#include "options.h"
#include "flrn_tcp.h"
#include "flrn_files.h"
#include "flrn_filter.h"
#include "rfc2047.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* La socket avec le serveur  pour la fermer apres un fork */
extern int tcp_fd;

int with_direc;

/* Ouverture du fichier de config. Renvoie un FILE dessus. Dans le cas  */
/* ou ce fichier n'existe pas, le crée et copie dedans un config	*/
/* minimale (à condition de trouver le home directory).			*/
/* file, si non NULL, donne le nom du fichier à partir de HOME		*/ 
/* mode a r pour le lire, w+ pour le creer */
/* flag =1, créer un fichier vide, =2 renvoyer le fichier par défaut de config
	(lecture seule)
 * flag=0 ne pas créer de fichier */

FILE *open_flrnfile (char *file,char *mode, int flag, time_t * date)
{
   char *home, name[MAX_PATH_LEN];
   int home_found = 1;
   FILE *config_file;
   struct stat buf;

   if ((file==NULL) || (*file!='/')) {
     if (NULL == (home = getenv ("FLRNHOME"))) {
        home = getenv ("HOME");
	/* On recherche un éventuel répertoire .flrn */
	if (home && (with_direc==-1)) {
	   strncpy(name,home,MAX_PATH_LEN-1);
	   name[MAX_PATH_LEN-2]='\0';
           strcat(name, "/");
	   strncat(name,DEFAULT_DIR_FILE,MAX_PATH_LEN-strlen(name)-1);
	   name[MAX_PATH_LEN-1]='\0';
	   if ((stat(name,&buf)>=0) && ((buf.st_mode & S_IFMT)==S_IFDIR)) with_direc=1;
	}
     }
     if (with_direc==-1) with_direc=0;
     if (home == NULL)
     {
        strcpy(name,"./");
        home_found = 0;
     }
     else
     {
        strncpy(name, home, MAX_PATH_LEN-1); 
        name[MAX_PATH_LEN-2]='\0';   /* oui, je sais, precaution ridicule */
        strcat(name, "/");
	if (with_direc) {
	   strncat(name,DEFAULT_DIR_FILE,MAX_PATH_LEN-strlen(name)-2);
	   name[MAX_PATH_LEN-2]='\0';
           strcat(name, "/");
	}
     }
   } else {
     home_found=0;
     name[0]='\0';
   }
   if (file)
       strncat(name,file,MAX_PATH_LEN-strlen(name)-1);
   else
       strncat(name,DEFAULT_CONFIG_FILE,MAX_PATH_LEN-strlen(name)-1);

   name[MAX_PATH_LEN-1]='\0';	/* Décidément, j'insiste */

   if (debug) fprintf(stderr,"On essaie d'ouvrir %s\n",name);
   if (date) {
     if (stat(name, &buf)<0)  *date=0;
      else *date=buf.st_mtime+Date_offset;
   }

   if (!(config_file=fopen(name,mode))) {
/*
     if (flag) {
       fprintf(stderr,"Le fichier %s ne peut être ouvert!\n",name);
       fprintf(stderr,"Utilisation du fichier par défaut.\n");
       fprintf(stderr,"Si vous avez un .flrn, vous devez le renommer en %s\n",
       		DEFAULT_CONFIG_FILE);
     }
*/
     if (home_found && (flag==1)) {
       if (debug) fprintf(stderr,"On va essayer de le créer\n");
       config_file=fopen(name,"w+");
       if (config_file == NULL) {
	  perror("fopen");
	  sleep(1);
	  return NULL;
       }
       fseek(config_file,0L,SEEK_CUR); /* Pour l'effet de synchronisation */
     } else if (flag==2) {
	 if (debug) fprintf(stderr,"Ouverture du fichier par défaut\n");
	 strncpy(name,PATH_CONFIG_DIRECTORY,MAX_PATH_LEN-1);
	 name[MAX_PATH_LEN-2]='\0';
	 strncat(name,DEFAULT_CONFIG_SYS_FILE,MAX_PATH_LEN-strlen(name)-1);
	 name[MAX_PATH_LEN-1]='\0';
	 config_file=fopen(name,"r");
	 if (config_file == NULL) {
	   if (debug) fprintf(stderr,"Fichier de config inexistant ??\n");
 	   return NULL;
	 }
     } else return NULL;
   }
   return config_file;
}

/* Renommage du .flnewsrc.new lors de la sauvegarde */
void rename_flnewsfile (char *old_link,char *new_link) 
{
   char *home, name1[MAX_PATH_LEN], name2[MAX_PATH_LEN];
   int home_found = 1, res;
   struct stat buf;

   if (NULL == (home = getenv ("FLRNHOME")))
   {
      home = getenv ("HOME");
      /* On recherche un éventuel répertoire .flrn */
      if (home && (with_direc==-1)) {
	   strncpy(name1,home,MAX_PATH_LEN-1);
	   name1[MAX_PATH_LEN-2]='\0';
           strcat(name1, "/");
	   strncat(name1,DEFAULT_DIR_FILE,MAX_PATH_LEN-strlen(name1)-1);
	   name1[MAX_PATH_LEN-1]='\0';
	   if ((stat(name1,&buf)>=0) && (buf.st_mode & S_IFDIR)) with_direc=1;
      }
   }
   if (with_direc==-1) with_direc=0;

   if (home == NULL)
   {
      if (debug) fprintf(stderr,"HOME semble non défini... On essaie un chemin local\n");
      strcpy(name1,"./");
      home_found = 0;
   }
   else
   {
      strncpy(name1, home, MAX_PATH_LEN-1);
      name1[MAX_PATH_LEN-2]='\0';   /* oui, je sais, precaution ridicule */
      strcat(name1, "/");
      if (with_direc)
      {
          strncat(name1,DEFAULT_DIR_FILE,MAX_PATH_LEN-strlen(name1)-2);
          name1[MAX_PATH_LEN-2]='\0';
	  strcat(name1, "/");
      }
   }
   strcpy(name2, name1);
   if (old_link)
       strncat(name1,old_link,MAX_PATH_LEN-strlen(name1));
   else {
       strncat(name1,DEFAULT_FLNEWS_FILE,MAX_PATH_LEN-strlen(name1));
       if (Options.flnews_ext) {
	   int rc;
	   char *trad;
	   rc=conversion_to_file(Options.flnews_ext,&trad,0,(size_t)(-1));
	   strncat(name1,trad,MAX_PATH_LEN-strlen(name1));
	   if (rc==0) free(trad);
       }
   }
   if (new_link)
       strncat(name2,new_link,MAX_PATH_LEN-strlen(name2));
   else {
       strncat(name2,DEFAULT_FLNEWS_FILE,MAX_PATH_LEN-strlen(name2));
       if (Options.flnews_ext) {
	   int rc;
	   char *trad;
	   rc=conversion_to_file(Options.flnews_ext,&trad,0,(size_t)(-1));
	   strncat(name2,trad,MAX_PATH_LEN-strlen(name2));
	   if (rc==0) free(trad);
       }
   }

   name1[MAX_PATH_LEN-1]=name2[MAX_PATH_LEN-1]='\0';
   res=rename(name1, name2);
   if (res<0) fprintf(stderr, "Echec dans le renommage du .flnewsrc.new.\n");
}

/* Ouverture du fichier temporaire de post. On renvoie NULL si echec   */
/* note : on ne teste pas la longueur de file, qui ne doit JAMAIS etre */
/* defini par l'utilisateur 					       */
/* resname renvoie le nom total du fichier. il doit être un tableau de */
/* caractères de taille MAX_PATH_LEN.                                  */
FILE *open_postfile (char *file,char *mode,char *name, int tmp) {
   char *home;
   FILE *tmppostfile;
   int fdfile;

   if (NULL == (home = getenv ("FLRNHOME")))
           home = getenv ("HOME");
   if (home==NULL) return NULL;
#ifdef USE_MKSTEMP
   if (tmp)
      strncpy(name,home,MAX_PATH_LEN-10-strlen(TMP_POST_FILE));
   else
#endif
      strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_POST_FILE));
   strcat(name,"/"); if (file) strcat(name, file); else
      strcat(name,TMP_POST_FILE);
#ifdef USE_MKSTEMP
   if (tmp) {
     strcat(name,".XXXXXX");
     fdfile=mkstemp(name);
     if (fdfile==-1) return NULL;
     tmppostfile=fdopen(fdfile,mode);
   } else
#endif
   tmppostfile=fopen(name,mode);
   return tmppostfile;
}

/* teste si un fichier type mailbox a du nouveau */
/* (reprise du code de forum) */
int newmail(char *name) {
   static struct stat s;

   if (stat(name, &s)==0) 
      return ((s.st_size>0) && (s.st_atime<s.st_mtime));
   return 0;
}

/* Ouverture d'un fichier d'aide en mode read_only. Pas a se poser de  */
/* questions.							       */
FILE *open_flhelpfile (char ext)
{
   char name[MAX_PATH_LEN];

   sprintf(name,"%s%s%c", DEFAULT_HELP_DIRECTORY, DEFAULT_HELP_FILES, ext);
   return fopen(name,"r");
}


/* Copie le contenu d'un article dans un fichier */
/* encoding = 0 -> on envoie l'article directement */
/* encoding = 1 -> on traduit le fichier vers le charset de files */
/* encoding = 2 -> on traduit le fichier vers le charset de l'éditeur */
/* encoding = 3 ou 4 -> idem, mais on traduit les entêtes via la rfc2047 */
void Copy_article (FILE *dest, Article_List *article, int copie_head,
	char *avant, int with_sig, int encoding) {
   int res, code, resconv=1;
   char *newline;
   flrn_char *newheader;
   size_t newheader_len=0, newline_len=0;
   char *num, *buf, *trad=NULL;
   int in_headers = (copie_head==1) || (encoding>0), quit_headers=0;
   int flag=2; /* flag & 1 : debut de ligne     */
               /*      & 2 : fin de ligne       */
/* affichage en quoted-printable */
#ifdef USE_CONTENT_ENCODING
   int isinQP=-1;
#endif

   num=safe_malloc(260*sizeof(char));
   if (article->numero>0) sprintf(num, "%d", article->numero); else
      if (article->msgid) strcpy(num,article->msgid); else
      return;
   res=write_command(in_headers ? CMD_ARTICLE : CMD_BODY, 1, &num);
   free(num);
   if (res<0) return;
   code=return_code();
   if (code==423) { /* bad article number ? */
      num=safe_malloc(260*sizeof(char));
      strcpy(num,article->msgid);
      res=write_command(in_headers ? CMD_ARTICLE : CMD_BODY, 1, &num);
      free(num);
      if (res<0) return;
      code=return_code();
   }
   if (encoding) parse_ContentType_header(NULL);
   if ((code<0) || (code>300)) return;
   newline=NULL; newheader=NULL;
   do {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      buf=tcp_line_read;
      if (res<1) break;
      if (flag & 2) {
        if (strncmp(tcp_line_read, ".\r\n", 3)==0) break;
        if (*buf=='.') buf++;
	flag=1;
      } else flag=0;
      if (tcp_line_read[res-2]=='\r') {
	  tcp_line_read[res-2]='\n';
	  tcp_line_read[--res]='\0';
      } else if (tcp_line_read[res-1]=='\r') tcp_line_read[--res]='\0';
                      /* cas limite de ligne longue, on lira '\n' après */
      if ((in_headers) && (encoding)) {
	  if (newline==NULL) {
	      newline=safe_strdup(buf);
	      newline_len=strlen(buf)+1;
	  } else {
	      if ((flag==1) && ((!isblank(tcp_line_read[0])) || 
		     (tcp_line_read[0]=='\n'))) {
		  /* on reprend newline */
		  if (newheader==NULL) {
		      newheader_len = 3*strlen(newline)+3;
		      newheader=safe_malloc(newheader_len*sizeof(flrn_char));
		  } else {
		      size_t newsize=3*strlen(newline)+3;
		      if (newsize>newheader_len) {
			  newheader=safe_realloc(newheader,
				  newsize*sizeof(flrn_char));
			  newheader_len=newsize;
		      }
		  }
		  {  int raw;
		     int i;
		     for (i=0;i<NB_UTF8_HEADERS;i++) {
			 if (strncasecmp(newline,Headers[i].header,
				     Headers[i].header_len)==0) break;
		     }
		     if (i==NB_UTF8_HEADERS) raw=(encoding<3);
		       else raw=2;
		     newheader_len=
		        rfc2047_decode(newheader,newline,newheader_len,
				  raw);
		  }
		  if (fl_strncasecmp(newheader,
			      fl_static("content-type:"),13)==0) {
		      parse_ContentType_header(newheader+13);
		  } else
#ifdef USE_CONTENT_ENCODING
                  if (fl_strncasecmp(newheader,
		             fl_static("content-transfer-encoding:"),26)==0) {
                      isinQP =  (fl_strstr(newheader+26,"uoted")!=NULL);
                  }
#endif
		  if (encoding%2==1) 
		     resconv=conversion_to_file(newheader,&trad,0,(size_t)(-1));
		  else
		     resconv=conversion_to_editor(newheader,&trad,0,(size_t)(-1));
		  if ((flag==1) && (tcp_line_read[0]=='\n')) quit_headers=1;
		  if (strlen(buf)>=newline_len) {
		      free(newline);
		      newline=safe_strdup(buf);
		      newline_len=strlen(buf)+1;
		  } else strcpy(newline,buf);
	      } else {
		  if (strlen(buf)+strlen(newline)>=newline_len) {
		      newline=safe_realloc(newline,strlen(buf)+
			      strlen(newline)+1);
		      newline_len=strlen(buf)+strlen(newline)+1;
		  }
		  strcat(newline,buf);
	      }
	 }
      } else {
	 if (encoding) {
	     int resconv2;
	     flrn_char *trad2;
#ifdef USE_CONTENT_ENCODING
	     if ((isinQP==1) && (tcp_line_read[res-1]=='\n') &&
		     (tcp_line_read[res-2]=='=')) {
		 /* cas du soft line break */
		 res-=2;
		 tcp_line_read[res]='\0';
             }
#endif
	     resconv2=conversion_from_message(buf,&trad2,0,(size_t)(-1));
	     if (encoding%2==1)
	       resconv=conversion_to_file(trad2,&trad,0,(size_t)(-1));
	     else
	       resconv=conversion_to_editor(trad2,&trad,0,(size_t)(-1));
	     if (resconv2==0) { /* il faut libérer trad2 */
		 if (resconv==0) free(trad2); /* aucun pb si trad!=trad2 */
		 else resconv=0; /* sinon on dit juset qu'il faudra *
				  * libérer trad */
	     }
	 } else {
	     trad=buf;
	     resconv=1;
	 }
      }
      if (trad) {
        if (flag==1) {
	   if ((!with_sig) && (strncmp(trad, "-- \n", 4)==0)) {
	      discard_server();
	      break;
	   }
	   if (((copie_head) || (!in_headers)) && (avant)) {
	       if ((*avant=='>') && Options.smart_quote && (*trad=='>'))
		   putc('>',dest); else
		   fputs(avant, dest);
	   }
        }
	if ((copie_head) || (!in_headers)) fputs(trad,dest);
	if (resconv==0) free(trad);
	trad=NULL;
      }
      if (tcp_line_read[res-1]=='\n') flag=2;
      if (quit_headers==1) {
	  in_headers=0;
	  quit_headers=0;
	  if (copie_head) {
	      if (avant) fputs(avant,dest);
	      putc('\n',dest);
	  }
#ifdef USE_CONTENT_ENCODING
          if (isinQP==1) change_QP_mode(1);
#endif
      }
   } while (1);
#ifdef USE_CONTENT_ENCODING
   change_QP_mode(0);
#endif
   if ((trad) && (resconv==0)) free(trad);
   if (newheader) free(newheader);
   if (newline) free(newline);
}

/* TODO à écrire correctement */
int init_kill_file() {
  FILE *blah = open_flrnfile(Options.kill_file_name,"r",0,NULL);
  if (blah) {
    if (!parse_kill_file(blah)){
      fprintf(stderr,"Kill-file invalide, ignoré !\n");
      sleep(1);
    }
    fclose(blah);
  }
  return 0;
}

int read_list_file(char *name, Flrn_liste *liste) {
  FILE *blah = open_flrnfile(name,"r",0,NULL);
  char buf1[MAX_BUF_SIZE];
  char *buf2;
  flrn_char *trad;
  int rc;
  if (blah) {
    while (fgets(buf1, MAX_BUF_SIZE,blah)) {
      buf2=strchr(buf1,'\n');
      if (buf2) *buf2=0;
      if (*buf1) {/* on vire les lignes vides... */
	rc=conversion_from_utf8(buf1,&trad,0,(size_t)(-1));
	add_to_liste(liste,trad);
	if (rc==0) free(trad);
      }
    }
  } else {
    return -1;
  }
  return 0;
}

int write_list_file(char *name, Flrn_liste *liste) {
  char buf1[MAX_PATH_LEN];
  FILE *blah;
  int res;
  strcpy(buf1,name);
  strcat(buf1,".swp");
  blah = open_flrnfile(buf1,"w",0,NULL);
  if (blah) {
    res=write_liste(liste, blah);
    if (res==0) res=(fclose(blah)==EOF ? -1 : 0);
    if (res==0) rename_flnewsfile(buf1,name); 
  } else res=-1;
  if (res==-1) fprintf(stderr,"Erreur dans l'écriture de la liste de censure.\n");
  return res;
}
