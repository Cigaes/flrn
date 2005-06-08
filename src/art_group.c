/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *	art_group.c : gestion des articles d'un groupe
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <sys/times.h>

#include "flrn.h"
#include "art_group.h"
#include "group.h"
#include "flrn_tcp.h"
#include "flrn_format.h"
#include "tty_display.h"
#include "rfc2047.h"
#include "flrn_filter.h"
#include "flrn_regexp.h"
#include "post.h"
#include "options.h"

static UNUSED char rcsid[] = "$Id$";

const Known_Headers Headers[NB_KNOWN_HEADERS] = {
   { "Newsgroups:", 11 },
   { "Followup-To:", 12 },
   { "Date:", 5 },
   { "Lines:", 6 },
   { "Expires:", 8 },
   { "Xref:", 5 },
   { "X-Trace:", 8 },
   { "From:", 5 },
   { "Subject:", 8 },
   { "Organization:", 13 },
   { "Sender:", 7 },
   { "Reply-To:", 9 },
   { "X-Newsreader:", 13 },
   { "To:", 3 },
   { "Cc:", 3 },
   { "Bcc:", 4 },
   { "X-Censorship:", 13 },
   { "References:", 11 },
   { "Message-Id:", 11 },
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
static void free_article_headers(Article_Header *headers) {
  int i;
  if (!headers) return;
  /* attention au message-id */
  for (i=0; i<NB_DECODED_HEADERS; i++) 
    if (headers->k_headers[i])
      free(headers->k_headers[i]);
  for (i=0; i<NB_RAW_HEADERS; i++)
    if ((i!=MESSAGE_ID_HEADER) && (headers->k_raw_headers[i]))
	free(headers->k_raw_headers[i]);
  if (headers->reponse_a) free(headers->reponse_a);
  free(headers);
}

static int calcul_hash(char *id) {
  unsigned int toto=0; /* éviter les valeurs négatives */
  toto=0; for(; *id; id++) toto += ((unsigned char) *id);
  toto %= HASH_SIZE;
  return ((int)toto);
}

void free_one_article(Article_List *article,int whole) {
  if (!article) return;
  free_article_headers(article->headers);
  if (whole) {
    if (article->msgid) free(article->msgid);
    free(article);
  } else {
    article->headers=NULL;
  }
}

/* cette fonction annule relie_article */
static void derelie_article(Article_List *pere, Article_List *fils) {
    if (fils->frere_suiv) fils->frere_suiv->frere_prev=fils->frere_prev;
    if (fils->frere_prev) fils->frere_prev->frere_suiv=fils->frere_suiv;
    if (pere->prem_fils==fils) 
    		pere->prem_fils=(fils->frere_prev ? fils->frere_prev :
    							     fils->frere_suiv);
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
   thread1->thr_flags|=thread2->thr_flags;
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
  char save_chr;

  if (Hash_table==NULL) {
     Hash_table=safe_calloc(1,sizeof(*Hash_table));
  }

  /* on fait tout en une seule passe... on est des torcheurs... */
  /* Evidemment, ça bouffe de la mémoire...			*/

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
	  /* helas oui... */
	  if (Article_courant==parcours) Article_courant=creation;
	  /* on doit même tester, pour le Quisar serveur, les non-lus */
	  if ((parcours->numero!=-1) && (!(parcours->art_flags & FLAG_READ))) {
	      Newsgroup_courant->not_read--;
	      parcours->thread->non_lu--;
	  }

	  /* On efface enfin l'article vidé */
	  if (parcours->prev) parcours->prev->next=parcours->next;
	     else {
	       /* n'excluons jamais le pire : un doublon dans le xover */
	       /* (on a deja vu ca sur le serveur de Quisar) */
	       if (parcours==Article_deb) Article_deb=parcours->next;
	         else Article_exte_deb=parcours->next;
             }
	  if (parcours->next) parcours->next->prev=parcours->prev;
	  free_one_article(parcours,1);
       }
       parcours_hash->msgid=creation->msgid;
       parcours_hash->article=creation;
       thread_creation=parcours_hash->thread;
       thread_creation->number++;
       if (!(creation->art_flags & FLAG_READ)) thread_creation->non_lu++;
    } else {
       parcours_hash=safe_calloc(1,sizeof(Hash_List));
       parcours_hash->prev_hash=(*Hash_table)[hash];
       (*Hash_table)[hash]=parcours_hash;
       parcours_hash->article=creation;
       parcours_hash->msgid=creation->msgid;
       parcours_hash->thread=thread_creation=safe_calloc(1,sizeof(Thread_List));
       thread_creation->next_thread=Thread_deb;
       Thread_deb=thread_creation;
       if (!(creation->art_flags & FLAG_READ)) thread_creation->non_lu=1;
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
	!(buf=creation->headers->k_raw_headers[REFERENCES_HEADER]))
      continue;
    while ((buf2=strrchr(buf,'<'))) {
       if (buf3) *buf3='<';
       buf3=strchr(buf2,'>');
       if (buf3==NULL) {
          if (debug) fprintf(stderr, "Header référence qui buggue : %s !\n",buf2);
	  *buf2='\0';
	  buf3=buf2;
	  continue;
       }
       save_chr=*(++buf3);
       *buf3='\0';
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
	       creation2->art_flags=FLAG_READ;
	       if (Article_exte_deb) Article_exte_deb->prev=creation2;
	       Article_exte_deb=creation2;
	       creation2->msgid = safe_flstrdup(buf2);
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
          if (!premier) parcours_hash->msgid=safe_flstrdup(buf2);
          parcours_hash->thread=save_hash->thread;
	  parcours_hash->next_in_thread=parcours_hash->thread->premier_hash;
	  parcours_hash->thread->premier_hash=parcours_hash;
	  if (premier) {
	     creation2 = safe_calloc (1,sizeof(Article_List));
	     creation2->numero=-1; /* c'est un article bidon ou ext */
	     creation2->art_flags=FLAG_READ;
	     creation2->next=Article_exte_deb;
	     if (Article_exte_deb) Article_exte_deb->prev=creation2;
	     Article_exte_deb=creation2;
	     creation2->msgid = safe_flstrdup(buf2);
	     creation2->thread=parcours_hash->thread;
	     parcours_hash->article=creation2;
	     parcours_hash->msgid=creation2->msgid;
	     relie_article(parcours_hash->article,creation);
	  } 
       }
       save_hash=parcours_hash;
       *buf3=save_chr;   
       buf3=buf2;
       *buf3='\0';
       if (arret) break;
       second=premier;
       premier=0;
    }
    if (buf3) *buf3='<';
  }
  return 0;
}

