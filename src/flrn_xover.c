/* flrn v 0.1                                                           */
/*              flrn_xover.c          02/02/98                          */
/*                                                                      */
/*  Gestion de la commade XOVER                                         */
/*  Ceci est destiné à devenir le standard pour flrn.                   */
/*                                                                      */

#include <strings.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include "flrn.h"
#include "group.h"
#include "art_group.h"

/* On définit ici la structure des XOVER qu'on va avoir */
#define OVERVIEW_SIZE_MAX 15
#define FULL_HEADER_OVER 256   /* doit être > NB_KNOWN_HEADERS */
/* -10 = bad, -1 = message-id */
static int overview_list[OVERVIEW_SIZE_MAX];
int overview_usable=0;

/* Cette fonction teste si overview.fmt existe et remplit la structure
 * du xover si possible...
 * Résultat : 0 Ok
 *           -1 Bug ou xover inutilisable */
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
   if (code>=400) return -1;

   num=0; 
   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return -1;
   while (tcp_line_read[0]!='.') {
      if (num==OVERVIEW_SIZE_MAX) {   /* C'est trop... :-( */
         discard_server();
	 return -1;
      }
      if (strncasecmp(tcp_line_read, "Message-Id:",11)==0)
      {
	   overview_list[num]=-1;
	   if (strstr(tcp_line_read,"full"))
	     return -1;
	   good++;
      } else
      for (i=0; i<NB_KNOWN_HEADERS; i++)
        if (strncasecmp(tcp_line_read, Headers[i].header,
		          Headers[i].header_len)==0) {
	   overview_list[num]=i;
	   if (strstr(tcp_line_read,"full"))
	     overview_list[num]|=FULL_HEADER_OVER;
	   if (i==REFERENCES_HEADER) good++;
	   break;
	}
      num++;
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) return -1;
   }
   overview_usable=(good>=2);
   return (good>=2)?0:-1;
}

