/* flrn v 0.1                                                           */
/*              group.c             24/11/97                            */
/*                                                                      */
/* Gestion des groupes...                                               */
/*                                                                      */
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "flrn.h"
#include "group.h"
#include "options.h"
#include "flrn_menus.h"

Newsgroup_List *Newsgroup_courant;
Newsgroup_List *Newsgroup_deb;
Newsgroup_List *Newsgroup_deleted=NULL;

time_t Last_check;


/* Lit les articles lus marques dans le .flnews */
/* La syntaxe est celle d'un .newsrc standard */
/* On lui passe un pointeur apres l'espace qui suit ':' ou '!' */
/* on renvoie aussi le max lu...				*/
Range_List *init_range(char *buf, int *max)
{
   Range_List *creation;
   char *index=buf;
   int i=0;
   if ((buf ==NULL)  || (*buf == '\0'))
     return NULL;
   /* allocation du premier bloc */
   creation = safe_calloc(1,sizeof(Range_List));
   do {
     /* on lit le premier nombre */
     creation->min[i] = creation->max[i] = strtol(index,&index,10);
     if (*max<creation->max[i]) *max=creation->max[i];
     if (*index=='\0')
       return creation;
     /* syntaxe 2-5 ? */
     if (*index=='-')
     {
       creation->max[i] = -strtol(index,&index,10);
       if (*max<creation->max[i]) *max=creation->max[i];
       if (*index=='\0')
	 return creation;
     }
     if (*index==',') /* le delimiteur */
       index++;
     else {
       fprintf(stderr,"Erreur dans le fichier .flnewsrc\n");
       sleep(1); /* on n'a pas encore initialisé l'écran */
       return NULL;
     }
     i++;
   } while (i<RANGE_TABLE_SIZE);
   /* ca deborde, on alloue un second bloc */
   creation->next=init_range(index,max);
   return creation;
}

static Newsgroup_List *add_group(Newsgroup_List **p) {
  Newsgroup_List *creation;
  creation = safe_calloc(1,sizeof(Newsgroup_List));
  if (*p) (*p)->next=creation;
  creation->prev=*p;
  if (!Newsgroup_deb) Newsgroup_deb=creation;
  *p=creation;
  return creation;
}

/* Creation de la liste des newsgroup a partir du .flnewsrc. */
void init_groups() {
   FILE *flnews_file;
   Newsgroup_List *creation;
   char *buf, *deb, *ptr;
   char name[MAX_PATH_LEN];
   int size_buf=1024;
   
   Newsgroup_courant=Newsgroup_deb=NULL;
   strcpy(name,DEFAULT_FLNEWS_FILE);
   if (Options.flnews_ext) strcat(name,Options.flnews_ext);
   flnews_file = open_flrnfile(name,"r",1,&Last_check);

   if (flnews_file==NULL) return;

   buf=deb=safe_malloc(size_buf*sizeof(char));
   while (fgets(deb,(deb==buf ? size_buf : 1025),flnews_file))
   	 /* si deb!=buf, on vient d'agrandir buf... */
   {
      /* on vire le \n de la fin de la ligne, qui peut ne pas exister ? */
      if ((deb=strchr(deb,(int) '\n')))
        *deb='\0';
      if (strlen(buf)==size_buf-1) { /* on n'a pas lu le \n */
         if (debug) 
	 	fprintf(stderr,"On agrandit la taille de buf (init_groups)\n");
      	 size_buf+=1024;
	 buf=safe_realloc(buf,size_buf*sizeof(char));
	 deb=buf+(size_buf-1025); /* on peut lire 1024 caractères, et non 1023 */
	 continue;
      }
      /* on cherche le debut de la liste des articles lus */
      ptr=deb=buf; while (*ptr==' ') ptr++;
      if (*ptr=='\0') continue;
      deb=strchr(ptr,(int) ' ');
      if (deb) *(deb++)='\0';
      if (strlen(ptr)>MAX_NEWSGROUP_LEN)
            { fprintf(stderr,"Nom du newsgroup %s dans le .flnewsrc trop long !!! \n",ptr);
	      exit(1); }
      creation = add_group(&Newsgroup_courant);
      strncpy(creation->name, ptr, strlen(ptr)-1);
      creation->flags=(ptr[strlen(ptr)-1]==':' ? 0 : GROUP_UNSUBSCRIBED);
      if (in_main_list(creation->name)) 
        creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
      creation->name[strlen(ptr)-1]='\0';
      creation->min=1;
      creation->max=0;
      creation->read=init_range(deb,&(creation->max));
      deb=buf;
   }
   if (Newsgroup_courant) Newsgroup_courant->next=NULL;
   free(buf);
   fclose (flnews_file);
}

