/*/flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_filter.c : gestion du kill-file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "flrn.h"
#include "flrn_filter.h"
#include "art_group.h"
#include "group.h"
#include "flrn_lists.h"
#include "flrn_files.h"
#include "flrn_xover.h"
#include "flrn_inter.h"
#include "enc/enc_strings.h"

#include "getdate.h"

static UNUSED char rcsid[]="$Id$";

/* on met ici le contenu du kill_file */
static flrn_kill *flrn_kill_deb=NULL;
static Flrn_liste *main_kill_list=NULL; /* la liste pour l'abonnement */
static char *main_list_file_name=NULL;

/* regarde si la liste d'article correspond au filtre
 * flag & 1 : appelle XHDR en cas de besoin
 * flag & 2 : appelle XOVER en cas de besoin
 */
int check_article_list(Article_List *debut, flrn_filter *filtre, int flag,
   int min, int max) {
  int with_xhdr,std_hdr,ret,reste=0, tofree=0, savetofree=0;
  flrn_char *ligne, *saved_ligne=NULL;
  flrn_condition *cond=filtre->condition;
  Article_List *parcours=debut;
  int newmin=max,newmax,deja_test[NB_KNOWN_HEADERS],i;

  for (i=0;i<NB_KNOWN_HEADERS;i++) deja_test[i]=0;
  if (parcours==NULL) return -1;
  if ((debut->headers==NULL) && (flag & 2)) {
     if (overview_usable) 
       cree_liste_xover(min,max,&Article_deb,&newmin,&newmax);
       /* Si on a de nouveaux articles, tant mieux, de toute façon       */
       /* newmin et newmax sont compris logiquement entre min et max     */
       /* si l'overview n'est pas utilisable, on va tout faire à la main */
  }
  while (parcours->numero<=max) {
     if (parcours->art_flags & FLAG_TMP_KILL) {
         if ((parcours->art_flags & filtre->cond_mask)!=filtre->cond_res) 
	     parcours->art_flags &= ~FLAG_TMP_KILL;
	 else {
	   if (reste==0) newmin=parcours->numero;
	   reste=1;
	 }
     }
     parcours=parcours->next;
     if (parcours==NULL) break;
  }
  if (reste==0) return 0;
  min=newmin;
  if ((filtre->date_after>0) || (filtre->date_before>0)) {
     with_xhdr=((flag & 1) && (!check_in_xover(DATE_HEADER)));
     if (with_xhdr) {
        ret=launch_xhdr(min,max,fl_static_tran(Headers[DATE_HEADER].header));
	deja_test[DATE_HEADER]=1;
	if (ret==-1) with_xhdr=0;
     }
     reste=0;
     parcours=debut;
     ret=0;
     while (parcours->numero<=max) {
        if (parcours->art_flags & FLAG_TMP_KILL) {
	   if (with_xhdr) {
	      if ((ret!=-2) && (ret<parcours->numero))
	           ret=get_xhdr_line(parcours->numero,&ligne,&tofree,
			                 DATE_HEADER,parcours);
	      if (ret==-1) with_xhdr=0;
	      if (tofree) free(ligne);
	      if (ligne==NULL) {
	         if (reste==0) newmin=parcours->numero;
	         parcours=parcours->next;
		 reste=1;
                 if (parcours==NULL) break;
		 continue;
	      }
	   }
	   if ((parcours->headers) && (parcours->headers->date_gmt>0)) {
	      if ((filtre->date_after>0) && 
	        (parcours->headers->date_gmt<filtre->date_after))
	           parcours->art_flags &= ~FLAG_TMP_KILL;
	      else
	      if ((filtre->date_before>0) && 
	        (parcours->headers->date_gmt>filtre->date_before))
		   parcours->art_flags &= ~FLAG_TMP_KILL;
	      else {
	        if (reste==0) newmin=parcours->numero;
	        reste=1;
	      }
	   } else parcours->art_flags &= ~FLAG_TMP_KILL; /* pas de match */
	}
	parcours=parcours->next;
        if (parcours==NULL) break;
     }
     if ((with_xhdr) && (ret!=-2)) end_xhdr_line();
  }
  min=newmin;
  while ((reste) && (cond)) {
     reste=0;
     if (cond->cond_flags & FLRN_COND_U_HEAD) {
         with_xhdr=(flag & 1);
	 std_hdr=-1;
     } else {
         std_hdr=cond->header_ns.header_num;
         with_xhdr=((flag & 1) && (!check_in_xover(std_hdr)));
	 if ((with_xhdr) && (deja_test[std_hdr])) with_xhdr=0;
     }
     if (with_xhdr) {
        ret=launch_xhdr(min,max,(std_hdr==-1 ? cond->header_ns.header_str : 
			fl_static_tran(Headers[std_hdr].header)));
	if (std_hdr!=-1) deja_test[std_hdr]=1;
	if (ret==-1) with_xhdr=0;
     }
     ret=0;
     parcours=debut;
     while (parcours->numero<=max) {
        if (parcours->art_flags & FLAG_TMP_KILL) {
	   tofree=0;
	   if (with_xhdr) {
	       if ((ret!=-2) && (ret<parcours->numero)) {
	          ret=get_xhdr_line(parcours->numero,&ligne,&tofree,
			  std_hdr,parcours);
	          if (ret>parcours->numero) {
		    saved_ligne=ligne;
		    savetofree=tofree;
		    tofree=0;
		    ligne=fl_static("");
	          }
	       }
	       else if (ret==parcours->numero) {
		       ligne=saved_ligne;
		       tofree=savetofree;
		       savetofree=0;
		       saved_ligne=NULL;
		    }
	       /* if ret==-2, ligne=NULL and will not change */
	       if (ret==-1) {
	          with_xhdr=0;
		  if (std_hdr==-1) {
	             if (reste==0) newmin=parcours->numero;
		     reste=1;
		     break;
		  }
	       }
	       if (ligne==NULL) { ligne=fl_static(""); tofree=0; }
	   } else if (std_hdr!=-1) {
	       ligne=((parcours->headers) &&
	       (parcours->headers->k_headers[cond->header_ns.header_num]) ?
	       parcours->headers->k_headers[cond->header_ns.header_num] : 
	       fl_static(""));
	   } else ligne=fl_static("");
	   if (!(cond->cond_flags & FLRN_COND_STRING)) {
	      if (fl_regexec(cond->condition.regex,ligne,0,NULL,0)
	             !=(cond->cond_flags & FLRN_COND_REV)?REG_NOMATCH:0)
		  parcours->art_flags &= ~FLAG_TMP_KILL;
	      else {
	        if (reste==0) newmin=parcours->numero;
	        reste=1;
	      }
	   } else {
	      if (fl_strstr(ligne,cond->condition.string)!=NULL) {
	         if ((cond->cond_flags & FLRN_COND_REV)!=0)
		     parcours->art_flags &= ~FLAG_TMP_KILL; else {
	                if (reste==0) newmin=parcours->numero;
		        reste=1;
	 	     }
	      } else {
	         if ((cond->cond_flags & FLRN_COND_REV)==0)
		     parcours->art_flags &= ~FLAG_TMP_KILL; else {
	                if (reste==0) newmin=parcours->numero;
			reste=1;
		     }
	      }
	   }
	   if (tofree) free(ligne);
	}
	parcours=parcours->next;
        if (parcours==NULL) break;
     }
     if ((with_xhdr) && (ret!=-2)) end_xhdr_line();
     cond=cond->next;
     min=newmin;
  }
  /* On est au bout... FLAG_TMP_KILL donne la liste des matchs... */
  return reste;
}
      


