/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_xover.c : obtention des Mesages-ID, References et autres
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include "flrn.h"
#include "art_group.h"
#include "group.h"
#include "options.h"
#include "flrn_tcp.h"
#include "flrn_format.h"
#include "flrn_xover.h"
#include "rfc2047.h"
#include "tty_display.h"

static UNUSED char rcsid[]="$Id$";

/* On définit ici la structure des XOVER qu'on va avoir */
#define OVERVIEW_SIZE_MAX 15
#define FULL_HEADER_OVER 256   /* doit être > NB_KNOWN_HEADERS */
/* -10 = bad, -1 = message-id */
static int overview_list[OVERVIEW_SIZE_MAX];
/* A un si l'on trouvé ce qu'on veut dans le xover */
int overview_usable=0;

/* Cette fonction teste si overview.fmt existe et remplit la structure
 * du xover si possible...
 * Résultat : 0 Ok (il faut Message-Id et References au moins
 *           -1 Bug ou xover inutilisable 
 *	     -2 authentification ?  */
int get_overview_fmt() {
   int res, code, num, i;
   char *buf;
   int good=0;

   for (num=0; num<OVERVIEW_SIZE_MAX; num++) overview_list[num]=-10;
   buf=safe_strdup("overview.fmt");
   res=write_command(CMD_LIST, 1, &buf);
   free(buf);
   if (res<0) return -1;

   code=return_code();
   if (code==480) return -2;
   if (code>=400) return -1;

   num=0; 
   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return -1;
   while (tcp_line_read[0]!='.') {
      if (num==OVERVIEW_SIZE_MAX) {   /* C'est trop... :-( */
         discard_server();
	 return -1;
      }
      for (i=0; i<NB_KNOWN_HEADERS; i++)
        if (strncasecmp(tcp_line_read, Headers[i].header,
		          Headers[i].header_len)==0) {
	   if (i>=NB_DECODED_HEADERS) good++; /* M-id ou references */
	   if (i==NB_DECODED_HEADERS+MESSAGE_ID_HEADER) {
	       overview_list[num]=-1;
	       if (strstr(tcp_line_read,"full"))
	 /* C'est moche mais on ne s'est pas donné la peine de gérer ce cas */
		   return -1;
	   } else {
	       overview_list[num]=i;
	       if (strstr(tcp_line_read,"full"))
		   overview_list[num]|=FULL_HEADER_OVER;
	       break;
	   }
	}
      num++;
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) return -1;
   }
   overview_usable=(good>=2);
   return (good>=2)?0:-1;
}

int check_in_xover(int number) {
   int num;
   if (overview_usable==0) return 0;
   for (num=0;num<OVERVIEW_SIZE_MAX;num++) {
      if (overview_list[num]==-10) break;
      if (overview_list[num]==-1) continue;
      if ((overview_list[num] & ~FULL_HEADER_OVER)==number) return 1;
   }
   return 0;
}