/* On obtient maintenant un résumé des articles n1-n2 du groupe par XOVER.  */
/* on crée les articles à chaque fois, en partant de article                */
/* si celui-ci est non NULL, et de Article_deb sinon...			    */
/* on va imiter ce que fait cree_liste de art_groupe, mais en mieux si      */
/* si possible								    */
/* On suppose qu'on est déjà dans le groupe courant (cette fonction peut    */
/* par exemple se faire appeler par un truc avant cree_liste...		    */
/* retour : >=0 si succès... -1 si échec				    */
int cree_liste_xover(int n1, int n2, Article_List **input_article) {
    int res, code, suite_ligne=0, num=0, crees=0, fait_coupure=0;
    char *buf, *buf2;
    Article_List *creation, *article=input_article?*input_article:NULL;
    Range_List *msg_lus=Newsgroup_courant->read;
    int i,lus_index=0;
    int new_article=0;

    buf=safe_malloc(50*sizeof(char)); /* De toute façon, on devra */
    	/* gérer des lignes TRES longues cette fois :-( */
 /* On envoie la commande Xover */   
    sprintf(buf, "%d-%d", n1, n2);
    res=write_command(CMD_XOVER, 1, &buf);
    free(buf);
    if (res<0) return -1;
    code=return_code();
    if ((code<0) || (code>300)) return -1; 

    if (article==NULL) Article_courant=Article_deb=NULL;
    res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
    if (res<0) return -1;
    while ((suite_ligne) || (tcp_line_read[0]!='.')) {
       if (!suite_ligne) {
         num=0;
         creation = safe_calloc (1,sizeof(Article_List));
	 buf=tcp_line_read;
	 buf2=strchr(buf,'\t');
	 *buf2='\0';  /* On suppose qu'il ne peut y avoir de pbs ici */
	 creation->numero=strtol(buf,NULL,10);
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
	       new_article=0;
	       free_article_headers(article->headers);
	       article->headers=NULL;
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
	 crees+= new_article;
	 buf=buf2+1;
	 article->headers=new_header();
	 fait_coupure=0;
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
	   }
	 }
       }
       buf2=strchr(buf,'\t');
       while (buf2!=NULL) {
          *buf2='\0';
	  if (buf!=buf2) {
	    if (new_article && (overview_list[num]==-1)) {
	      if(suite_ligne) {
		article->msgid= safe_realloc(article->msgid,
		    strlen(article->msgid) + strlen(buf) +2);
		strcat(article->msgid, buf);
	      } else
		article->msgid=safe_strdup(buf);
	    } else
	    if (overview_list[num]>=0) {
	       if ((!fait_coupure) && (overview_list[num] & FULL_HEADER_OVER)) {
	          buf=strchr(buf,':');
	  	  buf++;
		  if (*buf) buf++; /* on passe aussi le ' ' */
               } 
	       i=overview_list[num] & ~FULL_HEADER_OVER;
	       if (suite_ligne) { article->headers->k_headers[i]=safe_realloc(
	          article->headers->k_headers[i], 
		    (strlen(article->headers->k_headers[i])+strlen(buf)+2));
		    strcat(article->headers->k_headers[i],buf);
		  } else
	           article->headers->k_headers[i]=safe_strdup(buf);
	    }
	  }
	  if (overview_list[num]>=0) {
	    article->headers->k_headers_checked[
	      overview_list[num] & ~FULL_HEADER_OVER ] = 1;
	  }
	  buf=buf2+1;
	  fait_coupure=0;
	  suite_ligne=0;
	  buf2=strchr(buf,'\t');
	  if (buf2==NULL) buf2=strchr(buf,'\r');
	  num++;
       }
       /* si la ligne est fini, on devrait avoir *buf='\n' */
       if (*buf!='\n') {
         if (*buf) 
	    if (overview_list[num]>=0) {
	       if ((!fait_coupure) && (overview_list[num] & FULL_HEADER_OVER)) {
	          buf=strchr(buf,':');
		  if (buf==NULL) buf=index(buf,'\0'); else {
	  	    buf++;
		    fait_coupure=1;
		  }
                } 
	     }
	 if (*buf) { 
	   if (overview_list[num] >=0) {
	       i=overview_list[num] & ~FULL_HEADER_OVER;
	       if (suite_ligne) { article->headers->k_headers[i]=safe_realloc(
	          article->headers->k_headers[i], 
		    (strlen(article->headers->k_headers[i])+strlen(buf)+2));
		    strcat(article->headers->k_headers[i],buf);
		  } else
	           article->headers->k_headers[i]=safe_strdup(buf);
	   } else
	     if (new_article && (overview_list[num] == -1)) {
	       article->msgid=safe_strdup(buf);
	     }
	 }
	 suite_ligne=1;
       } else {
	 /* On décode le header subject */
	 /* C'est un peu limité, mais bon, faudra voir ça avec Jo */
	 /* On parse la date aussi */
	 if (article->headers->k_headers[SUBJECT_HEADER])
	   rfc2047_decode(article->headers->k_headers[SUBJECT_HEADER],
	       article->headers->k_headers[SUBJECT_HEADER],
	       strlen(article->headers->k_headers[SUBJECT_HEADER]));
	 if (article->headers->k_headers[DATE_HEADER])
	    article->headers->date_gmt=parse_date(article->headers->k_headers[DATE_HEADER]);
	 suite_ligne=0;
       }
       res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
       if (res<0) return -1;
    }
    return crees;
}


int va_dans_groupe() {
   int res;
   char *buf;

   if (Newsgroup_courant==NULL) return 0;
   /* On change le newsgroup courant pour le serveur */
   buf=Newsgroup_courant->name;
   res=write_command(CMD_GROUP, 1, &buf);
   if (res<0) return -1;
   return 0;
}

/* Cette fonction cree la liste des articles du newsgroup courant
 * On suppose provisoirement que la commande XHDR fonctionne.
 * Le retour est -2 pour un newsgroup invalide. */
