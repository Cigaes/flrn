/* flrn v 0.1                                                           */
/*              art_group.c         25/11/97                            */
/*                                                                      */
/* Gestion des articles au sein d'un groupe.                            */
/*                                                                      */
/*                                                                      */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <sys/times.h>

#include "flrn.h"
#include "art_group.h"
#include "group.h"

const Known_Headers Headers[NB_KNOWN_HEADERS] = {
   { "From:", 5 },
   { "References:", 11 },
   { "Subject:", 8 },
   { "Date:", 5 },
   { "Newsgroups:", 11 },
   { "Followup-To:", 12 },
   { "Organization:", 13 },
   { "Lines:", 6 },
   { "Sender:", 7 },
   { "Reply-To:", 9 },
   { "Expires:", 8 },
   { "X-Newsreader:", 13 },
   { "To:", 3 },
   { "Cc:", 3 },
   { "Bcc:", 4 },
   { "Xref:", 5 },
   { "Message-Id:", 11 },
   { "X-Censorship:", 13 },
};


Article_List *Article_courant;
Article_List *Article_deb;
Article_List *Article_exte_deb;
Article_List Article_bidon = {NULL, NULL, NULL, NULL, NULL, NULL, -10, -10,
  FLAG_READ, NULL, NULL,NULL};
/* Est-il preferable de considerer l'article bidon comme lu ? */
/* On va reserver le numero -1 pour les articles extérieurs au newsgroup */
Thread_List *Thread_deb=NULL;
Last_headers_cmd Last_head_cmd;

long Article_deb_key=1;

time_t Date_groupe;
Hash_List *(*Hash_table)[HASH_SIZE];

/* libere la memoire d'un article */
void free_article_headers(Article_Header *headers) {
  int i;
  if (!headers) return;
  /* attention au message-id */
  for (i=0; i<NB_KNOWN_HEADERS; i++) 
    if ((i!=MESSAGE_ID_HEADER) && (headers->k_headers[i]))
      free(headers->k_headers[i]);
  if (headers->reponse_a) free(headers->reponse_a);
  free(headers);
}

static int calcul_hash(char *id) {
  int toto=0;
  toto=0; for(; *id; id++) toto += *id;
  toto %= HASH_SIZE;
  return toto;
}

static void free_one_article(Article_List *article,int flag) {
  if (!article) return;
  free_article_headers(article->headers);
  if (flag) {
    if (article->msgid) free(article->msgid);
    free(article);
  } else {
    article->headers=NULL;
  }
}

/* cette fonction annule relie_article */
static void derelie_article(Article_List *pere, Article_List *fils) {
    if ((fils==NULL) || (pere==NULL)) return;
    fils->pere=NULL;
    fils->parent=0;
    if (fils->frere_suiv) fils->frere_suiv->frere_prev=fils->frere_prev;
    if (fils->frere_prev) fils->frere_prev->frere_suiv=fils->frere_suiv;
    if (pere->prem_fils==fils) 
    		pere->prem_fils=(fils->frere_prev ? fils->frere_prev :
    							     fils->frere_suiv);
    fils->frere_prev=fils->frere_suiv=NULL;
    return;
}

/* Cette fonction relie l'article pere et l'article fils */
static void relie_article(Article_List *pere, Article_List *fils) {
    Article_List *temp;

    if ((fils==NULL) || (pere==NULL)) return;
    if (fils->pere==pere) return;
    fils->pere=pere;
    fils->parent=pere->numero;
    if (pere->prem_fils==NULL) {
        pere->prem_fils=fils;
	fils->frere_prev=fils->frere_suiv=NULL;
    /* Je renonce provisoirement à la recherche de cousins   */
    /* (surtout qu'il faudrait noter dans ce cas à quel profondeur est */
    /* le cousin.						       */
    } else {

       temp=pere->prem_fils;
       while ((temp->frere_prev) && (temp->numero>fils->numero))
             temp=temp->frere_prev;
       while ((temp->frere_suiv) && (temp->numero<fils->numero))
	  temp=temp->frere_suiv;
       if (temp->numero<fils->numero) {
         temp->frere_suiv=fils;
         fils->frere_prev=temp;
	 fils->frere_suiv=NULL;
       } else {
         fils->frere_prev=temp->frere_prev;
	 if (fils->frere_prev) fils->frere_prev->frere_suiv=fils;
         temp->frere_prev=fils;
	 fils->frere_suiv=temp;
       }
    }
    return;
}


/* Fusion de deux threads, renvoie thread1 */
/* thread2 est a priori plus petit....	   */
static Thread_List *fusionne_thread(Thread_List *thread1, Thread_List *thread2) {
   Hash_List *parcours;
   Thread_List *parcours2;

   if (thread1==thread2) return thread1;
   if (thread1==NULL) return thread2;
   if (thread2==NULL) return thread1;
   thread1->non_lu+=thread2->non_lu;
   thread1->number+=thread2->number;
   thread1->flags|=thread2->flags;
   for (parcours=thread2->premier_hash; ;
        parcours=parcours->next_in_thread) 
	{
	  if (parcours->article) parcours->article->thread=thread1;
	  parcours->thread=thread1;
	  if (parcours->next_in_thread==NULL) break;
	}
   parcours->next_in_thread=thread1->premier_hash;
   thread1->premier_hash=thread2->premier_hash;
   if (Thread_deb==thread2) {
      Thread_deb=thread2->next_thread;
   } else {
      parcours2=Thread_deb;
      while (parcours2->next_thread!=thread2) parcours2=parcours2->next_thread;
      /* j'espere que la liste est complete, sinon c'est le SEGFAULT assuré */
      parcours2->next_thread=thread2->next_thread;
   }
   free(thread2);
   return thread1;
}