/* Ajoute un newsgroup a la liste... Appelé par les fonctions qui suivent */
/* la_ligne comprend, séparé par des ' ', le nom, la taille...             */
/* On vérifie avant l'existence du newsgroup...				  */
static Newsgroup_List *un_nouveau_newsgroup (char *la_ligne) 
{
    Newsgroup_List *actuel;
    Newsgroup_List *creation;
    char *buf;

    buf=strchr(la_ligne,' ');
    *(buf++)='\0';

    actuel=Newsgroup_deb;
    if (actuel) {
      while (actuel->next) {
	if (strcmp(la_ligne,actuel->name)==0) return actuel;
	actuel=actuel->next;
      }
      if (strcmp(la_ligne,actuel->name)==0) return actuel;
    }
    creation=add_group(&actuel);
    strncpy(creation->name, la_ligne, MAX_NEWSGROUP_LEN);
    creation->max = strtol(buf, &buf, 0);
    creation->min = strtol(buf, &buf, 0);
    creation->read= NULL;
    creation->description= NULL;
    creation->flags=GROUP_UNSUBSCRIBED | GROUP_NEW_GROUP_FLAG;
    if (in_main_list(creation->name)) 
      creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
    while (*buf && (isblank(*buf))) buf++;
/* TODO : améliorer ce test */
    switch (*buf) {
      case 'n' :
      case 'j' :
      case '=' :
      case 'x' : creation->flags |=GROUP_READONLY_FLAG;
      		 break;
    }
    creation->flags |= GROUP_READONLY_TESTED;
    return creation;
}

/* Test pour la creation de nouveaux newsgroup (par defaut place pour en */
/* fin de newsgroup).							 */
void new_groups(int opt_c) {
   struct tm *gmt_check;
   Newsgroup_List *creation;
   int res, code;
   char *buf;
   char param[20];
   int good, bad;
   regex_t goodreg, badreg;

  /* On forme la date en GMT */
   if (Last_check!=0) {
      gmt_check=gmtime(&Last_check); 
      res=strftime(param, 20, "%y%m%d %H%M%S GMT", gmt_check);
      param[res]='\0';
      if (debug) fprintf(stderr, "Date formée : %s\n",param);
   
      buf=param; 
      res=write_command(CMD_NEWGROUPS, 1, &buf);
   } else res=write_command(CMD_LIST, 0, NULL);
      
   if (res<0) return;
   code=return_code();
   
   if (code>400) {
      if (debug) fprintf(stderr,"Code d'erreur\n");
      return;
   }
  
   Newsgroup_courant=Newsgroup_deb; 
   while (Newsgroup_courant && Newsgroup_courant->next)
      Newsgroup_courant=Newsgroup_courant->next;      
  /* Ici, Newsgroup_courant==NULL signifie que le .flnewsrc etait vide */
   /* on lit la premiere ligne avant la boucle, pour savoir s'il
    * faut compiler les regexp */
   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<1) {
          if (debug) fprintf(stderr, "Echec en lecture du serveur\n");
          return;
   }
   if (tcp_line_read[1]=='\r') return; /* a priori, on a lu ".\r\n" */

   if (Options.auto_subscribe) {
     if (regcomp(&goodreg,Options.auto_subscribe,REG_EXTENDED|REG_NOSUB))
     {
       char buf[1024];
       regerror(regcomp(&goodreg,Options.auto_subscribe,REG_EXTENDED|REG_NOSUB),
	   &goodreg,buf,1024);
       if (debug) fprintf(stderr,"Goodreg : %s\n",buf);
       return;
     }
   }

   if(Options.auto_ignore) {
     if (regcomp(&badreg,Options.auto_ignore,REG_EXTENDED|REG_NOSUB)) {
       char buf[1024];
       if (Options.auto_subscribe) regfree(&goodreg);
       regerror(regcomp(&badreg,Options.auto_ignore,REG_EXTENDED|REG_NOSUB),
	   &badreg,buf,1024);
       if (debug) fprintf(stderr,"Badreg : %s\n",buf);
       return;
     }
   }

   /* on vire les flags GROUP_NEW_GROUP_FLAG */
   Newsgroup_courant=Newsgroup_deb;
   if (Newsgroup_courant!=NULL)
     while (Newsgroup_courant->next) {
     Newsgroup_courant->flags &= ~GROUP_NEW_GROUP_FLAG;
     Newsgroup_courant=Newsgroup_courant->next;
     }

   do {
      if (res<1) {
        if (debug) fprintf(stderr, "Echec en lecture du serveur\n");
	if (Options.auto_subscribe) regfree(&goodreg);
	if (Options.auto_ignore) regfree(&badreg);
        return;
      }
      if (tcp_line_read[1]=='\r') break; /* a priori, on a lu ".\r\n" */
      /* On parcourt à chaque fois l'ensemble des newsgroup pour vérifier */
      /* qu'on ne recrée pas un newsgroup.				  */
      creation=un_nouveau_newsgroup(tcp_line_read);
      if (!(creation->flags & GROUP_NEW_GROUP_FLAG)) {
	res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
	continue;
      }
      Newsgroup_courant=creation;
      creation->flags=GROUP_NEW_GROUP_FLAG;
      good=bad=0;
      if(Options.auto_ignore)
        bad=(regexec(&badreg,creation->name,0,NULL,0)==0)?1:0;
      if(Options.auto_subscribe)
        good=(regexec(&goodreg,creation->name,0,NULL,0)==0)?1:0;
      creation->flags |=
	(!Options.default_subscribe && !good)?GROUP_UNSUBSCRIBED:0;
      if (Options.subscribe_first)
	creation->flags |= (bad)?GROUP_UNSUBSCRIBED:0;
      else
	creation->flags |= (bad&&!good)?GROUP_UNSUBSCRIBED:0;

      if (Options.auto_kill && ((creation->flags & GROUP_UNSUBSCRIBED)==0)) 
	add_to_main_list(creation->name);
      if (opt_c) fprintf(stdout, "Nouveau groupe : %s\n", creation->name);
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   } while (1);
   if (Options.auto_subscribe) regfree(&goodreg);
   if (Options.auto_ignore) regfree(&badreg);
}