/* On obtient maintenant un résumé des articles n1-n2 du groupe par XOVER.  */
/* on crée les articles à chaque fois, en partant de article                */
/* si celui-ci est non NULL, et de Article_deb sinon...			    */
/* on va imiter ce que fait cree_liste de art_groupe, mais en mieux si      */
/* si possible								    */
/* On suppose qu'on est déjà dans le groupe courant (cette fonction peut    */
/* par exemple se faire appeler par un truc avant cree_liste...		    */
/* retour : >=0 si succès... -1 si échec				    */
int cree_liste_xover(int n1, int n2, Article_List **input_article, int *newmin,                        int *newmax) {
    int res, code, suite_ligne=0, num=0, crees=0;
    char *buf, *buf2;
    Article_List *creation, *article=input_article?*input_article:NULL;
    Range_List *msg_lus=Newsgroup_courant->read;
    int num_hdr=0,lus_index=0;
    int new_article=0; /* s'il y a du neuf, pour cree_liens */
    int out=0, progress=0, progress_step;
    char *hdr=NULL;

    /* FIXME : francais */
    Manage_progress_bar(fl_static("Lecture de l'overview, patientez..."),1);
    progress_step=(n2-n1)/10;
    if (progress_step==0) progress_step=1;
    *newmin=*newmax=-1;
    buf=safe_malloc(50*sizeof(char)); /* De toute façon, on devra */
    	/* gérer des lignes TRES longues cette fois :-( */
 /* On envoie la commande Xover */   
    sprintf(buf, "%d-%d", n1, n2);
    res=write_command(CMD_XOVER, 1, &buf);
    free(buf);
    if (res<0) return -1;
    code=return_code();
    if ((code<0) || (code>300)) return -1; 

    if (article==NULL) 
      Article_courant=Article_deb=NULL;
    res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
    if (res<0) return -1;
    while ((suite_ligne) || (tcp_line_read[0]!='.')) {
       if (!suite_ligne) {
         num=0;
	 buf=tcp_line_read;
	 buf2=strchr(buf,'\t');
	 if (!buf2) { /* il y a un bug dans l'overview, mais on continue 
			 en espérant que... */
              res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
	      if (res<0) return -1;
              continue; }
	 *buf2='\0';
         creation = safe_calloc (1,sizeof(Article_List));
	 creation->numero=strtol(buf,NULL,10);
	 while ((creation->numero-n1)/progress_step>progress) {
	      progress++;
	      Manage_progress_bar(NULL,0);
	 }
	 creation->flag=FLAG_NEW; /* pour le kill_file */
	 /* on le recherche */
	 if ((article) && (article->numero > creation->numero)) {
	   /* on insère avant article - sans danger le serveur répond
	    * dans l'ordre  et on suppose qu'on part du début de la liste*/
	   if (article->prev != NULL) { free(creation); return -1; }
	   article->prev=creation;
	   creation->next=article;
	   new_article=1;
	   article=creation;
	   *input_article=article;
	 } else {
	   while ((article) && (article->next) &&
	       (article->next->numero <= creation->numero))
	     article=article->next;
	   if (article && (article->numero == creation->numero)) {
	     /* on l'a déjà, celui-la */
	       new_article=0;
	       free_one_article(article,0); /* on detruit juste les headers */
	       free(creation);
	   } else {
	     if (article) {
	       if (article->next)
		 article->next->prev=creation;
	       creation->next=article->next;
	       article->next=creation;
	       creation->prev=article;
	     } else {
	       Article_deb=Article_courant=creation;
	     }
	     new_article=1;
	     article=creation;
	   }
	 }
	 if (new_article) {
	   if (creation->numero>*newmax) *newmax=creation->numero;
	   if ((creation->numero<*newmin) || 
	       (*newmin==-1)) *newmin=creation->numero;
	   crees++;
	 }
	 buf=buf2+1;
	 article->headers=new_header();
	 /* on regarde si le message est lu */
	 /* on recherche dans la table un max >= au numero de message */
	 if (new_article) {
	   while ((msg_lus) && (msg_lus->max[lus_index] < article->numero))
	   {
	     if(++lus_index>= RANGE_TABLE_SIZE) {
		lus_index=0;
		msg_lus = msg_lus->next;
	     }
	   }
	/* on verifie que le min est <= au numero */
	   if ((msg_lus) && (msg_lus->min[lus_index] <= creation->numero)) {
	     creation->flag |= FLAG_READ;
	   } else if (Newsgroup_courant->virtual_in_not_read>0) 
	       Newsgroup_courant->virtual_in_not_read--; else 
	       Newsgroup_courant->not_read++;
	 }
       } else buf=tcp_line_read;
       buf2=strchr(buf,'\t');
       if (buf2==NULL) buf2=strchr(buf,'\r');
       out=(*buf=='\n');
       /* en fait, y'a toujours au moins le message-id et references */
       while (!out) {
	  if (buf2) *buf2='\0';
	  if (*buf) {
	    if (new_article && (overview_list[num]==-1)) {
	      hdr=NULL;
	      /* article->msgid est == NULL ssi suite_ligne==0, non ? */
	      article->msgid=safe_strappend(article->msgid,buf);
	      article->headers->k_raw_headers[MESSAGE_ID_HEADER]=
		  article->msgid;
	      article->headers->
		  k_headers_checked[NB_DECODED_HEADERS+MESSAGE_ID_HEADER]=1;
	    } else
	    if (overview_list[num]>=0) {
	      num_hdr=overview_list[num] & ~FULL_HEADER_OVER;
	      if ((hdr==NULL) &&
		  (overview_list[num] & FULL_HEADER_OVER)) {
		buf=strchr(buf,':');
		if (buf) {
		  buf++;
		  if (*buf) buf++; /* on passe aussi le ' ' */
		}
              } 
	      if ((buf) && (buf !=buf2)) {
		hdr=safe_strappend(hdr,buf);
	      }
	      /* ca peut être trop tot, mais c'est pas grave */
	      article->headers->k_headers_checked[num_hdr]=1;
	    }
	  } else /* faut mettre a jour checked pour des headers vides */
	    if (overview_list[num]>=0) {
	      article->headers->k_headers_checked[
		overview_list[num] & ~FULL_HEADER_OVER ] = 1;
	    }
	  if (buf2) {
	    buf=buf2+1;
	    if ((overview_list[num]>=0) && (hdr)) {
		  if (num_hdr>=NB_DECODED_HEADERS) {
		      article->headers->k_raw_headers[num_hdr-
			  NB_DECODED_HEADERS]= hdr;
		  } else {
		      size_t ll = 3*strlen(hdr)+3;
		      article->headers->k_headers[num_hdr]=
			  safe_calloc(ll, sizeof(flrn_char));
		      ll=rfc2047_decode(article->headers->k_headers[num_hdr],
			      hdr, ll, 
			      (num_hdr<NB_UTF8_HEADERS ? 2 : 0));
		      article->headers->k_headers[num_hdr]=
			  safe_realloc(article->headers->k_headers[num_hdr],
				  (1+ll)*sizeof(flrn_char));
		  }
		  hdr=NULL;
	    }
	    if (*buf=='\n') /* finissons-en */ {
	       out=1; suite_ligne=0;
	    } else {
	      suite_ligne=1;
	      buf2=strchr(buf,'\t');
	      if (buf2==NULL) buf2=strchr(buf,'\r');
	      num++;
	    }
	  } else {
	    out=1;
	    if ((!buf) || (*buf != '\n')) {
	       /* buf==NULL : un champ plein a echoue, mais comme on n'a */
	       /* jamais rencontré le \n, on continue */
	      suite_ligne=1;
	    } else suite_ligne=0;
	  }
       }
       if (!suite_ligne) {
	 if (article->headers->k_headers[DATE_HEADER])
	    article->headers->date_gmt=
	      parse_date(article->headers->k_headers[DATE_HEADER]);
	 if ((article->msgid==NULL) || (strlen(article->msgid)==0)) { 
	          /* y'a eu une erreur grave */
            if (article->prev) article->prev->next=article->next; else
	         Article_deb=Article_courant=article->next;
	    if (article->next) article->next->prev=article->prev;
	    free_one_article(article,1);
	    article=Article_deb;
	    crees-=new_article;
	 }
       }
       res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
       if (res<0) return -1;
    }
    /* On veut leur faire la peau, à ces problèmes de synchronisation */
    if (crees>0) {
      if (n2>Newsgroup_courant->max)
	Newsgroup_courant->max=article->numero;
    }
    return crees;
}