/* on crée les liens à partir des headers References
 * on met des messages exterieurs pour les parents abscents */
/* il faut noter que maintenant, cree_liens sera appelée plusieurs fois
 * au fil des xovers... */
int cree_liens() {
  int hash, premier, second, arret;
  Article_List *creation;
  Article_List *creation2;
  Hash_List *parcours_hash, *save_hash;
  Thread_List *thread_creation=NULL;
  char *buf2, *buf3=NULL, *buf=NULL;

  if (Hash_table==NULL) {
     Hash_table=safe_calloc(1,sizeof(*Hash_table));
  }

  /* on fait tout en une seule passe... on est des torcheurs... */
  /* Evidemment, ça bouffe de la mémoire...			*/
  /* Si on veut économiser, l'idée c'est de vérifier via les tables */
  /* de hash quand on crée un article s'il est pas en trop... cela  */
  /* doit se faire dans cree_liste_xover...			    */
  /* [TODO] : faire ce qui est décrit au-dessus. A terme réunir article */
  /* et hash...							        */

  for(creation=Article_deb; creation; creation=creation->next) {
    if (creation->thread) continue;
    hash = calcul_hash(creation->msgid);
    for (parcours_hash=(*Hash_table)[hash]; parcours_hash;
         parcours_hash=parcours_hash->prev_hash) 
       if (strcmp(parcours_hash->msgid,creation->msgid)==0) break;
    if (parcours_hash) {
       if (parcours_hash->article==NULL) {
          free(parcours_hash->msgid);
       } else { /* C'est un ancien article extérieur */
          /* On replace ses fils */
          Article_List *parcours;
	  for (parcours=parcours_hash->article->prem_fils;
	       parcours; parcours=parcours->frere_prev) {
	       parcours->parent=creation->numero;
	       parcours->pere=creation;
	  }
	  if (parcours_hash->article->prem_fils) {
	     for (parcours=parcours_hash->article->prem_fils->frere_suiv;
	       parcours; parcours=parcours->frere_suiv) {
	       parcours->parent=creation->numero;
	       parcours->pere=creation;
	     }
	  }
	  /* Un article qui n'appartient à aucun thread n'a pas de fils */
	  creation->prem_fils=parcours_hash->article->prem_fils;
	  /* par contre, parcours_hash->article peut avoir un père */
	  /* :-( 						   */
	  parcours=parcours_hash->article;
	  if (parcours->pere) 
	     derelie_article(parcours->pere,parcours);

	  /* La ligne suivante peut servir un jour */
	  /* if (Article_courant==parcours) Article_courant=creation; */

	  /* On efface enfin l'article vidé */
	  if (parcours->prev) parcours->prev->next=parcours->next;
	     else Article_exte_deb=parcours->next;
	  if (parcours->next) parcours->next->prev=parcours->prev;
	  free_one_article(parcours,1);
       }
       parcours_hash->msgid=creation->msgid;
       parcours_hash->article=creation;
       thread_creation=parcours_hash->thread;
       thread_creation->number++;
       if (!(creation->flag & FLAG_READ)) thread_creation->non_lu++;
    } else {
       parcours_hash=safe_calloc(1,sizeof(Hash_List));
       parcours_hash->prev_hash=(*Hash_table)[hash];
       (*Hash_table)[hash]=parcours_hash;
       parcours_hash->article=creation;
       parcours_hash->msgid=creation->msgid;
       parcours_hash->thread=thread_creation=safe_calloc(1,sizeof(Thread_List));
       thread_creation->next_thread=Thread_deb;
       Thread_deb=thread_creation;
       if (!(creation->flag & FLAG_READ)) thread_creation->non_lu=1;
       thread_creation->number=1;
       thread_creation->premier_hash=parcours_hash;
       parcours_hash->next_in_thread=NULL;
    }
    creation->thread=thread_creation;
    save_hash=parcours_hash;
    premier=1;
    second=0;
    arret=0;
    if (!creation->headers ||
	!(buf=creation->headers->k_headers[REFERENCES_HEADER]))
      continue;
    while ((buf2=strrchr(buf,'<'))) {
       if (buf3) *buf3='<';
       buf3=strchr(buf2,'>');
       if (buf3==NULL) {
          if (debug) fprintf(stderr, "Header référence qui buggue !\n");
	  *buf2='\0';
	  buf3=buf2;
	  continue;
       }
       *(++buf3)='\0';
       /* J'ai donc la référence dans *buf2 */
       hash = calcul_hash(buf2);
       for (parcours_hash=(*Hash_table)[hash]; parcours_hash;
            parcours_hash=parcours_hash->prev_hash) 
           if (strcmp(parcours_hash->msgid,buf2)==0) break;
       if (parcours_hash) {
	  arret=(parcours_hash->article!=NULL);
          /* On a retrouvé un ancêtre, on s'arretera la s'il s'agit d'un */
	  /* article...							 */
	  if (premier) {
	    if (!(parcours_hash->article)) { /* on crée un article extérieur */
	       creation2 = safe_calloc (1,sizeof(Article_List));
	       creation2->numero=-1; /* c'est un article bidon ou ext */
	       creation2->next=Article_exte_deb;
	       creation2->flag=FLAG_READ;
	       if (Article_exte_deb) Article_exte_deb->prev=creation2;
	       Article_exte_deb=creation2;
	       creation2->msgid = safe_strdup(buf2);
	       creation2->thread=parcours_hash->thread;
	       parcours_hash->article=creation2;
	       free(parcours_hash->msgid);
	       parcours_hash->msgid=creation2->msgid;
	    }
	    relie_article(parcours_hash->article,creation);
	  } else if ((second) && (parcours_hash->article)) {
	      relie_article(parcours_hash->article,creation->pere);
	  }
          parcours_hash->thread=fusionne_thread(parcours_hash->thread,
	  					save_hash->thread);
       } else {
          parcours_hash=safe_calloc(1,sizeof(Hash_List));
          parcours_hash->prev_hash=(*Hash_table)[hash];
	  (*Hash_table)[hash]=parcours_hash;
          if (!premier) parcours_hash->msgid=safe_strdup(buf2);
          parcours_hash->thread=save_hash->thread;
	  parcours_hash->next_in_thread=parcours_hash->thread->premier_hash;
	  parcours_hash->thread->premier_hash=parcours_hash;
	  if (premier) {
	     creation2 = safe_calloc (1,sizeof(Article_List));
	     creation2->numero=-1; /* c'est un article bidon ou ext */
	     creation2->flag=FLAG_READ;
	     creation2->next=Article_exte_deb;
	     if (Article_exte_deb) Article_exte_deb->prev=creation2;
	     Article_exte_deb=creation2;
	     creation2->msgid = safe_strdup(buf2);
	     creation2->thread=parcours_hash->thread;
	     parcours_hash->article=creation2;
	     parcours_hash->msgid=creation2->msgid;
	     relie_article(parcours_hash->article,creation);
	  } 
       }
       save_hash=parcours_hash;
       *buf3='>';   
       buf3=buf2;
       *buf3='\0';
       if (arret) break;
       second=premier;
       premier=0;
    }
    if (buf3) *buf3='<';
  }
/*  fprintf(stderr,"\n");
  for (thread_creation=Thread_deb; thread_creation; thread_creation=thread_creation->next_thread)
  {
     parcours_hash=thread_creation->premier_hash;
     while (parcours_hash->article==NULL ||
      parcours_hash->article->numero<=0) parcours_hash=parcours_hash->next_in_thread;
     fprintf(stderr,"%d %d %s\n",thread_creation->number, parcours_hash->article->numero,parcours_hash->article->headers->k_headers[SUBJECT_HEADER]);
  }  */
  return 0;
}

