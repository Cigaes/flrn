#include "flrn.h"
#include "flrn_filter.h"
#include "art_group.h"
#include "flrn_lists.h"

/* on met ici le contenu du kill_file */
static flrn_kill *flrn_kill_deb=NULL;
static Flrn_liste *main_kill_list=NULL; /* la liste pour l'abonnement */
static char *main_list_file_name=NULL;

/* regarde si condition est vérifié */
/* renvoie -1, s'il manque des headers
 * 0 si ça matche
 * 1 si ça ne matche pas
 * flag=1 dit d'appeler cree_header au besoin */
int check_article(Article_List *article, flrn_filter *filtre, int flag) {
  flrn_condition *regexp=filtre->condition;

  /* en premier, on regarde si les flags de l'article sont bons */
  if ((article->flag & filtre->cond_mask) != filtre->cond_res)
    return 1;

  /* regarde s'il y a lieu de faire un xover */
  if (article->headers==NULL) {
    int first, last;
    first=last=article->numero;
    if (article->prev && article->prev->headers==NULL) {
      first -=50;
      if (first <1) first=1;
    }
    if (article->next && article->next->headers==NULL) {
      last +=50;
    }
    if (overview_usable) if(cree_liste_xover(first, last, &Article_deb)) {
      /* en fait, on a trouvé de nouveaux articles !!! */
      cree_liens();
      apply_kill_file();
    }
  }
  while(regexp) {
    if ((article->headers == NULL) ||
	(article->headers->k_headers_checked[regexp->header_num]==0)) {
      if (flag) cree_header(article,0,0);
      else return -1;
    }
    if(regexec(regexp->condition,
	article->headers->k_headers[regexp->header_num]?
	article->headers->k_headers[regexp->header_num]:"",
	0,NULL,0)!=(regexp->flags & FLRN_COND_REV)?REG_NOMATCH:0)
      return 1;
    regexp=regexp->next;
  }
  return 0;
}

void filter_do_action(flrn_filter *filt) {
  flrn_action *act;
  if (filt->flag ==0) {
    Article_courant->flag |= filt->action.flag;
    return;
  }
  act=filt->action.action;
  if (!act) return;
  while(act) {
    call_func(act->command_num,act->arg);
    act=act->next;
  }
}

flrn_filter *new_filter() {
  return (flrn_filter *)safe_calloc(1, sizeof(flrn_filter));
}

int parse_filter_flags(char * str, flrn_filter *filt) {
  if (strcmp(str,"unread") ==0 ) { filt->cond_mask |= FLAG_READ;
    filt->cond_res &= ~FLAG_READ;
  } else
  if (strcmp(str,"read") ==0 ) {
    filt->cond_res |=  FLAG_READ;
    filt->cond_mask |= FLAG_READ;
  } else
  if (strcmp(str,"all") ==0 )
    filt->cond_res = filt->cond_mask = 0;
  else return -1;
  return 0;
}

int parse_filter_action(char * str, flrn_filter *filt) {
  flrn_action *act,*a2;
  char *buf=str, *buf2;
  filt->flag=1;
  buf2=strchr(buf,' ');
  if (buf2) *(buf2++)='\0';
  act=safe_calloc(1,sizeof(flrn_action));
  act->command_num=fonction_to_number(str);
  if (act->command_num == -1) {
    free(act); return -2;}
  if (buf2) {act->arg=safe_strdup(buf2);}
  if (filt->action.action==NULL) filt->action.action=act;
  else {
    a2=filt->action.action;
    while(a2->next) a2=a2->next;
    a2->next=act;
  }
  return 0;
}

int parse_filter_toggle_flag(char * str, flrn_filter *filt) {
  if (filt->flag) return -1;
  if (strcmp(str,"read") ==0 )
    filt->action.flag = FLAG_READ;
  else
  if (strcmp(str,"action") ==0 )   /* pour la mise au point */
    filt->action.flag = FLAG_ACTION;
  else return -1;
  filt->cond_mask |= filt->action.flag;
  filt->cond_res &= ~filt->action.flag;
  return 0;
}

int parse_filter(char * istr, flrn_filter *start) {
  flrn_condition *cond, *c2;
  char *str=istr;
  char *buf;
  int i;
  cond=safe_calloc(1,sizeof(flrn_condition));

  if (*str == '~' || *str == '^') {
    cond->flags |= FLRN_COND_REV;
    str++;
  }
  for (i=0;i<NB_KNOWN_HEADERS;i++) {
    if (strncmp(str,Headers[i].header,Headers[i].header_len)==0) {
      cond->header_num=i; break;
    }
  }
  if (i == NB_KNOWN_HEADERS) { free(cond) ; return -1; }
  buf = str + Headers[cond->header_num].header_len;
  while(*buf==' ') buf++;
  cond->condition = safe_malloc(sizeof(regex_t));
  if (regcomp(cond->condition,buf,REG_EXTENDED|REG_NOSUB|REG_ICASE))
  {free(cond) ; return -2;}
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
  if (cond->condition)
    regfree(cond->condition);
  cond->condition=NULL;
}