/* regarde si l'article correspond au filtre */
/* renvoie -1, s'il manque des headers
 * 0 si ça matche
 * 1 si ça ne matche pas
 * flag=1 dit d'appeler cree_header au besoin */
int check_article(Article_List *article, flrn_filter *filtre, int flag) {
  flrn_condition *regexp=filtre->condition;
  int cree_header_done=0;
  flrn_char *ligne;
  Header_List *tmp;

  /* en premier, on regarde si les flags de l'article sont bons */
  if ((article->art_flags & filtre->cond_mask) != filtre->cond_res)
    return 1;
  if (((filtre->date_after>0) || (filtre->date_before>0)) &&
      (article->headers->k_headers_checked[DATE_HEADER]==0)) {
     if (flag) {
        if (Last_head_cmd.Article_vu!=article) 
          cree_header(article,0,1,0);
	cree_header_done=1;
     }
      else return -1;
  }
  if (filtre->date_after>0) {
     if ((article->headers) && (article->headers->date_gmt>0)) 
        if (article->headers->date_gmt<filtre->date_after) return 1;
  }
  if (filtre->date_before>0) {
     if ((article->headers) && (article->headers->date_gmt>0)) 
        if (article->headers->date_gmt>filtre->date_before) return 1;
  }
  while(regexp) {
    if (((article->headers == NULL) ||
	(regexp->cond_flags & FLRN_COND_U_HEAD) ||
	(article->headers->k_headers_checked[regexp->header_ns.header_num]==0))
	 && (cree_header_done==0))
    {
      if (flag) {
        if (Last_head_cmd.Article_vu!=article) 
          cree_header(article,0,1,0);
	cree_header_done=1;
      }
      else return -1;
    }
    if (regexp->cond_flags & FLRN_COND_U_HEAD) {
       tmp=Last_head_cmd.headers;
       while (tmp) {
          if (fl_strncasecmp(tmp->header_head,regexp->header_ns.header_str,
	  		fl_strlen(regexp->header_ns.header_str))==0) break;
	   tmp=tmp->next;
       }
       if (tmp==NULL) ligne=fl_static(""); else {
          ligne=tmp->header_body;
	  while (*ligne==' ') ligne++;
       }
    } else 
      ligne=article->headers->k_headers[regexp->header_ns.header_num]?
            article->headers->k_headers[regexp->header_ns.header_num]:
	    fl_static("");
    if (!(regexp->cond_flags & FLRN_COND_STRING)) {
      /* c'est une regexp */
      if(fl_regexec(regexp->condition.regex,ligne,
	0,NULL,0)!=(regexp->cond_flags & FLRN_COND_REV)?REG_NOMATCH:0)
	return 1;
    } else {
      /* c'est une chaine... FIXME: case-sensitive ou pas */
      if(fl_strstr(ligne,regexp->condition.string)!=NULL) {
	  if ((regexp->cond_flags & FLRN_COND_REV) != 0) return 1;
      } else {
	  if ((regexp->cond_flags & FLRN_COND_REV) == 0) return 1;
      }
    }
    regexp=regexp->next;
  }
  return 0;
}