void apply_kill_file() {
  Article_List *creation;
  /* 3eme passe, on regarde le kill-file */
  for(creation=Article_deb; creation; creation=creation->next)
    check_kill_article(creation,0);
}

Article_Header *new_header() {
  return (Article_Header *) safe_calloc(1,sizeof(Article_Header));
}

/* Fonction pour libérer Last_head_cmd */
static void Free_Last_head_cmd () {
   Header_List *tmp,*tmp2;
  
   if (Last_head_cmd.Article_vu==NULL) {
     Last_head_cmd.headers=NULL;
     return;
   }
   Last_head_cmd.Article_vu=NULL;
   tmp=Last_head_cmd.headers;
   while (tmp) {
     tmp2=tmp->next;
     free(tmp->header);
     free(tmp);
     tmp=tmp2;
   }
   Last_head_cmd.headers=NULL;
}

/* Cette fonction crée la structure headers d'un article. Elle renvoie	*/
/* un pointeur sur la structure.					*/
/* Si rech_pere==0, on ne recherche pas de reponse_a. reponse_a pourra  */
/* alors etre obtenu par ajoute_reponse_a.				*/
/* Si others=1, on detruit et refait les autres headers...		*/
/* si by_msgid=1, on utilise le message-ID. Alors rech_pere=0...	*/
Article_Header *cree_header(Article_List *article, int rech_pere, int others, int by_msgid) {
   int res, code, flag=2;  /* flag & 1 : debut de ligne     */
                           /*      & 2 : fin de ligne       */
   int header_courant=NULL_HEADER, i;
   Article_Header *creation;
   Header_List **actuel=NULL;
   char *num;

  /* Ecriture de la commande */
   num=safe_malloc(260*sizeof(char)); /* ca peut aussi etre une reference */
   /* on utilise en fait toujours le message id puisqu'on peut être
    * appelé sur des artciles extérieurs de numéro > 0 au groupe... */
   /* Quoiqu'en fait il est peut-etre préférable de faire l'inverse :  *
    * utiliser en priorité le numéro... C'est probablement plus rapide *
    * et parfois plus efficace...				       */
   /* ... mais si ca plante, le message-ID peut etre utile...	       */
   if ((article->numero>0) && (!by_msgid)) sprintf(num,"%d",article->numero); 
   else strcpy(num,article->msgid);
   res=write_command(CMD_HEAD, 1, &num);
   free(num);
   if (res<0) return NULL; 
   code=return_code();
   if (code==423) {  /* Bad article number ? */
       num=safe_malloc(260*sizeof(char));
       strcpy(num,article->msgid);
       res=write_command(CMD_HEAD, 1, &num);
       if (res<0) return NULL;
       code=return_code();
   }
   if ((code<0) || (code>300)) return NULL; /* Erreur, ou l'article n'est */
    					    /* pas encore disponible.	  */

   free_article_headers(article->headers);
   article->headers=creation=new_header();
   article->headers->k_headers[MESSAGE_ID_HEADER]=article->msgid;
   article->headers->k_headers_checked[MESSAGE_ID_HEADER]=1;
   if (others) {
     Free_Last_head_cmd();
     Last_head_cmd.Article_vu=article;
   }
   
  /* Resultats */
   do {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) return creation; 
      flag=(flag & 2 ? 1 : 0); 
      if (res>1) {
        flag|=2*(tcp_line_read[res-2]=='\r');
        if (flag & 2) tcp_line_read[res-2]='\0';
      } else continue;  /* ca signifie tcp_line_read='\n' */
/*      rfc2047_decode(tcp_line_read,tcp_line_read,MAX_READ_SIZE); */

      if ((flag & 1) && (!isblank(tcp_line_read[0]))) 
	                     header_courant=NULL_HEADER; 
                                                   /* On a affaire à un */
      						   /* nouveau header    */
      else {
	if (header_courant!=NULL_HEADER) {
	  if (header_courant!=UNKNOWN_HEADER) { 
	    creation->k_headers[header_courant]=
	      safe_realloc(creation->k_headers[header_courant],
		 (strlen(creation->k_headers[header_courant])+
		  strlen(tcp_line_read)+2)*sizeof(char));
	    if (flag & 1) strcat(creation->k_headers[header_courant], "\n");
	    strcat(creation->k_headers[header_courant], tcp_line_read);
	  } else { /* others==1 */
	    (*actuel)->header=
	      safe_realloc((*actuel)->header, 
		  (strlen((*actuel)->header)+
		  strlen(tcp_line_read)+2)*sizeof(char));
	    if (flag & 1) strcat((*actuel)->header, "\n");
	    strcat((*actuel)->header, tcp_line_read);
	  }
	} 
   	continue;
      }	
      for (i=0; i<NB_KNOWN_HEADERS; i++) {
	 if (strncasecmp(tcp_line_read, Headers[i].header, 
	                      Headers[i].header_len)==0) {
	   if (creation->k_headers[i]) {
               if (debug) fprintf(stderr, "Double header %s ???", 
		                            Headers[i].header);
	       header_courant=NULL_HEADER;
	   } else {
	      header_courant=i;
	      creation->k_headers[i]=safe_malloc(strlen(tcp_line_read+Headers[i].header_len)*sizeof(char));
	      strcpy(creation->k_headers[i], tcp_line_read+Headers[i].header_len+1);
	   }
	   break;
	 }
      }
      if ((i==NB_KNOWN_HEADERS) && (!(flag & 1) || (tcp_line_read[0]!='.'))) 
      {
        if (others) {
	   header_courant=UNKNOWN_HEADER;
	   if (actuel) { 
	     (*actuel)->next=safe_malloc(sizeof(Header_List));
	     actuel=&((*actuel)->next);
	   } else {
	     actuel=&(Last_head_cmd.headers);
	     (*actuel)=safe_malloc(sizeof(Header_List));
	   }
	   (*actuel)->header=safe_strdup(tcp_line_read);
        } else  header_courant=NULL_HEADER;
      }
   } while (!(flag & 1) || (tcp_line_read[0]!='.'));

   if (others) (*actuel)->next=NULL;

   /* tous les headers sont valides */
   /* on décode tout ! */
   for (i=0; i<NB_KNOWN_HEADERS; i++) {
     creation->k_headers_checked[i]=1;
     if(creation->k_headers[i]) {
      rfc2047_decode(creation->k_headers[i],creation->k_headers[i],
	  strlen(creation->k_headers[i]));
      creation->k_headers[i]=
	safe_realloc(creation->k_headers[i],strlen(creation->k_headers[i])+1);
     }
   }
   creation->all_headers=1;
   actuel=&(Last_head_cmd.headers);
   while(actuel && (*actuel)) {
      rfc2047_decode((*actuel)->header,(*actuel)->header,
	  strlen((*actuel)->header));
     (*actuel)->header=
	 safe_realloc((*actuel)->header,strlen((*actuel)->header)+1);
     actuel=&(*actuel)->next;
   }

   if (creation->k_headers[LINES_HEADER]) 
       creation->nb_lines=atoi(creation->k_headers[LINES_HEADER]);

  /* Recherche de reponse a */
   if (rech_pere) ajoute_reponse_a(article);

  /* Parsing de la date */
   if (creation->k_headers[DATE_HEADER]) 
     creation->date_gmt=parse_date(creation->k_headers[DATE_HEADER]);
   
   return creation;
}