void apply_kill_file(int min, int max) {
  Article_List *creation;
  /* 3eme passe, on regarde le kill-file */
  for(creation=Article_deb; creation; creation=creation->next)
     if (creation->numero>=min) break;
  if (creation) check_kill_article_in_list(creation,min,max,1);
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
     free(tmp->header_head);
     free(tmp->header_body);
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
   char *cur_header=NULL;
   size_t cur_header_len=0;

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
   article->headers->k_raw_headers[MESSAGE_ID_HEADER]=article->msgid;
   article->headers->k_headers_checked[MESSAGE_ID_HEADER]=1;
   if (others) {
     Free_Last_head_cmd();
     Last_head_cmd.Article_vu=article;
   }
   
  /* Resultats */
   do {
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<0) return NULL;  /* TODO : ceci est une erreur fatale */
      flag=(flag & 2 ? 1 : 0); 
      if (res>1) {
        flag|=2*(tcp_line_read[res-2]=='\r');
        if (flag & 2) tcp_line_read[res-2]='\0';
      } else continue;  /* ca signifie tcp_line_read='\n' */
/*      rfc2047_decode(tcp_line_read,tcp_line_read,MAX_READ_SIZE); */

      if ((flag & 1) && (!isblank(tcp_line_read[0]))) {
	  /* finir l'ancien header, en le décodant de suite */
	  if (header_courant!=NULL_HEADER) {
	      if (header_courant>=NB_DECODED_HEADERS) {
		  header_courant-=NB_DECODED_HEADERS;
		  creation->k_raw_headers[header_courant]=
		      safe_strdup(cur_header);
	      } else {
	        size_t ll = 3*strlen(cur_header)+3;
	        flrn_char *nouv_header;
	        nouv_header=safe_malloc(ll*sizeof(flrn_char));
	        ll=rfc2047_decode(nouv_header,cur_header, ll, 
			((header_courant>=0)
			 && (header_courant<NB_UTF8_HEADERS)) ? 2 : 0);
	        nouv_header=safe_realloc(nouv_header,
		      (1+ll)*sizeof(flrn_char));
	        if (header_courant!=UNKNOWN_HEADER) {
		    creation->k_headers[header_courant]=nouv_header;
                } else {
		    (*actuel)->header_body=nouv_header;
                }
	      }
	  }
          header_courant=NULL_HEADER; 
      } else {
	if ((header_courant!=NULL_HEADER) ||
		( (cur_header ? strlen(cur_header) : 0)+
		  strlen(tcp_line_read)+2>cur_header_len)) {
	    cur_header_len=(cur_header ? strlen(cur_header) : 0)+
		strlen(tcp_line_read)+2;
	    cur_header = safe_realloc(cur_header,
		    cur_header_len);
	    if (flag & 1) strcat(cur_header, "\n");
	    strcat(cur_header, tcp_line_read);
	}
   	continue;
      }	
      for (i=0; i<NB_KNOWN_HEADERS; i++) {
	 if (strncasecmp(tcp_line_read, Headers[i].header, 
	                      Headers[i].header_len)==0) {
	   if (creation->k_headers[i]) {
/* TODO : on peut avoir un message de warning ici, 
 * *** EXCEPTE POUR LE MID ****/
/*               if (debug) fprintf(stderr, "Double header %s ???", 
		                            Headers[i].header);    */
	       header_courant=NULL_HEADER;
	   } else {
	      char *ch=tcp_line_read+Headers[i].header_len;
	      while (*ch==' ') ch++;
	      header_courant=i;
              if (cur_header_len<=strlen(ch)+1)
	      {
		  cur_header_len=strlen(ch)+1;
		  cur_header=safe_realloc(cur_header, cur_header_len);
	      }
	      strcpy(cur_header, ch);
	   }
	   break;
	 }
      }
      if ((i==NB_KNOWN_HEADERS) && (!(flag & 1) || (tcp_line_read[0]!='.'))) 
      {
        if (others) {
	   char *ch, sch;
	   size_t ll;
	   ch=strchr(tcp_line_read,':');
	   if (ch==NULL) /* TODO : on pourrait mettre un warning ici */
	       header_courant=NULL_HEADER;
	   else {
	     header_courant=UNKNOWN_HEADER;
	     if (actuel) { 
	       (*actuel)->next=safe_malloc(sizeof(Header_List));
	       actuel=&((*actuel)->next);
	     } else {
	       actuel=&(Last_head_cmd.headers);
	       (*actuel)=safe_malloc(sizeof(Header_List));
	     }
	     ch++; sch=*ch; *ch='\0';
	     ll=4*strlen(tcp_line_read)+4;
	     (*actuel)->header_head=safe_malloc(
			ll*sizeof(flrn_char));
	     ll=rfc2047_decode((*actuel)->header_head,tcp_line_read,ll,1);
	     (*actuel)->header_head=safe_realloc((*actuel)->header_head,
		(ll+1)*sizeof(flrn_char));
	     *ch=sch; while (*ch==' ') ch++;
             if (cur_header_len<=strlen(ch))
	     {
		  cur_header_len=strlen(ch)+1;
		  cur_header=safe_realloc(cur_header, cur_header_len);
	     }
	     strcpy(cur_header, ch);
	   }
        } else  header_courant=NULL_HEADER;
      }
   } while (!(flag & 1) || (tcp_line_read[0]!='.'));
   if (cur_header) free(cur_header);

   if ((others) && (actuel)) (*actuel)->next=NULL;

   /* tous les headers sont valides */
   for (i=0; i<NB_KNOWN_HEADERS; i++) {
     creation->k_headers_checked[i]=1;
   }
   creation->all_headers=1;
   if (others) {
     actuel=&(Last_head_cmd.headers);
   }

   if (creation->k_headers[LINES_HEADER]) 
       creation->nb_lines=fl_atoi(creation->k_headers[LINES_HEADER]);

  /* Recherche de reponse a */
   if (rech_pere) ajoute_reponse_a(article);

  /* Parsing de la date */
   if (creation->k_headers[DATE_HEADER]) 
     creation->date_gmt=parse_date(creation->k_headers[DATE_HEADER]);
   /* un article doit avoir un sujet */
   if (creation->k_headers[SUBJECT_HEADER]==NULL) {
       creation->k_headers[SUBJECT_HEADER]=safe_flstrdup(fl_static(""));
   }
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
	        (fl_strlen(article->pere->headers->k_headers[FROM_HEADER])+1)
		*sizeof(flrn_char));
       fl_strcpy(article->headers->reponse_a, 
	         article->pere->headers->k_headers[FROM_HEADER]);
   }
   article->headers->reponse_a_checked=1;

   return;
}