/* Creation du .flnewsrc a partir de la liste des newsgroups */
/* libere la memoire occupee par les groupes */
void free_groups(int save_flnewsrc) {
   FILE *flnews_file=NULL;
   Newsgroup_List *tmpgroup=NULL;
   Range_List *msg_lus,*tmprange;
   int lu_index;
   int first=1;
   int write_flnewsrc=save_flnewsrc;
   char name[MAX_PATH_LEN];
   
   if (debug) fprintf(stderr, "Sauvegarde du .flnewsrc\n");
   if (write_flnewsrc)
   {
     strcpy(name,DEFAULT_FLNEWS_FILE);
     if (Options.flnews_ext) strcat(name,Options.flnews_ext);
     strcat(name,".new");
     flnews_file = open_flrnfile(name,"w+",1,NULL);
     if (flnews_file==NULL) write_flnewsrc=0;;
   }
   for (Newsgroup_courant=Newsgroup_deb;Newsgroup_courant;
       Newsgroup_courant=tmpgroup) {
     tmpgroup=Newsgroup_courant->next;
     if (Newsgroup_courant->description) free(Newsgroup_courant->description);
     /* on construit la ligne du .flnewsrc */
     /* Je propose de ne pas marquer le groupe quand on n'est non abonné */
     /* et qu'on a rien lu...						 */
     msg_lus=Newsgroup_courant->read;
     first=1;
     if (write_flnewsrc) {
       int name_written;
       if (!(Newsgroup_courant->flags & GROUP_UNSUBSCRIBED)) {
	 fprintf(flnews_file,"%s: ",Newsgroup_courant->name);
	 name_written=1;
       } else name_written=0;
       while (msg_lus)
       {
	  for (lu_index=0;lu_index<RANGE_TABLE_SIZE; lu_index++)
	  {
	    if (msg_lus->min[lu_index]==0) continue;
	    if (!name_written) {
	      fprintf(flnews_file,"%s! ",Newsgroup_courant->name);
	      name_written=1;
	    }
	    if (!first) putc(',',flnews_file);
	    first=0;
	    fprintf(flnews_file,"%d",msg_lus->min[lu_index]);
	    if (msg_lus->min[lu_index]<msg_lus->max[lu_index])
	      fprintf(flnews_file,"-%d",msg_lus->max[lu_index]);
	  }
	  msg_lus=msg_lus->next;
       }
       if (name_written) fprintf(flnews_file,"\n");
     }
     /* on libere la liste des messages lus */
     tmprange=msg_lus=Newsgroup_courant->read;
     while(tmprange) {
       msg_lus=tmprange;
       tmprange=msg_lus->next;
       free(msg_lus);
     }
     Newsgroup_courant->read=NULL;
     if (Newsgroup_courant->Article_deb) {
       Article_deb=Newsgroup_courant->Article_deb;
       Article_exte_deb=Newsgroup_courant->Article_exte_deb;
       libere_liste();
       Newsgroup_courant->Article_deb=NULL;
       Newsgroup_courant->Article_exte_deb=NULL;
       Newsgroup_courant->article_deb_key=0;
     }
     free(Newsgroup_courant);
   }
   if (write_flnewsrc) {
     fclose(flnews_file);
     if (debug) fprintf(stderr,"Ecriture du .flnewsrc.new finie.\n");
     rename_flnewsfile(name,NULL);
     if (debug) fprintf(stderr,"Renommage du .flnewsrc fini.\n");
   }
   if (debug) fprintf(stderr,"%s","C'est fini\n");
}