/* Ajoute le champ reponse_a à la structure article_headers */
void ajoute_reponse_a(Article_List *article) {
   
   if (!article->pere) return;
   if (!article->headers) return;
   
   if (!article->pere->headers ||
       (article->pere->headers->k_headers_checked[FROM_HEADER]==0))
      cree_header(article->pere,0,0,0);
   if (article->pere->headers && article->pere->headers->k_headers[FROM_HEADER]) {
       article->headers->reponse_a=safe_malloc(
	        (strlen(article->pere->headers->k_headers[FROM_HEADER])+1)*sizeof(char));
       strcpy(article->headers->reponse_a, 
	         article->pere->headers->k_headers[FROM_HEADER]);
   }
   article->headers->reponse_a_checked=1;

   return;
}

/* Ajouter un message localement à la liste des messages d'un newsgroup      */
/* On ne se casse surtout pas la tête.				             */
/* On renvoie NULL en cas d'echec, l'article cree en cas de succes.	     */
/* should_retry est mis a un si il semble y avoir un pb d'update du serveur  */
Article_List *ajoute_message (char *msgid, int *should_retry) {
   Article_List *creation, *parcours, *last=NULL;
   char *buf;
   int res,code;

  /* j'espere que stat va marcher */
   *should_retry=0;
   buf=strchr(msgid,'>');
   if (buf) *(++buf)='\0'; else return NULL;
   res=write_command(CMD_STAT, 1, &msgid);
   if (res<1) return NULL;
   res=read_server_with_reconnect(tcp_line_read,3, MAX_READ_SIZE-1);
   code=strtol(tcp_line_read, &buf, 10);
   if (code>400) return NULL;
   creation=safe_calloc(1,sizeof(Article_List));
   creation->numero=strtol(buf,NULL,10);
   creation->msgid=safe_strdup(msgid);
   creation->headers=cree_header(creation, 0, 1, 0);
   if (creation->headers==NULL) {
      free(creation->msgid);
      free(creation);
      *should_retry=1;
      return NULL;
   }

   parcours=Article_deb; 
   if ((parcours!=NULL) && (parcours!=&Article_bidon)) {
     while (parcours && (parcours->numero<creation->numero)) {
         last=parcours;
         parcours=parcours->next;
     }
     if (parcours && (parcours->numero==creation->numero)) {
	free_one_article(creation,1);
	return 0; 
     }
     if (parcours!=Article_deb) {
         last -> next=creation;
         creation->prev=last;
     } else Article_deb=creation;
     if (parcours!=NULL) {
        creation->next=parcours;
        parcours->prev=creation;
     }
   } else Article_deb=creation;
   if (Newsgroup_courant->max<creation->numero) {
      Newsgroup_courant->max=creation->numero;
      Aff_newsgroup_courant();
   }
   cree_liens(); /* On va s'occuper de cet article sans thread associé */
   Newsgroup_courant->not_read++;
   creation->flag |= FLAG_NEW;
   check_kill_article(creation,0);
   return creation;
}