/* il est important d'utiliser get_one_header sur l'instant, ou (mieux)
 * de le réallouer, à cause du risque de traduction "statique" */
flrn_char *get_one_header(Article_List *article, Newsgroup_List *newsgroup,
                         flrn_char *name, int *tofree) {
   int i,len,rep_a=0;
   flrn_char *buf;
   Header_List *parcours;

   *tofree=0;
   buf=fl_strchr(name,fl_static(':'));
   if (buf) len=buf-name; else len=fl_strlen(name);
   /* cas particulier de la reponse */
   if (fl_strncasecmp(fl_static(" References"),name,len)==0) {
       i = NB_DECODED_HEADERS + REFERENCES_HEADER;
       rep_a=1;
   } else
   for (i=0;i<NB_KNOWN_HEADERS;i++) 
       if ((len+1==Headers[i].header_len) &&
           (fl_strncasecmp(Headers[i].header,name,len)==0)) break;
   if ((article->headers==NULL) || 
       ((i<NB_KNOWN_HEADERS) && (article->headers->k_headers_checked[i]==0)) ||
       ((i>=NB_KNOWN_HEADERS) && (Last_head_cmd.Article_vu!=article))) {
       if (cree_header(article,rep_a,(i==NB_KNOWN_HEADERS),
               newsgroup==Newsgroup_courant ? 0 : 1)==NULL) 
	   return fl_static("");
   }
   if (i<NB_DECODED_HEADERS) {
       if (article->headers->k_headers[i]==NULL) return fl_static("");
       else return article->headers->k_headers[i];
   }
   if (i<NB_KNOWN_HEADERS) 
   {
       flrn_char *resstr;
       if (rep_a) {
	   if (!article->headers->reponse_a_checked)
	       ajoute_reponse_a(article);
	   if (article->headers->reponse_a==NULL) return fl_static("");
	   else return article->headers->reponse_a;
       }
       if (article->headers->k_raw_headers[i-NB_DECODED_HEADERS]==NULL)
	   return fl_static("");
       *tofree=0;
       resstr=fl_dynamic_tran(article->headers->k_raw_headers[i-NB_DECODED_HEADERS],tofree);
       return resstr;
   }
   parcours=Last_head_cmd.headers;
   while (parcours) {
      if ((fl_strncasecmp(parcours->header_head,name,len)==0) &&
          ((parcours->header_head)[len]==':')) 
      {
	  if (parcours->header_body==NULL) return fl_static("");
	  return parcours->header_body;
      }
      parcours=parcours->next;
   }
   return fl_static("");
}
          