/* Demande au serveur l'existence d'un newsgroup         */
/* L'ajoute à la liste des newsgroup existants (en fin)  */
/* name contient deja s'il y a lieu les *... */
/* flag=1 : avec prefixe_groupe */
Newsgroup_List *cherche_newsgroup_re (char *name, regex_t reg, int flag)
{
    int res, code;
    char *buf;
    buf=safe_malloc((MAX_NEWSGROUP_LEN+12)*sizeof(char));
    strcpy(buf, "active ");
    if (flag) strcat(buf, Options.prefixe_groupe);
    strcat(buf, name);
    res=write_command(CMD_LIST, 1, &buf);
    free(buf);
    if (res<1) return NULL;
    code=return_code();
    if ((code<0) || (code>400)) return NULL;
    while((res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1))>=0)
    {
      if (tcp_line_read[1]=='\r') return NULL; /* Existe pas... */
      buf=strchr(tcp_line_read,' ');
      *buf='\0';
      buf=tcp_line_read+ (flag ? strlen(Options.prefixe_groupe) : 0);
      if (!regexec(&reg,buf,0,NULL,0)) {
	*buf=' ';
	buf=safe_strdup(tcp_line_read);
        discard_server();
	strcpy(tcp_line_read,buf); free(buf);
	return un_nouveau_newsgroup(tcp_line_read);
      }
    }
    return NULL;
}

Newsgroup_List *cherche_newsgroup (char *name, int exact, int flag) {
    int res, code;
    Newsgroup_List *creation;
    char *buf;

    buf=safe_malloc((MAX_NEWSGROUP_LEN+12)*sizeof(char));
    strcpy(buf, "active ");
    if (flag) strcat(buf,Options.prefixe_groupe);
    if (!exact) strcat(buf, "*");
    strcat(buf, name);
    if (!exact) strcat(buf, "*");
    res=write_command(CMD_LIST, 1, &buf);
    free(buf);
    if (res<1) return NULL;

    code=return_code();
    if ((code<0) || (code>400)) return NULL;

    res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
    if (res<0) return NULL;
    if (tcp_line_read[1]=='\r') return NULL; /* Existe pas... */

    creation=un_nouveau_newsgroup(tcp_line_read);

    discard_server();
    return creation;
}