int va_dans_groupe() {
   int res,rc;
   char *buf;

   if (Newsgroup_courant==NULL) return 0;
   /* On change le newsgroup courant pour le serveur */
   rc=conversion_to_utf8(Newsgroup_courant->name,&buf,0,(size_t)(-1));
   res=write_command(CMD_GROUP, 1, &buf);
   if (rc==0) free(buf);
   if (res<0) return -1;
   return 0;
}

/* Cette fonction cree la liste des articles du newsgroup courant
 * On suppose provisoirement que la commande XHDR fonctionne.
 * Le retour est -2 pour un newsgroup invalide. */
int cree_liste_noxover(int min, int max, Article_List *start_article, int *newmin, int *newmax) {
   int res, code, crees=0;
   char *buf, *buf2;
   Article_List *creation,*article=start_article;
   Range_List *msg_lus=Newsgroup_courant->read;
   int lus_index=0;
   int progress=0, progress_step;
   
   Manage_progress_bar(fl_static("Lecture des References, patientez..."),1);
   progress_step=((max-min)*2)/9;
   if (progress_step==0) progress_step=1;
   *newmin=min;
   *newmax=max;
   /* Envoie de la commande XHDR
    * 270 comme taille max d'un message-ID ? */
   buf=safe_malloc(270*sizeof(char));
   sprintf(buf,"Message-ID %d-%d", min, max);
   res=write_command(CMD_XHDR, 1, &buf);
   if (res<0) {free(buf);return -1;}
   code=return_code();
   if ((code<0) || (code>300)) {free(buf);return -1;}

   if (article==NULL) 
     Article_courant=Article_deb=NULL;
   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<2) {free(buf);return -1;}
   tcp_line_read[res-2]='\0'; /* La, on écrase le \r\n */
   while (tcp_line_read[0]!='.') {
      crees++;
      creation = safe_calloc (1,sizeof(Article_List));
      creation->prev = article;
      if (article) article->next=creation; 
	  	else Article_deb=Article_courant=creation;
      creation->msgid = safe_malloc ((res-2)*sizeof(char));
      if (sscanf(tcp_line_read, "%d %s", &(creation->numero), creation->msgid)<2) {
         crees--;
	 free_one_article(creation,1);
         res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
         if (res<0) {free(buf);return -1;}
         tcp_line_read[res-2]='\0';  /* Idem */
	 continue;
      }

      while ((creation->numero-min)/progress_step>progress) {
	      progress++;
	      Manage_progress_bar(NULL,0);
      }

      creation->flag = FLAG_NEW;
      /* on regarde si le message est lu */
      /* on recherche dans la table un max >= au numero de message */
      while ((msg_lus) && (msg_lus->max[lus_index] < creation->numero))
      {
         if(++lus_index>= RANGE_TABLE_SIZE) {
            lus_index=0;
            msg_lus = msg_lus->next;
        }
      }
      /* on verifie que le min est <= au numero */
      if ((msg_lus) && (msg_lus->min[lus_index] <= creation->numero)) {
         creation->flag |= FLAG_READ;
      } else if (Newsgroup_courant->virtual_in_not_read>0) 
	       Newsgroup_courant->virtual_in_not_read--; else 
	       Newsgroup_courant->not_read++;

      article=creation;
     
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) {free(buf);return -1;}
      tcp_line_read[res-2]='\0';  /* Idem */
   }
   if (article) { 
      article->next=NULL;
      Newsgroup_courant->max=article->numero; /* Au cas ou ca aurait */
      						      /* quand même buggué...*/
   }

   Manage_progress_bar(NULL,0);
   progress=0;
   /* Recherche des parents */
   sprintf(buf, "References %d-%d", min, max);
   res=write_command(CMD_XHDR, 1, &buf);
   if (res<0) {free(buf);return -1;}
   code=return_code();
   if ((code<0) || (code>300)) {free(buf);return -1;}

   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) {free(buf); return -1;}
   for (article=Article_deb; article; article=article->next) {
      if (tcp_line_read[0]=='.') break;  /* Impossible */
      /* il faut faire ce genre de check, non ? */
      if ((code=strtol(tcp_line_read,&buf2,10)) != article->numero)
      {
	if (debug) fprintf(stderr,"xhdr References se passe mal\n");
	while (article && (article->numero<code))
	{ article = article->next; }
    /* Si article=NULL, on ignore ce qu'il y a encore a lire */
	if (!article) { discard_server(); break; }
      }
      while ((article->numero-min)/progress_step>progress) {
	      progress++;
	      Manage_progress_bar(NULL,0);
      }
      if (code == article->numero) {
	/* on construit le champ References */
	article->headers=new_header();
	article->headers->k_raw_headers[MESSAGE_ID_HEADER]=article->msgid;
	article->headers->k_headers_checked
	    [MESSAGE_ID_HEADER+NB_DECODED_HEADERS]=1;
	article->headers->k_headers_checked
	    [REFERENCES_HEADER+NB_DECODED_HEADERS] = 1;
	if ((buf2 = strchr(buf2,'<')))
	  article->headers->k_raw_headers[REFERENCES_HEADER] =
	      safe_strdup(buf2);
      }
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) {free(buf);return -1;}
   }   
   free(buf);
   return crees;
}