/* Appelé par ajoute_message et ajoute_message_par_num */
static Article_List *ajoute_message_fin(Article_List *creation) {
   Article_List *parcours, *last=NULL;

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
   if (Newsgroup_courant->virtual_in_not_read)
      Newsgroup_courant->virtual_in_not_read--; else
      Newsgroup_courant->not_read++;
   creation->art_flags |= FLAG_NEW;
   check_kill_article(creation,1);
   return creation;
}

/* Ajouter un message localement à la liste des messages d'un newsgroup      */
/* On ne se casse surtout pas la tête.				             */
/* On renvoie NULL en cas d'echec, l'article cree en cas de succes.	     */
/* should_retry est mis a un si il semble y avoir un pb d'update du serveur  */
Article_List *ajoute_message (char *msgid, int *should_retry) {
   Article_List *creation;
   char *buf;
   flrn_char *buf2;
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
   creation->numero=strtol(buf,NULL,10); /* Ceci ne marche pas toujours */
   		/* ça renvoie 0 avec les versions récentes de INN */
   creation->msgid=safe_strdup(msgid);
   cree_header(creation, 0, 1, 0);
   if (creation->headers==NULL) {
      free(creation->msgid);
      free(creation);
      *should_retry=1;
      return NULL;
   } else if (creation->headers->k_headers[FROM_HEADER]==NULL) 
   {
     free_article_headers(creation->headers);
     free(creation->msgid);
     free(creation);
     *should_retry=1;
     return NULL;
   }
   if (creation->numero==0) {
      /* on veut vérifier si le numéro est autre chose */
      /* seul le XREF le permet. Si le Xref n'existe pas, on abandonne */
      if (creation->headers->k_headers[XREF_HEADER]!=NULL) {
	  buf2 = creation->headers->k_headers[XREF_HEADER];
	  while ((buf2=fl_strstr(buf2,Newsgroup_courant->name))) {
	      if ((buf2==creation->headers->k_headers[XREF_HEADER])
		  || (fl_isblank(*(buf2-1)))) {
		  buf2+=fl_strlen(Newsgroup_courant->name);
                  if (*buf2==fl_static(':'))  {
		       creation->numero=fl_strtol(buf2+1,NULL,10);
		       break;
		  }
	      } else buf2+=fl_strlen(Newsgroup_courant->name);
	  }
      }
   }
   return ajoute_message_fin(creation);
}