/*  La fonction est semblable à cherche_newsgroup_re, mais elle créée un */
/* menu au lieu de renvoyer le premier newsgroup obtenu */
/* si avec_reg & 2, on utilise options.prefixe_groupe */
Liste_Menu *menu_newsgroup_re (char *name, regex_t reg, int avec_reg)
{
    Liste_Menu *lemenu=NULL, *courant=NULL;
    int res, code;
    Newsgroup_List *creation;
    char *buf;
    buf=safe_malloc((MAX_NEWSGROUP_LEN+12)*sizeof(char));
    strcpy(buf, "active ");
    if (avec_reg & 2) strcat(buf,Options.prefixe_groupe);
    if (!(avec_reg & 1)) strcat(buf,"*");
    strcat(buf, name);
    if (!(avec_reg & 1)) strcat(buf,"*");
    res=write_command(CMD_LIST, 1, &buf);
    free(buf);
    if (res<1) return NULL;
    code=return_code();
    if ((code<0) || (code>400)) return NULL;
    while((res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1))>=0)
    {
      if (tcp_line_read[1]=='\r') return lemenu; /* On peut repartir */
      buf=strchr(tcp_line_read,' ');
      *buf='\0';
      if ((!(avec_reg & 1)) || (!regexec(&reg,tcp_line_read,0,NULL,0))) {
	*buf=' ';
	creation=un_nouveau_newsgroup(tcp_line_read);
	courant=ajoute_menu(courant,creation->name,creation);
	if (lemenu==NULL) lemenu=courant;
      }
    }
    return lemenu;
}

/* Renvoie -1 en cas d'erreur */
/* 0 s'il n'y a rien de neuf  */
/* le nombre d'articles ajoutés s'il y a pas trop de neuf */
/* -2 s'il y a beaucoup de neuf */
int cherche_newnews() {
   char *buf;
   int res,code;
   struct tm *gmt_check;
   char param[32];
   char *Message_id[25];
   int nombre,i;
   time_t actuel;

   /* On envoie un newnews au serveur */
   buf=safe_malloc((MAX_NEWSGROUP_LEN+50)*sizeof(char));
   strcpy(buf, Newsgroup_courant->name);
   gmt_check=gmtime(&Date_groupe); 
   res=strftime(param, 20, " %y%m%d %H%M%S GMT", gmt_check);
   param[res]='\0';
   
   strcat(buf,param);
   if (debug) fprintf(stderr, "Date formée : %s\n",buf);
   actuel=time(NULL);
   res=write_command(CMD_NEWNEWS, 1, &buf);
   free(buf);
   code=return_code();
   if ((code<0) || (code>400)) return -1;
   nombre=0;

   for(nombre=0; nombre<25;nombre++) {
     res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
     if (res<0) return -1;
     if (tcp_line_read[1]!='\r')
       Message_id[nombre] = safe_strdup(tcp_line_read);
     else break;
   }
   if (tcp_line_read[1]!='\r')
   {
     discard_server();
     for (i=0;i<25;i++)
       free(Message_id[i]);
     return -2;
   }
   if(nombre >0) {
      int articles_ajoutes, mettre_time;
      int retry=0;

      articles_ajoutes=0;
      mettre_time=1;
     
     /* a remplacer par une insertion du message dans la liste */
     for (i=0;i<nombre;i++) {
       if (debug) fprintf(stderr,"New news - %s\n",Message_id[i]);
       if (ajoute_message(Message_id[i],0,&retry)!=NULL) 
       		articles_ajoutes++; else
	if (retry) 
           mettre_time=0; 
       free(Message_id[i]);
     }
     if (mettre_time) Date_groupe=actuel+Date_offset;
     Aff_newsgroup_courant();
     return articles_ajoutes;
   } else 
      Date_groupe=actuel+Date_offset;
   return 0;
}