/* Essaie d'ajout direct d'un message localement  */
/* On ne se casse surtout pas la tête.				             */
/* On part cette fois du numéro du message, et non de son message-ID	     */
/* Enfin... d'une suite de numéro, on rend le premier qui marche...	     */
/* On renvoie NULL en cas d'echec, l'article cree en cas de succes.	     */
/* Beaucoup de code vient de ajoute_message... C'est dommage, mais tant pis  */
/* On devra corriger ça plus tard...					     */
Article_List *ajoute_message_par_num (int min, int max) {
   Article_List *creation, *parcours, *last=NULL;
   char *buf, *buf2;
   int i,res,code;

   buf2=safe_malloc(16);
   for (i=min;i<=max;i++) {
     /* j'espere que stat va marcher */
     sprintf(buf2,"%d",i);
     res=write_command(CMD_STAT, 1, &buf2);
     if (res<1) return NULL;
     res=read_server_with_reconnect(tcp_line_read,3, MAX_READ_SIZE-1);
     code=strtol(tcp_line_read, &buf, 10);
     if ((code>200) && (code<300)) break;
   }
   free(buf2);
   if (i>max) return NULL;
   buf=strchr(buf,'<'); if (buf==NULL) return NULL; /* Beurk !!! */
   buf2=strchr(buf,'>'); if (buf2) *(buf2+1)='\0';
   creation=safe_calloc(1,sizeof(Article_List));
   creation->numero=i;
   creation->msgid=safe_strdup(buf);
   creation->headers=cree_header(creation, 0, 1, 0);
   if (creation->headers==NULL) {
      free(creation->msgid);
      free(creation);
      return NULL;
   }

   parcours=Article_deb; 
   if ((parcours!=NULL) && (parcours!=&Article_bidon)) {
     while (parcours && (parcours->numero<creation->numero)) {
         last=parcours;
         parcours=parcours->next;
     }
     if (parcours && (parcours->numero==creation->numero)) {
	free_one_article(creation,1);
	return 0;  /* Théoriquement, on ne peut arriver ici */
     }
     if (parcours!=Article_deb) {
         last -> next=creation;
         creation->prev=last;
     } else Article_deb=creation;
     if (parcours!=NULL) {
        creation->next=parcours;
        parcours->prev=creation;
     }
   } else Article_deb=creation;
   if (Newsgroup_courant->max<creation->numero) {
      Newsgroup_courant->max=creation->numero;
      Aff_newsgroup_courant();
   }
   cree_liens();
   creation->flag |= FLAG_NEW;
   Newsgroup_courant->not_read++;
   check_kill_article(creation,0);
   return creation;
}