/* Fait l'action correspondant au filtre */
void filter_do_action(flrn_filter *filt) {
  flrn_action *act;
  int old_flag=Article_courant->art_flags;
  if (filt->fltr_flag ==0) { /* il s'agit de mettre un flag */
    Article_courant->art_flags |= filt->action.art_flag;
    /* FIXME ? FLAG_KILLED => FLAG_READ */
    if (Article_courant->art_flags & FLAG_KILLED) 
      Article_courant->art_flags |= FLAG_READ;
    /* FIXME ? FLAG_READ => !FLAG_IMPORTANT */
    if (Article_courant->art_flags & FLAG_READ)
      Article_courant->art_flags &= ~FLAG_IMPORTANT;
    if ((Article_courant->numero>0) && (Newsgroup_courant->not_read>=0)) {
      if (old_flag & (~Article_courant->art_flags) & FLAG_READ) {
        Newsgroup_courant->not_read++;
        if (Article_courant->thread) Article_courant->thread->non_lu++;
      } else if ((~old_flag) & Article_courant->art_flags & FLAG_READ) {
        if (Newsgroup_courant->not_read>0) Newsgroup_courant->not_read--;
        if (Article_courant->thread) Article_courant->thread->non_lu--;
      }
      if (old_flag & (~Article_courant->art_flags) & FLAG_IMPORTANT) 
        Newsgroup_courant->important--;
      else if ((~old_flag) & Article_courant->art_flags & FLAG_IMPORTANT)
        Newsgroup_courant->important++;
    }
    return;
  }
  /* TODO : passer à la "vraie" execution d'une commande */
  act=filt->action.action;
  if (!act) return;
  while(act) {
    call_func(act->command->cmd[CONTEXT_COMMAND],act->command->after);
    act=act->next;
  }
}

flrn_filter *new_filter() {
  return (flrn_filter *)safe_calloc(1, sizeof(flrn_filter));
}

/* Parse une ligne commencant par F 
 * Par exemple : Fread */