/* Renvoie le nombre estimé d'articles non lus dans group */
/* returne -1 en cas d'erreur de lecture.              */
/*         -2 si le newsgroup n'existe pas (glup !)    */
int NoArt_non_lus(Newsgroup_List *group) {
   int i, res, code, non_lus;
   Range_List *actuel;
   char *buf;

   /* On envoie un list active au serveur */
   buf=safe_malloc((MAX_NEWSGROUP_LEN+10)*sizeof(char));
   strcpy(buf, "active ");
   strcat(buf, group->name);
   res=write_command(CMD_LIST, 1, &buf);
   free(buf);
   if (res<3) return -1;

   code=return_code();
   if ((code<0) || (code>400)) return -1;

   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return -1;
   if (tcp_line_read[1]=='\r') {
      return -2; /* Pas glop, tout ca */
   }
   /* Normalement, une ligne de lecture suffit amplement */
   buf=strchr(tcp_line_read,' ');
   group->max=strtol(buf,&buf,0);
   group->min=strtol(buf,&buf,0);
   while (*buf && (isblank(*buf))) buf++;
/* TODO : améliorer ce test */
   switch (*buf) {
     case 'n' :
     case 'j' :
     case '=' :
     case 'x' : group->flags |=GROUP_READONLY_FLAG;
                break;
   }
   group->flags |= GROUP_READONLY_TESTED;
   discard_server(); /* Si il y a plusieurs newsgroups, BEURK */
   
   non_lus=group->max-group->min+1;
   if (group->max==0) { return 0; /* le groupe est vide */
   }
   actuel=group->read;
   while (actuel) { 
      for (i=0; i<RANGE_TABLE_SIZE; i++) {
	  if (actuel->max[i]<group->min) continue;
	  if (actuel->min[i]<group->min) non_lus-=actuel->max[i]-group->min+1;
	     else non_lus-=actuel->max[i]-actuel->min[i]+1;
      }
      actuel=actuel->next;
   }
   if (non_lus<0) non_lus=0;

   return non_lus;
}

void test_readonly(Newsgroup_List *groupe) {
   char *buf;
   int res,code;

   /* On envoie un list active au serveur */
   buf=safe_malloc((MAX_NEWSGROUP_LEN+10)*sizeof(char));
   strcpy(buf, "active ");
   strcat(buf, groupe->name);
   res=write_command(CMD_LIST, 1, &buf);
   free(buf);
   if (res<3) return;
   code=return_code();
   if ((code<0) || (code>400)) return;

   res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return;
   if (tcp_line_read[1]=='\r') {
      return; /* Pas glop, tout ca */
   }
   /* Normalement, une ligne de lecture suffit amplement */
   buf=strchr(tcp_line_read,' ');
   strtol(buf,&buf,0);
   strtol(buf,&buf,0);
   while (*buf && (isblank(*buf))) buf++;
/* TODO : améliorer ce test */
   switch (*buf) {
     case 'n' :
     case 'j' :
     case '=' :
     case 'x' : groupe->flags |=GROUP_READONLY_FLAG;
                break;
   }
   groupe->flags |= GROUP_READONLY_TESTED;
   discard_server(); /* Si il y a plusieurs newsgroups, BEURK */
}


void zap_newsgroup(Newsgroup_List *mygroup) {
   Newsgroup_List *pere=Newsgroup_deb;
   Range_List *range1, *range2;

   if (pere==mygroup) {
       Newsgroup_deb=mygroup->next;
       if (mygroup->next) mygroup->next->prev=NULL;
   }
   else { pere=mygroup->prev;
          pere->next=mygroup->next;
          if (mygroup->next) mygroup->next->prev=pere;
	}
   if (mygroup->description) free(mygroup->description);
   range1=mygroup->read;
   while (range1) {
      range2=range1->next;
      free(range1);
      range1=range2;
   }
   /* on les garde pour pouvoir avoir des ptr dessus */
   /* Ca fait gagner 128 octet par tag soit 64 K... */
   mygroup->description=NULL;
   mygroup->read=NULL;
   mygroup->prev=NULL;
   mygroup->next=Newsgroup_deleted;
   Newsgroup_deleted=mygroup;
   if (mygroup->next) mygroup->next->prev=mygroup;
/*   free(mygroup); */
   return;
}
   
/* c'est un code penible... Mais bon, j'ai pas choisi un design simple,
 * tant pis pour moi */
void copy_range_list(Range_List *source, int source_index, Range_List *dest,
    int dest_index)
{
  int delta=dest_index-source_index;
  int start=source_index+((delta>=0)?0:-delta);
  int i,j;

  do {
    for (i=start+((delta>=0)?0:-delta);
	i<RANGE_TABLE_SIZE-((delta>=0)?delta:0); i++)
    {
      dest->min[i+delta] = source->min[i];
      dest->max[i+delta] = source->max[i];
    }
    if (delta>0) {
      if (source->next) { source=source->next; }
      else {
        for (j=RANGE_TABLE_SIZE-delta; j<RANGE_TABLE_SIZE; j++)
	  dest->min[j]=dest->max[j]=0;
        return;
      }
      for (i=0, j=RANGE_TABLE_SIZE-delta; j<RANGE_TABLE_SIZE; i++,j++)
      {
        dest->min[j] = source->min[i];
        dest->max[j] = source->max[i];
      }
    }
    if (delta<0) {
      dest->next=safe_calloc(1,sizeof(Range_List));
      dest=dest->next;
      for (i=0,j=RANGE_TABLE_SIZE+delta;j<RANGE_TABLE_SIZE;i++,j++) {
	  dest->min[i] = source->min[j];
	  dest->max[i] = source->max[j];
      }
      if (source->next) { source=source->next; }
	else return;

    }
    if (delta==0) {
      if (source->next) { source=source->next; }
      else return;
      dest->next=safe_calloc(1,sizeof(Range_List));
      dest=dest->next;
    }
    start=0;
  } while(1);
}