int cree_liste(int art_num, int *part) {
   int min,max,res,code,newmin,newmax;
   char *buf;
   /* On change le newsgroup courant pour le serveur */
   *part=0;
   if (va_dans_groupe()<0) return -1;

   /* on n'utilise pas return_code car on veut le max du newsgroup */
   res=read_server_with_reconnect(tcp_line_read, 3, MAX_READ_SIZE-1);
   if (res<3) {
      if (debug) fprintf(stderr, "Cree_liste: Echec en lecture du serveur\n");
      return -1;
   }
   /* On ne suppose pas que la ligne est trop longue : c'est quasi-exclu */
   if (tcp_line_read[res-1]!='\n') {
     if (debug) fprintf(stderr, "Serveur grotesque : ligne trop longue\n");
     return -1;
   }
   code=strtol(tcp_line_read, &buf, 10);

   if (code<=0) return -1;
   if (code==411) { if (debug) fprintf(stderr, "Newsgroup invalide !\n"); 
       return -2; }
   strtol(buf, &buf, 10);
   min=strtol(buf, &buf, 10);
   max=strtol(buf, &buf, 10);
   if (Options.max_group_size>=0) {
      Newsgroup_courant->min=
             (art_num>max ? max : art_num)-Options.max_group_size;
      if (Newsgroup_courant->min<min) Newsgroup_courant->min=min; else
                                      min=Newsgroup_courant->min;
   } else Newsgroup_courant->min=min;

   if (Newsgroup_courant->Article_deb) {
     Article_deb=Newsgroup_courant->Article_deb;
     Article_exte_deb=Newsgroup_courant->Article_exte_deb;
     Hash_table=Newsgroup_courant->Hash_table;
     Thread_deb=Newsgroup_courant->Thread_deb;
     if (Newsgroup_courant->max<max) /* Ca peut etre > si list active et */
     {
     /* on essaie un cree_liste_xover moins couteux qu'un cherche newnews */
       if (cree_liste_xover(Newsgroup_courant->max+1,max,&Article_deb,&newmin,&newmax)>0) {
	 Date_groupe = time(NULL)+Date_offset;
	 cree_liens();
	 apply_kill_file(newmin,newmax);
       }
       else
	 cherche_newnews();
     }
     return 1;
   }
   /* si on a deja ce qu'on veut, on n'en fait pas trop */
   Newsgroup_courant->not_read=0;
   Newsgroup_courant->virtual_in_not_read=0;
   Newsgroup_courant->important=0;
   if ((max<min)||(max==0)) 
     return 0;
   if (Newsgroup_courant->max<=max) /* Ca peut etre > si list active et */
                                   /* group sont contradictoires....    */
       Newsgroup_courant->max=max;
   else { /* Des messages ne sont pas disponibles */
     	  /* On va supposer tout bêtement qu'il y a un cancel */
      if (debug) fprintf(stderr, "Problème de max de Newsgroup_courant\n");
#if 0
      Aff_error("Patientez svp...");
      sleep(5); /* En esperant que ca suffise */
#endif
   }

   if (art_num==0) art_num=max-100;
   /* fixme */
   art_num -= 20; /* on veut essayer d'avoir le père et les frères */
   if ((art_num>min) && (art_num<=max) && (overview_usable)) {
     min=art_num;
     *part=1;
   }
   Date_groupe = time(NULL)+Date_offset;
   res=0;
   while (1) {
     if (overview_usable)
       res=cree_liste_xover(min,max,NULL,&newmin,&newmax);
     else
       res=cree_liste_noxover(min,max,NULL,&newmin,&newmax);
     if ((res!=0) || (*part==0)) break;
     /* on a trouvé aucun groupe , mais on n'a pas tout fait , alors
      * on recommence jusqu'à... */
     art_num -= 100;
     if (art_num<=Newsgroup_courant->min) {
	   art_num=Newsgroup_courant->min;
           *part=0;
     }
     min=art_num;
   } 

   if (res<=0) return res;
   Article_exte_deb=NULL;
   Hash_table=NULL;
   Thread_deb=NULL;
   cree_liens();
   /* on memorise tout */
   /* attention, Article_deb et Article_exte_deb pourraient changer !!! */
   /* donc on ne les utilise pas, et on les remplacera a la fin... */
   Newsgroup_courant->article_deb_key=Article_deb_key;
   /* on a besoin de article_deb_key pour le kill_file... */
   apply_kill_file(newmin,newmax);
   Newsgroup_courant->Article_deb=Article_deb;
   Newsgroup_courant->Article_exte_deb=Article_exte_deb;
   Newsgroup_courant->Hash_table=Hash_table;
   Newsgroup_courant->Thread_deb=Thread_deb;
   return res;
}