int parse_filter_flags(flrn_char * str, flrn_filter *filt) {
  int ret,flag, flagset;
  ret=parse_flags(str,&flagset,&flag);
  if (ret==-1) return -1;
  if (ret==-2) {
     filt->cond_res = filt->cond_mask = 0;
     return 0;
  }
  if (flagset) {
     filt->cond_res |= flag;
     filt->cond_mask |= flag;
  } else {
     filt->cond_res &= ~flag;
     filt->cond_mask |= flag;
  }
  return 0;
}

/* Parse une ligne commençant par C
 * Par exemple : Comet */
int parse_filter_action(flrn_char * str, flrn_filter *filt) {
  flrn_action *act,*a2;
  flrn_char *buf=str, *buf2;
  Cmd_return *cmd;
  int ret;
  cmd=safe_calloc(1,sizeof(Cmd_return));
  filt->fltr_flag=1;
  buf2=fl_strchr(buf,fl_static(' '));
  if (buf2) *(buf2++)=fl_static('\0');
  act=safe_calloc(1,sizeof(flrn_action));
  act->command=cmd;
  ret=Lit_cmd_explicite(str,CONTEXT_COMMAND,-1,cmd);
  if (ret<0) {
    free(cmd);
    free(act); return -2;}
      /* on prend les arguments éventuels */
  if (buf2) cmd->after=safe_flstrdup(buf2);
    /* on ajoute l'action... */
  if (filt->action.action==NULL) filt->action.action=act;
  else {
    a2=filt->action.action;
    while(a2->next) a2=a2->next;
    a2->next=act;
  }
  return 0;
}

/* Parse une ligne commençant par T
 * Par exemple Tread */
int parse_filter_toggle_flag(flrn_char * str, flrn_filter *filt) {
  int ret,flag, toset;
  ret=parse_flags(str,&toset,&flag);
  if (filt->fltr_flag) return -1;
  if (ret<0) return -1;
  if (toset) {
    filt->action.art_flag = flag;
    filt->cond_mask |= flag;
    filt->cond_res &= ~flag;
    return 0;
  } 
  /* On ne peut que mettre des flags. On ne peut pas en enlever */
  return -1;
}

/* La, on parse une condition, du genre ^From: jo
 * C'est utilisé pour les kill-files et les résumés
 * Pour les kill-files, il y avait une * devant */
int parse_filter(flrn_char * istr, flrn_filter *start) {
  flrn_condition *cond, *c2;
  flrn_char *str=istr;
  flrn_char *buf;
  int i;
  cond=safe_calloc(1,sizeof(flrn_condition));

  /* si ça commence par ^ ou ~, on inverse la condition */
  while(1) {
    if (*str == fl_static('~') || *str == fl_static('^')) {
      cond->cond_flags |= FLRN_COND_REV;
      str++;
    } else
    /* si ça commence par ' ou ", on fait un match exact */
    if (*str == fl_static('\'') || *str == fl_static('"')) {
      cond->cond_flags |= FLRN_COND_STRING;
      str++;
    }
    else break;
  }
  /* on cherche le header correspondant */
  for (i=0;i<NB_KNOWN_HEADERS;i++) {
    if (fl_strncasecmp(str,
		fl_static_tran(Headers[i].header),
		Headers[i].header_len)==0) {
      cond->header_ns.header_num=i; break;
    }
  }
  if (i == NB_KNOWN_HEADERS) { 
     /* cas d'une date */
     if (fl_strncasecmp(str,fl_static("date "),5)==0) {
        buf=str+5;
	if (*buf==fl_static('>')) start->date_after=get_date(buf+1,NULL);
	  else if (*buf==fl_static('<')) 
	      start->date_before=get_date(buf+1,NULL);
	return 0;
     } else {
       char saf_chr;
       cond->cond_flags |= FLRN_COND_U_HEAD;
       buf=fl_strchr(str,fl_static(':'));
       if ((buf==NULL) || (buf==str)) {
         free(cond);
	 return -1;
       }
       saf_chr=*(++buf);
       *buf=fl_static('\0');
       cond->header_ns.header_str=safe_flstrdup(str);
       *buf=saf_chr;
     }
  } else
    buf = str + Headers[cond->header_ns.header_num].header_len;
  while(*buf==fl_static(' ')) buf++;
  /* on parse la regexp */
  if (cond->cond_flags & FLRN_COND_STRING) {
    cond->condition.string = safe_flstrdup(buf);
  } else {
    cond->condition.regex = safe_malloc(sizeof(regex_t));
    if (fl_regcomp(cond->condition.regex,buf,REG_EXTENDED|REG_NOSUB|REG_ICASE))
    {  if (cond->cond_flags & FLRN_COND_U_HEAD) free(cond->header_ns.header_str);
       free(cond->condition.regex); free(cond) ; return -2;}
  }
  /* on l'ajoute au filtre */
  if (start->condition==NULL)
    start->condition=cond;
  else {
    c2=start->condition;
    while(c2->next) c2=c2->next;
    c2->next=cond;
  }
  return 0;
}