int cree_liste_noxover(int min, int max, Article_List *start_article) {
   int res, code, crees=0;
   char *buf, *buf2;
   Article_List *creation,*article=start_article;
   Range_List *msg_lus=Newsgroup_courant->read;
   int lus_index=0;
   
   /* Envoie de la commande XHDR
    * 270 comme taille max d'un message-ID ? */
   buf=safe_malloc(270*sizeof(char));
   sprintf(buf,"Message-ID %d-%d", min, max);
   res=write_command(CMD_XHDR, 1, &buf);
   if (res<0) {free(buf);return -1;}
   code=return_code();
   if ((code<0) || (code>300)) {free(buf);return -1;}

   if (article==NULL) Article_courant=Article_deb=NULL;
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
      sscanf(tcp_line_read, "%d %s", &(creation->numero), creation->msgid);

      creation->flag =0;
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
      }

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
      if (code == article->numero) {
	/* on construit le champ References */
	article->headers=new_header();
	article->headers->k_headers_checked[REFERENCES_HEADER] = 1;
	if ((buf2 = strchr(buf2,'<')))
	  article->headers->k_headers[REFERENCES_HEADER] =
	      safe_strdup(buf2);
      }
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) {free(buf);return -1;}
   }   
   free(buf);
   return crees;
}

int cree_liste(int art_num, int *part) {
   int min,max,res,code;
   char *buf;
   /* On change le newsgroup courant pour le serveur */
   *part=0;
   if (va_dans_groupe()<0) return -1;

   /* on n'utilise pas return_code car on veut le max du newsgroup */
   res=read_server(tcp_line_read, 3, MAX_READ_SIZE-1);
   if (res<3) {
      if (debug) fprintf(stderr, "Cree_liste: Echec en lecture du serveur\n");
      return -1;
   }
   /* On ne suppose pas que la ligne est trop longue : c'est quasi-exclu */
   if (tcp_line_read[res-1]!='\n') {
     if (debug) fprintf(stderr, "Serveur grotesque \n");
     return -1;
   }
   code=strtol(tcp_line_read, &buf, 10);

   if (code<=0) return -1;
   if (code==503) { /* Il y a eu probablement timeout */
     		    /* on lance donc un reconnect, et on */
     		    /* recommence */
     	            if (reconnect_after_timeout(0)<0) return -1;
		    return cree_liste(art_num, part);
		  }
   if (code==411) { if (debug) fprintf(stderr, "Newsgroup invalide !\n"); 
       return -2; }
   strtol(buf, &buf, 0);
   min=strtol(buf, &buf, 0);
   max=strtol(buf, &buf, 0);

   if (Newsgroup_courant->Article_deb) {
     Article_deb=Newsgroup_courant->Article_deb;
     Article_exte_deb=Newsgroup_courant->Article_exte_deb;
     if (Newsgroup_courant->max<max) /* Ca peut etre > si list active et */
     {
     /* on essaie un cree_liste_xover moins couteux qu'un cherche newnews */
       if (cree_liste_xover(Newsgroup_courant->max+1,max,&Article_deb)) {
	 Date_groupe = time(NULL)+Date_offset;
	 cree_liens();
	 apply_kill_file();
       }
       else
	 cherche_newnews();
     }
     return 1;
   }
   /* si on a deja ce qu'on veut, on n'en fait pas trop */

   if ((max<min)||(max==0)) {
     if (debug) fprintf(stderr, "Pas d'article disponibles !\n"); return 0;
   }
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
   if (overview_usable)
     res=cree_liste_xover(min,max,NULL);
   else
     res=cree_liste_noxover(min,max,NULL);

   if (res<=0) return res;
   Article_exte_deb=NULL;
   cree_liens();
   /* on memorise tout */
   /* attention, Article_deb et Article_exte_deb pourraient changer !!! */
   /* donc on ne les utilise pas, et on les remplacera a la fin... */
   Newsgroup_courant->article_deb_key=Article_deb_key;
   /* on a besoin de article_deb_key pour le kill_file... */
   apply_kill_file();
   Newsgroup_courant->Article_deb=Article_deb;
   Newsgroup_courant->Article_exte_deb=Article_exte_deb;
   return res;
}

int cree_liste_end() {
  int res;
  /* fixme */
  if (!overview_usable) return 0;
  if (Article_deb->numero <2) return 0;
  res=cree_liste_xover(1,Article_deb->numero-1,&Article_deb);
  if (res) {
    cree_liens();
    apply_kill_file();
  }
  return 0;
}