static void free_action( flrn_action *act) {
  if (act->arg) free(act->arg);
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
  if (filt->flag) {
    act=filt->action.action;
    while(act) {
      act2=act->next;
      free_action(act);
      act=act2;
    }
  }
  free(filt);
}

void free_kill(flrn_kill *kill) {
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

flrn_filter * parse_kill_block(FILE *fi,Flrn_liste *liste) {
  char buf1[MAX_BUF_SIZE];
  char *buf2;
  flrn_filter *filt = NULL;
  int out=0;

  while (fgets(buf1,MAX_BUF_SIZE,fi)) {
    if(buf1[0]=='\n') {return filt;}
    buf2=strchr(buf1,'\n');
    if (buf2) *buf2=0;
    else out=1;
    if(filt==NULL) {
      filt = new_filter();
    /* par défaut, on s'applique aux messages non lus */
      filt->cond_mask = FLAG_READ;
      filt->cond_res = 0;
    }
    if (buf1[0]=='*') out=parse_filter(buf1+1,filt);
    else if (buf1[0]=='F') out=parse_filter_flags(buf1+1,filt);
    else if (buf1[0]=='C') out=parse_filter_action(buf1+1,filt);
    else if (buf1[0]=='T') out=parse_filter_toggle_flag(buf1+1,filt);
    else if (buf1[0]==':') {
      if (liste) add_to_liste(liste,buf1+1);
      else out=1;
    }
    else if (buf1[0]=='#') out=0;
    else out=-1;
    if(out) { free_filter(filt); return NULL; }
  }
  return filt;
}

int parse_kill_file(FILE *fi) {
  char buf[MAX_BUF_SIZE];
  char *buf2,*buf1=buf;
  flrn_kill *kill, *k2=flrn_kill_deb;
  int out=0;
  int file=0;

  while(fgets(buf,MAX_BUF_SIZE,fi)) {
    buf1=buf;
    buf2=strchr(buf1,'\n');
    if (buf2) *buf2=0;
    else out=1;
    if (buf1[0]=='\n') continue; /* skip empty_lines */
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
	  kill->newsgroup_regexp=0;
	  kill->newsgroup_cond.liste=alloue_liste();
	  read_list_file(buf1,kill->newsgroup_cond.liste);
	  file=1;
	}
	if (strchr(buf2,'l')) {
	  kill->newsgroup_regexp=0;
	  kill->newsgroup_cond.liste=alloue_liste();
	  add_to_liste(kill->newsgroup_cond.liste,buf1);
	}
	if (strchr(buf2,'m')) {
	  if (kill->newsgroup_regexp == 0) {
	    main_kill_list=kill->newsgroup_cond.liste;
	    if (file)
	      main_list_file_name=safe_strdup(buf1);
	  }
	  else out=1;
	}
	file=0;
	/* FIXME : parser les listes */
	if (kill->newsgroup_regexp==1) {
	  kill->newsgroup_cond.regexp = safe_malloc(sizeof(regex_t));
	  if (regcomp(kill->newsgroup_cond.regexp,buf1,REG_EXTENDED|REG_NOSUB))
	    out=1;
	  else {
	    kill->filter=parse_kill_block(fi,NULL);
	    if (!kill->filter) out=1;
	  }
	} else {
	    kill->filter=parse_kill_block(fi,kill->newsgroup_cond.liste);
	    if (!kill->filter) out=1;
	}
      }
    }
    if (out) {
      if (flrn_kill_deb)
	free_kill(flrn_kill_deb);
      flrn_kill_deb=NULL;
      return 0;
    }
  }
  return 1;
}

static int check_group(flrn_kill *kill) {
  if (kill->Article_deb_key &&
      (Newsgroup_courant->article_deb_key == kill->Article_deb_key))
    return kill->group_matched;
  kill->Article_deb_key=Newsgroup_courant->article_deb_key;
  if (kill->newsgroup_regexp) {
    if (regexec(kill->newsgroup_cond.regexp,
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

/* flag = 1 -> appel cree_header */
/* on s'applique a Article_courant. Il faut le FLAG_NEW */
void apply_kill(int flag) {
  flrn_kill *kill=flrn_kill_deb;
  int res;
  int all_bad=1;

  if (!(Article_courant->flag & FLAG_NEW))
    return;

  while (kill) {
    if (check_group(kill)) {
      if ((res=check_article(Article_courant,kill->filter,flag))==0) {
	Article_courant->flag &= ~FLAG_NEW;
	filter_do_action(kill->filter);
      } else if (res <0) all_bad=0;
    }
    kill=kill->next;
  }
  if (all_bad || flag)
    Article_courant->flag &= ~FLAG_NEW;
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

int add_to_main_list(char *name) {
  if(main_kill_list) {
    add_to_liste(main_kill_list,name);
    return 0;
  }
  return -1;
}

int remove_from_main_list(char *name) {
  if(main_kill_list) {
    remove_from_liste(main_kill_list,name);
    return 0;
  }
  return -1;
}