static void free_condition( flrn_condition *cond) {
  if (!cond->cond_flags & FLRN_COND_STRING) {
    if (cond->condition.regex)
      regfree(cond->condition.regex);
      free(cond->condition.regex);
      cond->condition.regex=NULL;
  } else {
    if (cond->condition.string)
      free(cond->condition.string);
      cond->condition.string=NULL;
  }
  if (cond->cond_flags & FLRN_COND_U_HEAD) free(cond->header_ns.header_str);
  free(cond);
}

static void free_action( flrn_action *act) {
  if (act->command) {
      if (act->command->after) free (act->command->after);
      free(act->command);
  }
  free(act);
}

void free_filter(flrn_filter *filt) {
  flrn_condition *cond,*cond2;
  flrn_action *act,*act2;
  cond=filt->condition;
  while(cond) {
    cond2=cond->next;
    free_condition(cond);
    cond=cond2;
  }
  if (filt->fltr_flag) {
    act=filt->action.action;
    while(act) {
      act2=act->next;
      free_action(act);
      act=act2;
    }
  }
  free(filt);
}

void raw_free_kill(flrn_kill *kill) {
  flrn_kill *k2=kill;
  if (main_list_file_name) {
    write_list_file(main_list_file_name,main_kill_list);
    free(main_list_file_name);
    main_list_file_name=NULL;
    main_kill_list=NULL;
  }
  while(kill) {
    k2=kill->next;
    if (kill->filter)
      free_filter(kill->filter);
    if (kill->newsgroup_regexp) {
      if (kill->newsgroup_cond.regexp) {
	regfree(kill->newsgroup_cond.regexp);
	free(kill->newsgroup_cond.regexp);
      }
    } else {
      if (kill->newsgroup_cond.liste) {
	free_liste(kill->newsgroup_cond.liste);
      }
    }
    free(kill);
    kill=k2;
  }
}

void free_kill(void) {
  if (flrn_kill_deb)
    raw_free_kill(flrn_kill_deb);
  flrn_kill_deb=NULL;
}

/* Parse un block du kill-file
 * ie ce qui suit une ligne commençant par : */
flrn_filter * parse_kill_block(FILE *fi,Flrn_liste *liste) {
  char buf1[MAX_BUF_SIZE];
  char *buf2;
  flrn_char *trad;
  int rc;
  flrn_filter *filt = NULL;
  int out=0;

  while (fgets(buf1,MAX_BUF_SIZE,fi)) {
    if(buf1[0]=='\n') {return filt;} /* un block se finit par une ligne vide */
    buf2=strchr(buf1,'\n');
    if (buf2) *buf2=0;
    else out=1;
    if(filt==NULL) {
      filt = new_filter();
    /* par défaut, on s'applique aux messages non lus */
      filt->cond_mask = FLAG_READ;
      filt->cond_res = 0;
    }
    /* on appelle la bonne fonction suivant le type de ligne */
    rc=conversion_from_file(buf1+1,&trad,0,(size_t)(-1));
    if (buf1[0]=='*') out=parse_filter(trad,filt);
    else if (buf1[0]=='F') out=parse_filter_flags(trad,filt);
    else if (buf1[0]=='C') out=parse_filter_action(trad,filt);
    else if (buf1[0]=='T') out=parse_filter_toggle_flag(trad,filt);
    else if (buf1[0]==':') {
      if (liste) add_to_liste(liste,trad);
      else out=1;
    }
    else if (buf1[0]=='#') out=0;
    else out=-1;
    if (rc==0) free (trad);
    if(out) { free_filter(filt); return NULL; }
  }
  return filt;
}