/* Libère la mémoire de la liste courante...				*/
/* On fait cela avant chaque changement de newsgroup.			*/
/* Est-ce ce qu'il faut faire ? J'en sais rien.				*/
/* Peut-etre plutot garder la structure de chaque newsgroup une fois    */
/* qu'elle a ete créée...						*/
/* En tout cas, on met a jour la liste des messages lus */
/* flag = 0 on garde des infos... */
void detruit_liste(int flag) {
   Range_List *msg_lus=Newsgroup_courant->read;
   Range_List *tmprange;
   Article_List *tmparticle=NULL;
   int lus_index=0;
   int min=1;
   int max=1;

   Article_deb_key++;
   if (!msg_lus) {
     msg_lus = safe_calloc(1,sizeof(Range_List));
     Newsgroup_courant->read = msg_lus;
   }
   /* attention au premier article... */
   if(Article_deb && (Article_deb->numero==1) &&
       !(Article_deb->flag & FLAG_READ))
     min=0;
    
   tmparticle=Article_courant=Article_deb;
   for (;Article_courant; Article_courant=tmparticle) {
       tmparticle=Article_courant->next;
       if ((Article_courant->flag & FLAG_READ) && (!min))
       {
	   min = Article_courant->numero;
       }
       if ((!(Article_courant->flag & FLAG_READ)) && min)
       {
	   msg_lus->min[lus_index] = min;
	   msg_lus->max[lus_index] = Article_courant->numero -1;
	   lus_index++;
	   if (lus_index >= RANGE_TABLE_SIZE)
	   {
	     if (!(msg_lus->next))
	       msg_lus->next = safe_calloc(1,sizeof(Range_List));
	     msg_lus=msg_lus->next;
	     lus_index=0;
	   }
	   min=0;
       }
       max = Article_courant->numero;
       free_one_article(Article_courant,flag);
   }
   if (min) {
     msg_lus->min[lus_index] = min;
     msg_lus->max[lus_index] = Newsgroup_courant->max; 
     				    /* Peut être (hélas !) different du max */
				    /* du groupe...            		    */
     				    /* Dans ce cas, on préférera peut-être  */
     				    /* le max du groupe, je sais pas... :-( */
     lus_index++;
   }
   /* on met a 0 la fin de la table */
   for (; lus_index< RANGE_TABLE_SIZE; lus_index++)
     msg_lus->min[lus_index] = msg_lus->max[lus_index] =0;
   tmprange=msg_lus->next; /* liste a liberer */
   msg_lus->next=NULL;
   while (tmprange) {
     msg_lus=tmprange;
     tmprange=msg_lus->next;
     free(msg_lus);
   }
   /* On libere aussi la structure des Article_exte_deb, mais sans question */
   /* de lecture...							    */
   Article_courant=tmparticle=Article_exte_deb;
   for (; Article_courant; Article_courant=tmparticle) {
      tmparticle=Article_courant->next;
      free_one_article(Article_courant,flag);
   }
   if(flag)
     Article_deb=Article_courant=Article_exte_deb=NULL;
   else {
     Newsgroup_courant->Article_deb=Article_deb;
     Newsgroup_courant->Article_exte_deb=Article_exte_deb;
     Newsgroup_courant->Hash_table=Hash_table;
     Newsgroup_courant->Thread_deb=Thread_deb;
   }
   Thread_deb=NULL;
   Hash_table=NULL;
   Free_Last_head_cmd();
}

void libere_liste()
{
   Article_List *tmparticle=NULL;
   int i;
   Hash_List *hash_courant, *tmp_hash=NULL;
   Thread_List *thread_courant, *tmp_thread=NULL;
   Article_courant=tmparticle=Article_deb;
   for (; Article_courant; Article_courant=tmparticle) {
      tmparticle=Article_courant->next;
      free_one_article(Article_courant,1);
   }
   Article_courant=tmparticle=Article_exte_deb;
   for (; Article_courant; Article_courant=tmparticle) {
      tmparticle=Article_courant->next;
      free_one_article(Article_courant,1);
   }
   Article_deb=Article_courant=Article_exte_deb=NULL;
   if (Hash_table)
     for (i=0;i<HASH_SIZE;i++) {
        hash_courant=tmp_hash=(*Hash_table)[i];
        for (; hash_courant; hash_courant=tmp_hash) {
          tmp_hash=hash_courant->prev_hash;
	  if (!(hash_courant->article)) free(hash_courant->msgid);
	  free(hash_courant);
        }
     }
   Hash_table=NULL;
   thread_courant=tmp_thread=Newsgroup_courant->Thread_deb;
   for (; thread_courant; thread_courant=tmp_thread) {
      tmp_thread=thread_courant->next_thread;
      free(thread_courant);
   }
}

/* donne le prochain article de la thread...
 * on ne peut pas boucler.
 * On fait un parcours en profondeur */
Article_List * raw_next_in_thread(Article_List *start, int *level)
{
  Article_List *famille;
  Article_List *famille2;
  int mylevel=0;
  int collision_vue=0;
  
  if (level) mylevel=*level;
  famille=start;
  /* on commence a descendre suivant les fils */
  if(famille->prem_fils) {
    mylevel ++;
    famille=famille->prem_fils;
    while(famille->frere_prev) /* on prend le premier fils */
      famille=famille->frere_prev;
    if (level) *level=mylevel;
    return famille;
  }
  famille2=famille;
  while(mylevel >=1) {
    /* on regarde les frères dans l'ordre */
    if(famille->frere_suiv) {
      famille=famille->frere_suiv;
      if (level) *level=mylevel;
      return famille;
    }
    /* on remonte si on n'est pas trop haut */
    if (mylevel && famille->pere) famille=famille->pere;
    else return NULL;
    if(famille2) famille2=famille2->pere;
    if(famille2) famille2=famille2->pere;
    mylevel --;
    /* On detecte les collisions
     * On veut les voir deux fois pour etre sur de ne rien oublier */
    if(famille==famille2) {
      if (collision_vue) return NULL; else collision_vue=1;
    }
  }
  return NULL;
}