void add_read_article(Newsgroup_List *mygroup, int article)
{
   Range_List *range1, *range2, *pere;
   int lu_index;
   int first=0;
   int borne=0;
   int i;
   pere=NULL;
   range1=mygroup->read;
   lu_index=0;
   while(range1) {
     for(lu_index=0; lu_index<RANGE_TABLE_SIZE; lu_index++)
     {
       if ((range1->max[lu_index]>=article) && (range1->min[lu_index]<=article))
	 return;   /* l'article est deja lu */
       if ((range1->max[lu_index]==0)|| (range1->min[lu_index]>article))
	 break;
     }
     if ((range1->max[lu_index]==0)|| (range1->min[lu_index]>article)) break;
     pere=range1;
     range1=range1->next;
   }
   if (range1) {
     if (range1->min[lu_index]==article+1) { range1->min[lu_index]=article;
      borne=1;}
     if (lu_index==0)  {
       if (pere) { range1=pere; lu_index=RANGE_TABLE_SIZE-1; }
       first=1;
     } else lu_index--;
     if (range1->max[lu_index] == article-1) {
       range1->max[lu_index]=article;
       borne++;
     }
   } else first=1;
   if (borne==1) return;
   if (borne==2) {
     range2=safe_calloc(1,sizeof(Range_List));
     for (i=0;i<=lu_index;i++) {
       range2->min[i]=range1->min[i];
       range2->max[i]=range1->max[i];
     }
     if (lu_index==RANGE_TABLE_SIZE-1)
     { range1=range1->next;
       range2->max[lu_index] = range1->max[0];
       range2->next=safe_calloc(1,sizeof(Range_List));
       range2=range2->next;
       copy_range_list(range1,1,range2,0);
     } else {
       range2->max[lu_index]=range1->max[lu_index+1];
       if (lu_index==RANGE_TABLE_SIZE-2) {
	 range1=range1->next;
         copy_range_list(range1,0,range2,lu_index+1);
       } else
         copy_range_list(range1,lu_index+2,range2,lu_index+1);
     }
     if (pere) pere->next=range2;
     else mygroup->read=range2;
     range2=range1;
     while(range2) {
       range1=range2;
       range2=range2->next;
       free(range1);
     }
     return;
   }

   range2=safe_calloc(1,sizeof(Range_List));
   if (!first) lu_index++;
   range2->min[lu_index]=article;
   range2->max[lu_index]=article;
   if (!range1) { mygroup->read=range2; return;}

   for (i=0;i<lu_index;i++) {
     range2->min[i]=range1->min[i];
     range2->max[i]=range1->max[i];
   }

   if (lu_index==RANGE_TABLE_SIZE-1)
   {
     range2->next=safe_calloc(1,sizeof(Range_List));
     range2=range2->next;
     copy_range_list(range1,lu_index,range2,0);
   } else
     copy_range_list(range1,lu_index,range2,lu_index+1);
   if (pere) pere->next=range2;
   else mygroup->read=range2;
   range2=range1;
   while(range2) {
     range1=range2;
     range2=range2->next;
     free(range1);
   }
   return;
}

/* Renvoie un pointeur sur ce qu'il faut aficher du nom du groupe */
/* flag=1 :  si l'option est NULL, afficher juste la fin */
char *truncate_group (char *str, int flag) {
  char *tmp;

  if ((Options.prefixe_groupe) && (strncmp(str,Options.prefixe_groupe,strlen(Options.prefixe_groupe))==0))
       return str+strlen(Options.prefixe_groupe);
  if (flag) return ((tmp=strrchr(str,'.')) ? tmp+1 : str);
  return str;
}