/* Parse le fichier entier
 * FIXME: il n'y a pas de retour d'erreur !!! */
int parse_kill_file(FILE *fi) {
  char buf[MAX_BUF_SIZE];
  char *buf2,*buf1=buf;
  flrn_kill *kill, *k2=flrn_kill_deb;
  int out=0;
  int file=0;
  flrn_char *trad;
  int rc;

  while(fgets(buf,MAX_BUF_SIZE,fi)) {
    buf1=buf;
    buf2=strchr(buf1,'\n');
    if (buf2) *buf2=0;
    else out=1;
    if (buf1[0]=='\0') continue;  /* skip empty_lines */
    if (buf1[0]=='#') continue; /* skip comments */
    if (buf1[0]!=':') out=1; 
    else {
      buf1++;
      buf2=strrchr(buf1,':');
      if (!buf2) out=1;
      else {
	kill = safe_calloc(1,sizeof(flrn_kill));
	if (!flrn_kill_deb) flrn_kill_deb =kill;
	else k2->next=kill;
	k2=kill;
	*(buf2++)='\0';
	/* buf2 pointe sur les flags */
	kill->newsgroup_regexp=1;
	if (strchr(buf2,'f')) {
	  /* flag f, ce qui précède est un fichier ou trouver une liste */
	  kill->newsgroup_regexp=0;
	  kill->newsgroup_cond.liste=alloue_liste();
	  read_list_file(buf1,kill->newsgroup_cond.liste);
	  /* ici, exceptionnellement, je ne fais aucune conversion, ni
	   * dans un sens, ni dans l'autre, elle serait reversible directe */
	  file=1;
	}
	if (strchr(buf2,'l')) {
	  /* flag l, c'est une liste, et on en a donné le premier élément */
	  kill->newsgroup_regexp=0;
	  kill->newsgroup_cond.liste=alloue_liste();
	  rc=conversion_from_file(buf1,&trad,0,(size_t)(-1));
	  add_to_liste(kill->newsgroup_cond.liste,trad);
	  if (rc==0) free(trad);
	}
	if (strchr(buf2,'m')) {
	  /* C'est la liste principale,
	   * qui peut être modifiée intéractivement */
	  if (kill->newsgroup_regexp == 0) {
	    main_kill_list=kill->newsgroup_cond.liste;
	    /* pour le cas ou un idiot utilise plusieurs fois le flag m ! */
	    if (main_list_file_name)
	      free (main_list_file_name);
	    main_list_file_name=NULL;
	    if (file)
	      main_list_file_name=safe_strdup(buf1);
	  }
	  else out=1;
	}
	file=0;
	if (kill->newsgroup_regexp==1) {
	  kill->newsgroup_cond.regexp = safe_malloc(sizeof(regex_t));
	  rc=conversion_from_file(buf1,&trad,0,(size_t)(-1));
	  if (regcomp(kill->newsgroup_cond.regexp,trad,REG_EXTENDED|REG_NOSUB))
	    out=1;
	  else {
	    kill->filter=parse_kill_block(fi,NULL);
	    if (!kill->filter) out=1;
	  }
	  if (rc==0) free(trad);
	} else {
	    kill->filter=parse_kill_block(fi,kill->newsgroup_cond.liste);
	    if (!kill->filter) out=1;
	}
      }
    }
    if (out) {
      if (flrn_kill_deb)
	raw_free_kill(flrn_kill_deb);
      flrn_kill_deb=NULL;
      return 0;
    }
  }
  return 1;
}

/* regarde si la règle kill s'applique au groupe courant */
static int check_group(flrn_kill *kill) {
  if (kill->Article_deb_key &&
      (Newsgroup_courant->article_deb_key == kill->Article_deb_key))
    return kill->group_matched;
  kill->Article_deb_key=Newsgroup_courant->article_deb_key;
  if (kill->newsgroup_regexp) {
    if (fl_regexec(kill->newsgroup_cond.regexp,
	Newsgroup_courant->name,0,NULL,0)==0) {
      kill->group_matched=1;
      return 1;
    }
  } else {
    if (kill->newsgroup_cond.liste &&
	find_in_liste(kill->newsgroup_cond.liste,Newsgroup_courant->name)) {
      kill->group_matched=1;
      return 1;
    }
  }
  kill->group_matched=0;
  return 0;
}