/* donne l'article suivant de la thread ayant le flag flag a la valeur set
 * On se limite entre deb et fin
 * modifie *level si level!=NULL;
 * set vaut 0 ou flag... */
/* Note : si big_thread=1, on peut très bien revenir avant l'article */
/* dans l'ordre du thread. A priori, c'est normal... (en tout cas ce me semble */
Article_List * next_in_thread(Article_List *start, long flag, int *level,
    int deb, int fin, int set, int big_thread)
{
  Article_List *famille;
  Article_List *famille2;
  int mylevel=1<<30;
  int mylevel2;
  
  if (debug) fprintf(stderr,"appel a next_in_thread... %d\n",(level ? *level : -1));
  if (level) mylevel=*level;
  mylevel2=mylevel;
  if(fin ==0) fin=1<<30;
  famille=start;
  famille2=raw_next_in_thread(start,&mylevel2);
  do {
    famille = raw_next_in_thread(famille,&mylevel);
    if (famille2) famille2 = raw_next_in_thread(famille2,&mylevel2);
    if (famille2) famille2 = raw_next_in_thread(famille2,&mylevel2);
    if ((famille) && ((famille->flag & flag)==set) &&
       (famille->numero<=fin) && (famille->numero>=deb)) { 
      if (level) *level=mylevel;
      return famille;
    }
 } while(famille && (famille != famille2));
 if (famille && (famille == famille2)) {
    /* on a un cycle !!! */
    /* on essaie de trouver des freres pour en sortir */
    while(!(famille->frere_suiv)){
      famille = famille->pere;
      if (famille==famille2) return NULL;
    }
    /* y'a un embranchement */
    /* le frere ne peut pas etre dans le cycle, donc on en est sorti */
    famille=famille->frere_suiv;
    famille2=famille;
    do {
      if ((famille) && ((famille->flag & flag)==set) &&
	 (famille->numero<=fin) && (famille->numero>=deb)) { 
	if (level) *level=mylevel;
	return famille;
      }
      famille = raw_next_in_thread(famille,&mylevel);
    } while (famille && (famille != famille2));
    /* On ne teste pas big_thread ici pour des raisons de cycle */
    return NULL;
  }
  /* y'a plus rien a donner, sauf si big_thread=1 */
  /* La c'est un truc lourd, bien lourd... */
  if ((big_thread) && (start->thread)) {
     Hash_List *parcours=start->thread->premier_hash;
     if (debug) fprintf(stderr,"Recherche dans le thread générique...\n");
     while (1) {
       while (parcours && ((parcours->article==NULL) || 
     			   ((parcours->article->flag & flag)!=set) ||
     			   (parcours->article->numero<deb) ||
			   (parcours->article->numero>fin)))
	  parcours=parcours->next_in_thread;
       if (parcours) {
          famille=root_of_thread(parcours->article,0);
	  if (level) *level=1;
          if (((famille->flag & flag)==set) &&
              (famille->numero<=fin) && (famille->numero>=deb)) return famille;
	  famille2=next_in_thread(famille,flag,level,deb,fin,set,0);
	  if (famille2) return famille2;
       } else break;
       parcours=parcours->next_in_thread;
    }
  }
  if (debug) fprintf(stderr,"Echec :-(...\n");
  /* Cette fois c'est bien fini */
  return NULL;
}

/* recherche de cousins. On suppose debut->frere_prev=NULL */
/* si il y a un cycle, zut... */
Article_List *cousin_prev(Article_List *debut) {
  Article_List *ancetre, *parcours=debut->pere, *parcours2;
  int profondeur=1, remonte=1;

  ancetre=root_of_thread(debut,1);
  if (ancetre->pere) return NULL;
  while (parcours && (parcours!=ancetre)) {
    if (!remonte) {
      parcours2=parcours->prem_fils;
      if (parcours2) {
        while (parcours2->frere_suiv) parcours2=parcours2->frere_suiv;
        profondeur--;
        parcours=parcours2;
	if (profondeur==0) return parcours;
        continue;
      }
    } 
    remonte=0;
    parcours2=parcours->frere_prev;
    if (parcours2) {
      parcours=parcours2;
      continue;
    }
    remonte=1;
    parcours=parcours->pere;
    profondeur++;
  }
  return NULL;
}
Article_List *cousin_next(Article_List *debut) {
  Article_List *ancetre, *parcours=debut->pere, *parcours2;
  int profondeur=1, remonte=1;

  ancetre=root_of_thread(debut,1);
  if (ancetre->pere) return NULL;
  while (parcours && (parcours!=ancetre)) {
    if (!remonte) {
      parcours2=parcours->prem_fils;
      if (parcours2) {
        while (parcours2->frere_prev) parcours2=parcours2->frere_prev;
        profondeur--;
        parcours=parcours2;
	if (profondeur==0) return parcours;
        continue;
      }
    } 
    remonte=0;
    parcours2=parcours->frere_suiv;
    if (parcours2) {
      parcours=parcours2;
      continue;
    }
    remonte=1;
    parcours=parcours->pere;
    profondeur++;
  }
  return NULL;
}