/* Essaie d'ajout direct d'un message localement  */
/* On ne se casse surtout pas la tête.				             */
/* On part cette fois du numéro du message, et non de son message-ID	     */
/* Enfin... d'une suite de numéro, on rend le premier qui marche...	     */
/* On renvoie NULL en cas d'echec, l'article cree en cas de succes.	     */
/* Beaucoup de code vient de ajoute_message... C'est dommage, mais tant pis  */
Article_List *ajoute_message_par_num (int min, int max) {
   Article_List *creation;
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
   } else if (creation->headers->k_headers[FROM_HEADER]==NULL) 
   {
     free_article_headers(creation->headers);
     free(creation->msgid);
     free(creation);
     return NULL;
   }

   return ajoute_message_fin(creation);
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
       (!(Article_deb->art_flags & FLAG_READ) || 
         (Article_deb->art_flags & FLAG_WILL_BE_OMITTED)))
     min=0; 
    
   tmparticle=Article_courant=Article_deb;
   for (;Article_courant; Article_courant=tmparticle) {
       tmparticle=Article_courant->next;
       if (Article_courant && (Article_courant->art_flags &
		   FLAG_WILL_BE_OMITTED)) {
         if (Article_courant->art_flags & FLAG_READ) {
            if (Newsgroup_courant->not_read!=-1) Newsgroup_courant->not_read++;
	    Article_courant->thread->non_lu++;
	 }
         Article_courant->art_flags&=~(FLAG_READ | FLAG_WILL_BE_OMITTED);
       }
       if ((Article_courant->art_flags & FLAG_READ) && (!min))
       {
	   min = Article_courant->numero;
       }
       if ((!(Article_courant->art_flags & FLAG_READ)) && min)
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
     				    /* Dans ce cas, on préférera peut-être  */
     				    /* le max du groupe, je sais pas... :-( */
     lus_index++;
   }
   /* cas particulier : si aucun message existant n'a été lu, on ne met
    * rien, comme ça en cas de non-abonnement la ligne sera supprimée
    * du .flnewsrc (cf save_groups) */
   if ((lus_index==1) && (Newsgroup_courant->read==msg_lus) &&
	   (Newsgroup_courant->grp_flags & GROUP_UNSUBSCRIBED) &&
	   (Newsgroup_courant->Article_deb!=NULL) &&
	   (Newsgroup_courant->Article_deb->numero>msg_lus->max[0])) {
       msg_lus->min[0] = msg_lus->max[0] =0;
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
   Article_deb=NULL;
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
    if ((famille) && ((famille->art_flags & flag)==set) &&
       (famille->numero<=fin) && (famille->numero>=deb)) { 
      if (level) *level=mylevel;
      return famille;
    }
 } while(famille && (famille != famille2));
 if (famille && (famille == famille2)) {
    /* on a un cycle !!! */
    /* attention, ca ne prouve pas que famille et famille2 sont dans le
       cycle même, ils peuvent être dans les frere_prev du cycle */
    /* Il n'est même pas prouvé qu'on ait un message non lu dans ces
       frere_prev... Pour tester ça :
        1) on revient dans le cycle avec des pere
	2) a chaque frere_prev, on teste la sous-branche
	3) si on ne trouve rien, on teste pour les frere_suiv, qui permettent
	   de sortir du cycle à tout coup */
    do {
       famille=famille->pere;
       famille2=famille2->pere;
       famille2=famille2->pere;
    } while (famille!=famille2);
    /* là on est dans le cycle */
    {
      Article_List *famille3;
      do {
         if (famille->frere_prev) {
           famille3=famille->frere_prev;
	   /* famille3 est hors du cycle */
	   while (famille3 && (famille3!=famille)) {
	     if ((famille3) && ((famille3->art_flags & flag)==set) &&
                (famille3->numero<=fin) && (famille3->numero>=deb)) {
                if (level) *level=mylevel;
                return famille3;
             }
	     famille3=raw_next_in_thread(famille3,&mylevel);
	   }
	 }
	 famille=famille->pere;
      } while (famille!=famille2);
    }
    /* rien dans les frere_prev */
    /* on prend les frere_suiv */
    while(!(famille->frere_suiv)){
      famille = famille->pere;
      if (famille==famille2) return NULL;
    }
    /* y'a un embranchement */
    /* le frere_suiv ne peut pas etre dans le cycle, donc on en est sorti */
    famille=famille->frere_suiv;
    famille2=famille;
    do {
      if ((famille) && ((famille->art_flags & flag)==set) &&
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
     while (1) {
       while (parcours && ((parcours->article==NULL) || 
     			   ((parcours->article->art_flags & flag)!=set) ||
     			   (parcours->article->numero<deb) ||
			   (parcours->article->numero>fin)))
	  parcours=parcours->next_in_thread;
       if (parcours) {
          famille=root_of_thread(parcours->article,1);
	  if (level) *level=1;
          if (((famille->art_flags & flag)==set) &&
              (famille->numero<=fin) && (famille->numero>=deb)) return famille;
	  famille2=next_in_thread(famille,flag,level,deb,fin,set,0);
	  if (famille2) return famille2;
       } else break;
       parcours=parcours->next_in_thread;
    }
  }
  /* Cette fois c'est bien fini */
  if (debug) fprintf(stderr,"Echec de next_in_thread...\n");
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
  Article_List *num_positif=NULL;

  if (racine->numero>0) num_positif=racine;
  if(racine->pere) 
    racine2=racine->pere;
  while(racine2->pere && (racine2!=racine)) {
    racine=racine->pere;
    racine2=racine2->pere;
    if (racine2->numero>0) num_positif=racine2;
    if(racine2->pere) racine2=racine2->pere;
    if (racine2->numero>0) num_positif=racine2;
  }
  if ((!flag) && (racine2->numero==-1))
     racine2=num_positif;
  return racine2; /* On peut rendre NULL si pas de pere possible !!! */
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
  if (!(article->art_flags & FLAG_READ) && (article->numero>0) &&
  	(Newsgroup_courant->not_read>0)) {
  		Newsgroup_courant->not_read--;
		article->thread->non_lu--;
  }
  if (article->art_flags & FLAG_IMPORTANT) {
     Newsgroup_courant->important--;
     article->art_flags &= ~FLAG_IMPORTANT;
  }
  article->art_flags |= FLAG_READ;
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
      if (parcours->numero==-1) {
          parcours=parcours->prem_fils;
	  while (parcours && (parcours->numero==-1)) 
	             parcours=parcours->frere_suiv;
          if (parcours==NULL) parcours=Article_deb;
      }
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


void recherche_article_par_msgid(Article_List **larticle,
                Newsgroup_List **legroupe, char *msgid) {
   int h=calcul_hash(msgid);
   Hash_List *recherche=(*Hash_table)[h];
   while (recherche) {
      if ((recherche->article) && (strcmp(msgid,recherche->msgid)==0)) {
         *legroupe=Newsgroup_courant;
         *larticle=recherche->article;
         return;
      }
      recherche=recherche->prev_hash;
   }
   for (*legroupe=Newsgroup_deb;*legroupe;*legroupe=(*legroupe)->next) {
      if ((*legroupe)->Hash_table==NULL) continue;
      if (*legroupe==Newsgroup_courant) continue;
      recherche=(*((*legroupe)->Hash_table))[h];
      while (recherche) {
         if ((recherche->article) && (strcmp(msgid,recherche->msgid)==0)) {
            *larticle=recherche->article;
            return;
         }
      }
      recherche=recherche->prev_hash;
   }
   *larticle=NULL;
}


/* Détermine si on est le propriétaire d'un article */
/* retour : 1 si oui, 0 si non, -1 si bug */
/* strict = 1 : on veut vérifier pour le cancel */
int Est_proprietaire(Article_List *article, int strict) {
  flrn_char *la_chaine, *ladresse, *buf;

#ifdef MODE_EXPERT
  if (strict) return 1;
#endif
  if (!article->headers) {
     cree_header(article,0,0,0);
     if (!article->headers) return -1;
  }
  if ((strict==0) || 
	  ((la_chaine=article->headers->k_headers[SENDER_HEADER])==NULL)) {
      regex_t reg;
      la_chaine=article->headers->k_headers[FROM_HEADER];
      if (!la_chaine) return -1;
      if ((Options.alternate) 
	       && (fl_regcomp(&reg, Options.alternate, REG_EXTENDED)==0)) {
         int ret;
	 ret = fl_regexec(&reg, la_chaine, 0, NULL, 0);
	 fl_regfree(&reg);
	 if (ret==0) return 1;
      }
  }
#ifndef DOMAIN
  /* On veut une adresse EXACTE */
  ladresse=safe_malloc((257+strlen(flrn_user->pw_name))*sizeof(flrn_char));
  Get_user_address(ladresse);
  buf=fl_strstr(la_chaine,ladresse);
  if (buf==NULL) {
    free(ladresse);
    return 0;
  }
  if ((buf!=la_chaine) && (fl_isalnum(*(buf-1)))) {
    free(ladresse);
    return 0;
  }
  if (fl_isalnum(*(buf+strlen(ladresse)))) {
    free(ladresse);
    return 0;
  }
  free(ladresse);
#else
  ladresse=NULL;
  /* on veut un login plus un nom de domaine */
  buf=fl_strstr(la_chaine,fl_static_tran(flrn_user->pw_name));
  if (buf==NULL) return 0;
  if ((buf!=la_chaine) && (fl_isalnum(*(buf-1)))) return 0;
  if (fl_isalnum(*(buf+strlen(flrn_user->pw_name)))) return 0;
  buf=fl_strstr(la_chaine,fl_static_tran(DOMAIN));
  if (buf==NULL) return 0;
  if ((*(buf-1))!=fl_static('.')) return 0;
  if (fl_isalnum(*(buf+strlen(DOMAIN)))) return 0;
#endif
  return 1; /* C'est bon */
}