int cree_liste_end() {
  int res,newmin,newmax;
  /* fixme */
  if (!overview_usable) return 0;
  /* ca ne veut pas dire gd chose comme test, mais bon... */
  if (Article_deb->numero <2) return 0;
  res=cree_liste_xover(Newsgroup_courant->min,Article_deb->numero-1,&Article_deb,&newmin,&newmax);
  if (res>0) {
    cree_liens();
    apply_kill_file(newmin,newmax);
  }
  return 0;
}

/* On retourne 0 si tout est créé, 1 sinon */
int cree_liste_suite(int end) {
  int res, num,newmin,newmax;
  if (end) return cree_liste_end();
  if (Article_deb->numero-301<Newsgroup_courant->min) return cree_liste_end();
  if (!overview_usable) return 0;
  if (Article_deb->numero-Newsgroup_courant->min>800) num=(Article_deb->numero-Newsgroup_courant->min)/4; else num=200;
  res=cree_liste_xover(Article_deb->numero-num,Article_deb->numero-1,&Article_deb,&newmin,&newmax);
  if (res>0) {
    cree_liens();
    apply_kill_file(newmin,newmax);
  }
  return 1;
}

int launch_xhdr(int min, int max, char *header_name) {
   int res,code;
   char *buf, *buf2;

   buf=safe_malloc((30+strlen(header_name))*sizeof(char));
   sprintf(buf,"%s %d-%d", header_name, min, max);
   buf2=strchr(buf,':');
   if (buf2) *buf2=' ';
   res=write_command(CMD_XHDR, 1, &buf);
   free(buf);
   if (res<0) return -1; 
   code=return_code();
   if ((code<0) || (code>300)) return -1;
   return 0;
}

