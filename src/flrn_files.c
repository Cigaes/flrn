/* flrn v 0.1		                                                */
/*              flrn_files.c        22/06/98                            */
/*									*/
/* Des routines de gestion des fichiers (.flrn(rc?) et .flnewsrc)       */
/* Ce fichier doit se limiter à la gestion de fichiers.			*/
/*									*/

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <signal.h>

#include "flrn.h"
#include "group.h"
#include "art_group.h"
#include "options.h"

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
       if (Options.flnews_ext) strncat(name1,Options.flnews_ext,MAX_PATH_LEN-strlen(name1));
   }
   if (new_link)
       strncat(name2,new_link,MAX_PATH_LEN-strlen(name2));
   else {
       strncat(name2,DEFAULT_FLNEWS_FILE,MAX_PATH_LEN-strlen(name2));
       if (Options.flnews_ext) strncat(name2,Options.flnews_ext,MAX_PATH_LEN-strlen(name2));
   }

   name1[MAX_PATH_LEN-1]=name2[MAX_PATH_LEN-1]='\0';
   res=rename(name1, name2);
   if (res<0) fprintf(stderr, "Echec dans le renommage du .flnewsrc.new.\n");
}

/* Ouverture du fichier temporaire de post. On renvoie NULL si echec   */
/* note : on ne teste pas la longueur de file, qui ne doit JAMAIS etre */
/* defini par l'utilisateur 					       */
FILE *open_postfile (char *file,char *mode) {
   char name[MAX_PATH_LEN];
   char *home;
   FILE *tmppostfile;

   if (NULL == (home = getenv ("FLRNHOME")))
           home = getenv ("HOME");
   if (home==NULL) return NULL;
   strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_POST_FILE));
   strcat(name,"/"); if (file) strcat(name, file); else
      strcat(name,TMP_POST_FILE);
   tmppostfile=fopen(name,mode);
   return tmppostfile;
}

int stat_postfile(char *file,struct stat *st) {
   char name[MAX_PATH_LEN];
   char *home;
   if (NULL == (home = getenv ("FLRNHOME")))
           home = getenv ("HOME");
   if (home==NULL) return -1;
   strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_POST_FILE));
   strcat(name,"/"); if (file) strcat(name, file); else
      strcat(name,TMP_POST_FILE);
   return stat(name,st);
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
void Copy_article (FILE *dest, Article_List *article, int copie_head, char *avant) {
   int res, code;
   char *num, *buf;
   int flag=2; /* flag & 1 : debut de ligne     */
               /*      & 2 : fin de ligne       */

   num=safe_malloc(260*sizeof(char));
   if (article->numero>0) sprintf(num, "%d", article->numero); else
      strcpy(num,article->msgid);
   res=write_command(copie_head ? CMD_ARTICLE : CMD_BODY, 1, &num);
   free(num);
   if (res<0) return;
   code=return_code();
   if (code==423) { /* bad article number ? */
      num=safe_malloc(260*sizeof(char));
      strcpy(num,article->msgid);
      res=write_command(copie_head ? CMD_ARTICLE : CMD_BODY, 1, &num);
      free(num);
      if (res<0) return;
      code=return_code();
   }
   if ((code<0) || (code>300)) return;
   do {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      buf=tcp_line_read;
      if (res<1) return;
      if (res==1) { flag=2; fprintf(dest, "\n"); continue; }  /* On a lu '\n' */
      if (flag & 2) {
        if (strncmp(tcp_line_read, ".\r\n", 3)==0) return;
        if (*buf=='.') buf++;
	flag=1;
      } else flag=0;
      flag|=2*(tcp_line_read[res-2]=='\r');
      if ((flag & 1) && avant) {
	if ((*avant=='>') && Options.smart_quote && (*buf=='>'))
	  putc('>',dest); else
	  fprintf(dest, "%s", avant);
      }
      if (flag & 2) tcp_line_read[res-2]='\0';
      fprintf(dest, "%s", buf);
      if (flag & 2) fprintf(dest, "\n");
    } while (1);
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
  if (blah) {
    while (fgets(buf1, MAX_BUF_SIZE,blah)) {
      buf2=strchr(buf1,'\n');
      if (buf2) *buf2=0;
      if (*buf1) /* on vire les lignes vides... */
	add_to_liste(liste,buf1);
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