/* flag = 1 -> appel cree_header
 * on s'applique a Article_courant pour les commandes qui peuvent être
 * invoquées...
 * Il faut le FLAG_NEW */
void apply_kill(int flag) {
  flrn_kill *kill=flrn_kill_deb;
  int res;
  int all_bad=1;

  if (!(Article_courant->art_flags & FLAG_NEW))
    return;

  while (kill) {
    if (check_group(kill)) {
      if ((res=check_article(Article_courant,kill->filter,flag))==0) {
	Article_courant->art_flags &= ~FLAG_NEW;
	filter_do_action(kill->filter);
      } else if (res <0) all_bad=0;
    }
    kill=kill->next;
  }
  if (all_bad || flag)
    Article_courant->art_flags &= ~FLAG_NEW;
}

void check_kill_article(Article_List *article, int flag) {
  Article_List *save;
  save_etat_loop();
  save=Article_courant;
  Article_courant=article;
  apply_kill(flag);
  Article_courant=save;
  restore_etat_loop();
}

void check_kill_article_in_list(Article_List *debut, int min, int max, int flag) {
  Article_List *save;
  int res,vide;
  flrn_kill *kill=flrn_kill_deb;
  save_etat_loop();
  save=Article_courant;
  while (kill) {
    if (check_group(kill)) {
      vide=1;
      Article_courant=debut;
      while ((Article_courant) && (Article_courant->numero<=max)) {
        if (Article_courant->art_flags & FLAG_NEW) {
	  vide=0;
          Article_courant->art_flags |= FLAG_TMP_KILL;
	}
        Article_courant=Article_courant->next;
      }
      res=(vide ? 0 : check_article_list(debut,kill->filter,flag,min,max));
      if (res) {
         Article_courant=debut;
	 while ((Article_courant) && (Article_courant->numero<=max)) {
	    if (Article_courant->art_flags & FLAG_TMP_KILL) {
	       Article_courant->art_flags &= ~(FLAG_NEW | FLAG_TMP_KILL);
	       filter_do_action(kill->filter);
	    }
	    Article_courant=Article_courant->next;
	 }
      }
      if (vide) break;
    }
    kill=kill->next;
  }
  Article_courant=debut;
  while ((Article_courant) && (Article_courant->numero<=max)) {
      Article_courant->art_flags &= ~(FLAG_NEW | FLAG_TMP_KILL);
      Article_courant=Article_courant->next;
  }
  Article_courant=save;
  restore_etat_loop();
}

int add_to_main_list(flrn_char *name) {
  if(main_kill_list) {
    add_to_liste(main_kill_list,name);
    return 0;
  }
  return -1;
}

int remove_from_main_list(flrn_char *name) {
  if(main_kill_list) {
    remove_from_liste(main_kill_list,name);
    return 0;
  }
  return -1;
}

int in_main_list (flrn_char *name) {
  return (main_kill_list &&
     find_in_liste(main_kill_list,name));
}

/* Je met ca la pour une éventuelle unification */
int parse_flags(flrn_char *name, int *toset, int *flag) {
   char *buf=name;
   int l;
   *flag=0;
   if (fl_strcasecmp(buf,fl_static("all"))==0)
       return -2; /* cas pour les filtres */
   if (fl_strncasecmp(name,fl_static("un"),2)==0) {
      buf+=2;
      *toset=0;
   } else *toset=1;
   l=fl_strlen(buf)-1;
   while ((l>0) && (fl_isblank(buf[l]))) buf[l--]=fl_static('\0');
   if (strcasecmp(buf,fl_static("read"))==0) *flag=FLAG_READ; else
   if (strcasecmp(buf,fl_static("killed"))==0) *flag=FLAG_KILLED; else
   if (strcasecmp(buf,fl_static("interesting"))==0) *flag=FLAG_IMPORTANT; else
   if (strcasecmp(buf,fl_static("toread"))==0) *flag=FLAG_TOREAD; else
   if (strcasecmp(buf,fl_static("selected"))==0) *flag=FLAG_IS_SELECTED;
   if (*flag==0) return -1;
   return 0;
}