/* return the number of the article obtained */
/* -1 :erreur   -2 : the end */
int get_xhdr_line(int num, flrn_char **flligne, int *tofree,
	int num_hdr, Article_List *art) {
   int res, number,rc;
   Article_List *artbis=art;
   char *ligne;
   *tofree=0;
   ligne=NULL;
   while (1) {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<2) return -1;
      if (res<4) return -2;
      tcp_line_read[res-2]='\0';
      number=strtol(tcp_line_read,&ligne,10);
      if (ligne) while (*ligne==' ') ligne++;
      if (ligne && (strcmp(ligne,"(none)")==0)) ligne="";
      if (number<num) continue;
      if (num_hdr!=-1) 
          while ((artbis) && (artbis->numero>number)) artbis=artbis->next;
      if ((artbis) && (number==artbis->numero) && (num_hdr!=-1)) {
         if (artbis->headers==NULL) {
	   artbis->headers=new_header();
	   artbis->headers->k_raw_headers[MESSAGE_ID_HEADER]=artbis->msgid;
	   artbis->headers->k_headers_checked
	       [MESSAGE_ID_HEADER+NB_DECODED_HEADERS]=1;
	 }
	 if (artbis->headers->k_headers_checked[num_hdr]==1) {
	     if (num_hdr<NB_DECODED_HEADERS) {
	         *flligne=artbis->headers->k_headers[num_hdr];
	     } else {
		 rc=conversion_from_utf8(
			 artbis->headers->k_raw_headers
			                  [num_hdr-NB_DECODED_HEADERS],
			 flligne, 0,(size_t)(-1));
		 *tofree=(rc==0);
	     }
	     return number;
	 } else {
	    artbis->headers->k_headers_checked[num_hdr]=1;
	    if (num_hdr<NB_DECODED_HEADERS) {
		size_t ll=3*strlen(ligne)+3;
		artbis->headers->k_headers[num_hdr]=
		    safe_calloc(ll,sizeof(flrn_char));
		ll=rfc2047_decode(artbis->headers->k_headers[num_hdr],
			ligne, ll, (num_hdr<NB_UTF8_HEADERS) ? 2 : 0);
		artbis->headers->k_headers[num_hdr]=safe_realloc(
			artbis->headers->k_headers[num_hdr],
			(1+ll)*sizeof(flrn_char));
	        if (num_hdr==DATE_HEADER) {
	           artbis->headers->date_gmt=
		      parse_date(artbis->headers->k_headers[DATE_HEADER]);
	        }
		*flligne=artbis->headers->k_headers[num_hdr];
	    } else {
		artbis->headers->k_raw_headers
		    [num_hdr-NB_DECODED_HEADERS]=safe_strdup(ligne);
		rc=conversion_from_utf8(
			ligne,flligne,0,(size_t)(-1));
		*tofree=(rc==0);
	    }
	    return number;
	 }
      } 
      if (number>=num) {
	  size_t ll=3*strlen(ligne)+3;
	  *flligne=safe_calloc(ll,sizeof(flrn_char));
	  ll=rfc2047_decode(*flligne,ligne,ll,0);
	  *tofree=1;
	  return number;
      }
   }
   /* unreachable */
}

int end_xhdr_line() {
   return discard_server();
}