/* Trouve l'ancetre de la thread */
/* si flag=1, on admet d'arriver hors de la liste principale */
Article_List * root_of_thread(Article_List *article, int flag) {
  Article_List *racine=article;
  Article_List *racine2=article;
  Article_List *racine3=article;

  if(racine->pere) 
    racine2=racine->pere;
  while(racine2->pere && (racine2!=racine)) {
    racine=racine->pere;
    racine3=racine2;
    racine2=racine2->pere;
    if(racine2->pere) {
      racine3=racine2;
      racine2=racine2->pere;
    }
  }
  return ((flag || (racine2->numero!=-1)) ? racine2 : racine3);
}

/* Parse le champ Xref, et marque les copies comme lues */
void article_read(Article_List *article)
{
  Newsgroup_List *mygroup;
  char *newsgroup;
  char *num;
  char *buf;
  int numero;
  if ((article->headers==NULL) ||
      (article->headers->k_headers_checked[XREF_HEADER]==0))
    cree_header(article,0,0,0); 
  if (!(article->flag & FLAG_READ) && (article->numero>0) &&
  	(Newsgroup_courant->not_read>0)) {
  		Newsgroup_courant->not_read--;
		article->thread->non_lu--;
  }
  article->flag |= FLAG_READ;
  if ((article->headers==NULL) ||
      (article->headers->k_headers[XREF_HEADER]==0)) return;
    /* Eviter un segfault quand le serveur est Bipesque */
  if (rindex(article->headers->k_headers[XREF_HEADER],':') ==
      index(article->headers->k_headers[XREF_HEADER],':')) return;
  buf=safe_strdup(article->headers->k_headers[XREF_HEADER]);
  newsgroup=strtok(buf," "); /* magog.ens.fr */
  while((newsgroup=strtok(NULL," :")))
  {
    num=strtok(NULL,": ");
    if (!num) {free(buf); return;}
    numero=atoi(num);
    if (strcmp(newsgroup,Newsgroup_courant->name)==0) continue;
    for(mygroup=Newsgroup_deb;mygroup; mygroup=mygroup->next){
      if (strcmp(mygroup->name,newsgroup)==0) {
	add_read_article(mygroup,numero); 
	break;
      }
    }
  }
  free(buf); return;
}

/* Recherche d'un article par numero... retour : article obtenu    */
/* et point de depart (c'est mieux)				   */
/* flags : si non trouvé : -1 : prendre le precedent               */
/* 0 : renvoyer NULL, 1 : prendre le suivant 	                   */
/* retour : 0 : OK    -1 : renvoye le proche  -2 : rien a renvoyer */
int Recherche_article (int num, Article_List **retour, int flags) {
    Article_List *parcours=*retour;

    if (parcours==NULL) parcours=Article_deb; else
      if (parcours->numero==-1) parcours=parcours->prem_fils;
    /* On ne recherche que dans le newsgroup */
    if (parcours==NULL) return -2;
    if (parcours->numero==num) {
        *retour=parcours;
	return 0;
    }
    if (parcours->numero<num) {
        while (parcours->next && (parcours->next->numero<=num)) 
           parcours=parcours->next;
        if ((parcours->numero==num) || (flags==-1)) {
	   *retour=parcours;
	   return -(parcours->numero!=num); 
        }
        if (flags==1) {
	   *retour=parcours->next;
	   return (*retour ? -1 : -2);
        }
	*retour=NULL;
        return -2;
    }
    while (parcours->prev && (parcours->prev->numero>=num))
       parcours=parcours->prev;
    if ((parcours->numero==num) || (flags==1)) {
        *retour=parcours;
        return -(parcours->numero!=num); 
    }
    if (flags==-1) {
       *retour=parcours->prev;
       return (*retour ? -1 : -2);
    }
    *retour=NULL;
    return -2;
}

/* Détermine si on est le propriétaire d'un article */
/* retour : 1 si oui, 0 si non, -1 si bug */
int Est_proprietaire(Article_List *article) {
  char *la_chaine, *ladresse, *buf;
   
  if (!article->headers) {
     cree_header(article,0,0,0);
     if (!article->headers) return -1;
  }
  la_chaine=article->headers->k_headers[SENDER_HEADER];
  if (!la_chaine) la_chaine=article->headers->k_headers[FROM_HEADER];
  if (!la_chaine) return -1;
#ifndef DOMAIN
  /* On veut une adresse EXACTE */
  ladresse=safe_malloc(257+strlen(flrn_user->pw_name));
  Get_user_address(ladresse);
  buf=strstr(la_chaine,ladresse);
  if (buf==NULL) {
    free(ladresse);
    return 0;
  }
  if ((buf!=la_chaine) && (isalnum(*(buf-1)))) {
    free(ladresse);
    return 0;
  }
  if (isalnum(*(buf+strlen(ladresse)))) {
    free(ladresse);
    return 0;
  }
  free(ladresse);
#else
  ladresse=NULL;
  /* on veut un login plus un nom de domaine */
  buf=strstr(la_chaine,flrn_user->pw_name);
  if (buf==NULL) return 0;
  if ((buf!=la_chaine) && (isalnum((int) *(buf-1)))) return 0;
  if (isalnum((int) *(buf+strlen(flrn_user->pw_name)))) return 0;
  buf=strstr(la_chaine,DOMAIN);
  if (buf==NULL) return 0;
  if ((*(buf-1))!='.') return 0;
  if (isalnum((int) *(buf+strlen(DOMAIN)))) return 0;
#endif
  return 1; /* C'est bon */
}