/* ceci sert pour la complétion */
#define NUM_FLAGS_STR 5

static flrn_char *get_flag_name(void *ptr, int num) {
   char *pipo;
   switch (num%NUM_FLAGS_STR) {
      case 0 : pipo=fl_static("unread"); break;
      case 1 : pipo=fl_static("unkilled"); break;
      case 2 : pipo=fl_static("uninteresting"); break;
      case 3 : pipo=fl_static("unselected"); break;
      default : pipo=fl_static("untoread"); break;
   }
   return (num>=NUM_FLAGS_STR ? pipo+2 : pipo);
}

int flags_comp(flrn_char *str, size_t len, Liste_Chaine *debut) {
   Liste_Chaine *courant;
   int res,i;
   int result[NUM_FLAGS_STR*2];

   for (i=0;i<NUM_FLAGS_STR*2;i++) result[i]=0;
   res = Comp_generic(debut, str,len,NULL,NUM_FLAGS_STR*2,
      get_flag_name,fl_static(" "),result,0);
   if (res==-3) return 0;
   if (res>= 0) {
      if (str[0]) debut->complet=0;
      fl_strcat(debut->une_chaine,str);
      return 0;
   }
   if (res==-1) {
      courant=debut->suivant;
      for (i=0;i<NUM_FLAGS_STR*2;i++) {
        if (result[i]==0) continue;
	fl_strcat(courant->une_chaine,str);
	if (str[0]) courant->complet=0;
	courant=courant->suivant;
     }
     return -1;
   }
   return -2;
}

static flrn_char *get_header_name(void *ptr, int num) {
   static flrn_char result[20];
   char *bla;
   bla=((Known_Headers *)ptr)[num].header;
   strcpy(result, fl_static_tran(bla));
   return result;
}
int header_comp(flrn_char *str, size_t len, Liste_Chaine *debut) {
   Liste_Chaine *courant;
   int res,i;
   int result[NB_KNOWN_HEADERS];

   for (i=0;i<NB_KNOWN_HEADERS;i++) result[i]=0;
   res = Comp_generic(debut,str,len,(void *)Headers,NB_KNOWN_HEADERS,
      get_header_name,fl_static(" "),result,0);
   if (res==-3) return 0;
   if (res>= 0) {
      if (str[0]) debut->complet=0;
      fl_strcat(debut->une_chaine,str);
      return 0;
   }
   if (res==-1) {
      courant=debut->suivant;
      for (i=0;i<NUM_FLAGS_STR*2+NB_KNOWN_HEADERS;i++) {
        if (result[i]==0) continue;
	fl_strcat(courant->une_chaine,str);
	if (str[0]) courant->complet=0;
	courant=courant->suivant;
     }
     return -1;
   }
   return -2;
}
   
static flrn_char *get_flag_header_name(void *ptr, int num) {
   static flrn_char result[20];
   char *bla;
   if (num>=2*NUM_FLAGS_STR)
     bla=((Known_Headers *)ptr)[num-2*NUM_FLAGS_STR].header;
   else return get_flag_name(ptr,num);
   strcpy(result, fl_static_tran(bla));
   return result;
}
int flag_header_comp(flrn_char *str, size_t len, Liste_Chaine *debut) {
   Liste_Chaine *courant;
   int res,i;
   int result[2*NUM_FLAGS_STR+NB_KNOWN_HEADERS];

   for (i=0;i<2*NUM_FLAGS_STR+NB_KNOWN_HEADERS;i++) result[i]=0;
   res = Comp_generic(debut,str,len,(void *)Headers,2*NUM_FLAGS_STR+
       NB_KNOWN_HEADERS, get_flag_header_name,fl_static(" "),result,0);
   if (res==-3) return 0;
   if (res>= 0) {
      if (str[0]) debut->complet=0;
      fl_strcat(debut->une_chaine,str);
      return 0;
   }
   if (res==-1) {
      courant=debut->suivant;
      for (i=0;i<NUM_FLAGS_STR*2+NB_KNOWN_HEADERS;i++) {
        if (result[i]==0) continue;
	fl_strcat(courant->une_chaine,str);
	if (str[0]) courant->complet=0;
	courant=courant->suivant;
     }
     return -1;
   }
   return -2;
}
   
