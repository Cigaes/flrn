/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      group.c : gestion des groupes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

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
#include "art_group.h"
#include "group.h"
#include "options.h"
#include "flrn_menus.h"
#include "flrn_tcp.h"
#include "flrn_files.h"
#include "tty_display.h"
#include "flrn_filter.h"

static UNUSED char rcsid[]="$Id$";

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
   flnews_file = open_flrnfile(name,"r",Options.default_flnewsfile ? 0 : 1,&Last_check);

   if (flnews_file==NULL) {
     if (Options.default_flnewsfile) {
        flnews_file=open_flrnfile(Options.default_flnewsfile,"r",0,NULL);
	if (flnews_file) Last_check=time(NULL); else return;
     } else return;
   }
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
	      deb=buf;
	      sleep(1);
	      continue; }
      /* ce qui gène dans un nom de groupe : on met "\t,", ca devrait suffire */
      if (strpbrk(ptr,"\t,")) {
          fprintf(stderr,"Nom du newsgroup %s invalide dans le .flnewsrc !!! \n",ptr);
	  sleep(1);
	  deb=buf;
	  continue;
      }
      creation = add_group(&Newsgroup_courant);
      strncpy(creation->name, ptr, strlen(ptr)-1);
      creation->flags=(ptr[strlen(ptr)-1]==':' ? 0 : GROUP_UNSUBSCRIBED);
      if (in_main_list(creation->name)) 
        creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
      creation->name[strlen(ptr)-1]='\0';
      creation->min=1;
      creation->max=0;
      creation->not_read=-1;
      creation->virtual_in_not_read=0;
      creation->read=init_range(deb,&(creation->max));
      deb=buf;
   }
   if (Newsgroup_courant) Newsgroup_courant->next=NULL; else
     /* le fichier était vide, on refait un list normal */
     Last_check=0;
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
    if (buf) *(buf++)='\0'; else {
       /* Le serveur ne respecte pas la RFC. Tant pis */
       buf=strchr(la_ligne,'\r');
       if (buf) *buf='\0';
       buf=NULL;
    }


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
    if (buf) {
      creation->max = strtol(buf, &buf, 10); 
      creation->min = strtol(buf, &buf, 10); 
    } else {
      creation->max=0;
      creation->min=1;
    }
    creation->not_read=-1;
    creation->virtual_in_not_read=0;
    creation->read= NULL;
    creation->description= NULL;
    creation->flags=GROUP_UNSUBSCRIBED | GROUP_NEW_GROUP_FLAG;
    if (in_main_list(creation->name)) 
      creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
    if (buf) {
      while (*buf && (isblank(*buf))) buf++;
      /* TODO : améliorer ce test */
      switch (*buf) {
        case 'n' :
        case 'j' :
        case '=' :
        case 'x' : creation->flags |=GROUP_READONLY_FLAG;
      		   break;
      }
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
   int good, bad, wait_for_key=0;
   regex_t goodreg, badreg;
   time_t actuel;

  /* On forme la date en GMT */
   if (Last_check!=0) {
      actuel=Last_check+Date_offset;
      gmt_check=gmtime(&actuel); 
      Last_check=time(NULL);
      res=strftime(param, 20, "%y%m%d %H%M%S GMT", gmt_check);
      param[res]='\0';
      if (debug) fprintf(stderr, "Date formée : %s\n",param);
   
      buf=param; 
      res=write_command(CMD_NEWGROUPS, 1, &buf);
   } else {
      res=write_command(CMD_LIST, 0, NULL);
      wait_for_key=-1;
   }
      
   if (res<0) return;
   code=return_code();
   
   if (code>400) {
      if (debug) fprintf(stderr,"Code d'erreur %d\n",code);
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
      if (res<4) break; /* a priori, on a lu ".\r\n" */
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

      if (Options.auto_kill) {
	add_to_main_list(creation->name);
	creation->flags|=GROUP_IN_MAIN_LIST_FLAG;
      }
      if ((opt_c) || (Options.warn_if_new && (wait_for_key!=-1))) {
          fprintf(stdout, "Nouveau groupe : %s ", creation->name); 
	  if (opt_c==0) {
	     if (creation->flags & GROUP_UNSUBSCRIBED) 
	        fputs("Non abonné",stdout);
	     else
	     if (creation->flags & GROUP_IN_MAIN_LIST_FLAG) 
	        fputs("Abonné, dans le kill-file",stdout);
	     else fputs("Abonné",stdout);
	     wait_for_key=1;
	  }
	  putc('\n',stdout);
      }
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
   } while (1);
   if (Options.auto_subscribe) regfree(&goodreg);
   if (Options.auto_ignore) regfree(&badreg);
   if (wait_for_key==1) {
      fputs("Appuyez sur entree pour continuer...\n",stdout);
      getc(stdin); /* A priori, ça pose pas de problèmes */
   }
}

/* Sauvegarde du .flnewsrc */
/* Il s'agit d'une séparation de free_groups. Sinon on ne s'en sort */
/* plus...						            */
/* retour : 0 : succès -1 : échec 				    */
int save_groups() {
   FILE *flnews_file=NULL;
   Newsgroup_List *tmpgroup=NULL, *courant;
   Range_List *msg_lus;
   int lu_index;
   int first=1;
   char name[MAX_PATH_LEN];
   int name_written, failed=0;
   
   if (debug) fprintf(stderr, "Sauvegarde du .flnewsrc\n");
   strcpy(name,DEFAULT_FLNEWS_FILE);
   if (Options.flnews_ext) strcat(name,Options.flnews_ext);
   strcat(name,".new");
   flnews_file = open_flrnfile(name,"w+",1,NULL);
   if (flnews_file==NULL) return -1;
   for (courant=Newsgroup_deb;courant;courant=tmpgroup) {
     tmpgroup=courant->next;
     /* on construit la ligne du .flnewsrc */
     /* Je propose de ne pas marquer le groupe quand on n'est non abonné */
     /* et qu'on a rien lu...						 */
     msg_lus=courant->read;
     first=1;
     if (!(courant->flags & GROUP_UNSUBSCRIBED)) {
       failed|=(fprintf(flnews_file,"%s: ",courant->name)<0);
       name_written=1;
     } else name_written=0;
     while (msg_lus)
     {
         for (lu_index=0;lu_index<RANGE_TABLE_SIZE; lu_index++)
	 {
	   if (msg_lus->min[lu_index]==0) continue;
	   if (!name_written) {
	     failed|=(fprintf(flnews_file,"%s! ",courant->name)<0);
	     name_written=1;
	   }
	   if (!first) failed|=(putc(',',flnews_file)==EOF);
	   first=0;
	   failed|=(fprintf(flnews_file,"%d",msg_lus->min[lu_index])<0);
	   if (msg_lus->min[lu_index]<msg_lus->max[lu_index])
	      failed|=(fprintf(flnews_file,"-%d",msg_lus->max[lu_index])<0);
	 }
	 msg_lus=msg_lus->next;
     }
     if (name_written) failed|=(putc('\n',flnews_file)==EOF);
   }
   if (!failed) {
     if (fclose(flnews_file)!=EOF) {
       if (debug) fprintf(stderr,"Ecriture du .flnewsrc.new finie.\n");
       rename_flnewsfile(name,NULL);
       if (debug) fprintf(stderr,"Renommage du .flnewsrc fini.\n");
     } else failed=1;
   } else fclose(flnews_file);
   if (debug) fprintf(stderr,"%s","C'est fini\n");
   return -failed;
}
   

/* Libération de la liste des groupes */
void free_groups(int save_flnewsrc) {
   Newsgroup_List *tmpgroup=NULL;
   Range_List *msg_lus,*tmprange;
   int write_flnewsrc;
   
   if (save_flnewsrc) {
      write_flnewsrc=save_groups();
      if (write_flnewsrc) {
        fprintf(stderr,"Erreur dans la sauvegarde du .flnewsrc\n");
	fprintf(stderr,"On garde l'ancien .flnewsrc.\n");
      }
   }
   for (Newsgroup_courant=Newsgroup_deb;Newsgroup_courant;
       Newsgroup_courant=tmpgroup) {
     tmpgroup=Newsgroup_courant->next;
     if (Newsgroup_courant->description) free(Newsgroup_courant->description);
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
       Hash_table=Newsgroup_courant->Hash_table;
       libere_liste();
       Newsgroup_courant->Article_deb=NULL;
       Newsgroup_courant->Article_exte_deb=NULL;
       if (Newsgroup_courant->Hash_table)
	 free(Newsgroup_courant->Hash_table);
       Newsgroup_courant->Hash_table=NULL;
       Newsgroup_courant->article_deb_key=0;
     }
     free(Newsgroup_courant);
   }
   /* C'est tout */
}

/* Demande au serveur l'existence d'un newsgroup         */
/* L'ajoute à la liste des newsgroup existants (en fin)  */
/* name contient deja s'il y a lieu les *... */
/* flag=1 : avec prefixe_groupe */
/* name doit etre non NULL */
Newsgroup_List *cherche_newsgroup_re (char *name, regex_t reg, int flag)
{
    int res, code, order, best_order=-1;
    char *buf,*buf2,*buf3=NULL;

    buf=safe_malloc((MAX_NEWSGROUP_LEN+12)*sizeof(char));
    strcpy(buf, "active ");
    if (flag) strcat(buf, Options.prefixe_groupe);
    strcat(buf, name);
    res=write_command(CMD_LIST, 1, &buf);
    free(buf);
    if (res<1) return NULL;
    code=return_code();
    if ((code<0) || (code>400)) return NULL;
    while((res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1))>=0)
    {
      if (res<4) break; /* Existe pas... */
      buf2=strchr(tcp_line_read,' ');
      if (buf2==NULL) continue;
      *buf2='\0';
      buf=tcp_line_read+(flag ? strlen(Options.prefixe_groupe) : 0);
      if ((order=calcul_order_re(buf,&reg))!=-1) {
	*buf2=' ';
        if ((best_order==-1) || (best_order>order)) {
	  buf3=safe_strdup(tcp_line_read);
	  best_order=order;
	}
      }
    }
    if (buf3) {
       strcpy(tcp_line_read,buf3); free(buf3);
       return un_nouveau_newsgroup(tcp_line_read);
    }
    return NULL;
}

/* name doit être non NULL */
Newsgroup_List *cherche_newsgroup (char *name, int exact, int flag) {
    int res, code, order, best_order=-1;
    char *buf, *buf2, *buf3=NULL;

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
    while((res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1))>=0)
    {
      if (res<4) break; /* Existe pas... */
      buf2=strchr(tcp_line_read,' ');
      if (buf2==NULL) continue;
      *buf2='\0';
      buf=tcp_line_read+(flag ? strlen(Options.prefixe_groupe) : 0);
      if ((order=calcul_order(buf,name))!=-1) {
	*buf2=' ';
        if ((best_order==-1) || (best_order>order)) {
	  buf3=safe_strdup(tcp_line_read);
	  best_order=order;
	}
      }
    }
    if (buf3) {
       strcpy(tcp_line_read,buf3); free(buf3);
       return un_nouveau_newsgroup(tcp_line_read);
    }
    return NULL;
}

/*  La fonction est semblable à cherche_newsgroup_re, mais elle créée un */
/* menu au lieu de renvoyer le premier newsgroup obtenu */
/* si avec_reg & 2, on utilise options.prefixe_groupe */
/* name peut être NULL */
Liste_Menu *menu_newsgroup_re (char *name, regex_t reg, int avec_reg)
{
    Liste_Menu *lemenu=NULL, *courant=NULL;
    int res, code, order;
    Newsgroup_List *creation;
    char *buf;
    buf=safe_malloc((MAX_NEWSGROUP_LEN+12)*sizeof(char));
    strcpy(buf, "active ");
    if (avec_reg & 2) strcat(buf,Options.prefixe_groupe);
    if (!(avec_reg & 1)) strcat(buf,"*");
    if (name) {
      strcat(buf, name);
      if (!(avec_reg & 1)) strcat(buf,"*");
    }
    res=write_command(CMD_LIST, 1, &buf);
    free(buf);
    if (res<1) return NULL;
    code=return_code();
    if ((code<0) || (code>400)) return NULL;
    while((res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1))>=0)
    {
      if (res<4) return lemenu; /* On peut repartir */
      buf=strchr(tcp_line_read,' ');
      if (buf==NULL) continue;
      *buf='\0';
      order=((avec_reg & 1) ? calcul_order_re(tcp_line_read+(avec_reg & 2 ? strlen(Options.prefixe_groupe):0),&reg) : calcul_order(tcp_line_read+(avec_reg & 2 ? strlen(Options.prefixe_groupe):0),name));
      if (order!=-1) {
	*buf=' ';
	creation=un_nouveau_newsgroup(tcp_line_read);
	courant=ajoute_menu_ordre(&lemenu,creation->name,creation,-1,order);
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
       if (ajoute_message(Message_id[i],&retry)!=NULL) 
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
int NoArt_non_lus(Newsgroup_List *group, int force_check) {
   int i, res, code, non_lus, max, min;
   Range_List *actuel;
   char *buf;

   if ((group->not_read!=-1) && (!force_check)) return group->not_read;
   /* On envoie un list active au serveur */
   buf=safe_malloc((MAX_NEWSGROUP_LEN+10)*sizeof(char));
   strcpy(buf, "active ");
   strcat(buf, group->name);
   res=write_command(CMD_LIST, 1, &buf);
   free(buf);
   if (res<3) return -1;

   code=return_code();
   if ((code<0) || (code>400)) return -1;

   res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return -1;
   if (tcp_line_read[1]=='\r') {
      return -2; /* Pas glop, tout ca */
   }
   /* Normalement, une ligne de lecture suffit amplement */
   buf=strchr(tcp_line_read,' ');
   /* SURTOUT ne pas changer group->max et group->min si le groupe est déjà */
   /* connu : ça fait planter les changement de groupes qui suivent...	    */
   max=strtol(buf,&buf,10);
   min=strtol(buf,&buf,10);
   if (group->Article_deb==NULL) {
     group->max=max;
     group->min=min;
   }
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
   
   non_lus=max-min+1;
   if (group->max==0) { return 0; /* le groupe est vide */
   }
   actuel=group->read;
   while (actuel) { 
      for (i=0; i<RANGE_TABLE_SIZE; i++) {
	  if (actuel->max[i]<min) continue;
	  if (actuel->min[i]<min) non_lus-=actuel->max[i]-min+1;
	     else non_lus-=actuel->max[i]-actuel->min[i]+1;
      }
      actuel=actuel->next;
   }
   if (non_lus<0) non_lus=0;
   if (group->not_read<=non_lus) 
        group->virtual_in_not_read=non_lus-(group->not_read > 0 ? group->not_read : 0);
	else group->virtual_in_not_read=0;
   group->not_read=non_lus;

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

   res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return;
   if (tcp_line_read[1]=='\r') {
      return; /* Pas glop, tout ca */
   }
   /* Normalement, une ligne de lecture suffit amplement */
   buf=strchr(tcp_line_read,' ');
   strtol(buf,&buf,10);
   strtol(buf,&buf,10);
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
  int start=source_index+((delta>=0)?0:delta);
  int i,j;

  if (delta<0) {
    for (i=start-delta; i<RANGE_TABLE_SIZE; i++) {
      dest->min[i+delta] = source->min[i];
      dest->max[i+delta] = source->max[i];
    }  
    if (source->next) source=source->next;
    else {
      for (j=RANGE_TABLE_SIZE+delta; j<RANGE_TABLE_SIZE; j++)
         dest->min[j]=dest->max[j]=0;
      return;
    }
    delta=RANGE_TABLE_SIZE+delta;
    start=0;
  }
  /* A partir de maintenant, delta>=0 */
  do {
    for (i=start;
	i<RANGE_TABLE_SIZE-delta; i++)
    {
      dest->min[i+delta] = source->min[i];
      dest->max[i+delta] = source->max[i];
    }
    if (delta>0) {
      if (source->max[RANGE_TABLE_SIZE-delta]==0) return;
      dest->next=safe_calloc(1,sizeof(Range_List));
      dest=dest->next;
      for (j=0,i=RANGE_TABLE_SIZE-delta;i<RANGE_TABLE_SIZE;j++,i++) {
	  dest->min[j] = source->min[i];
	  dest->max[j] = source->max[i];
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
   int borne=0,virtuel=1;
   int i;
   pere=NULL;
   range1=mygroup->read;
   lu_index=0;

   /* On commence éventuellement par mettre le flag read à l'article */
   if (mygroup->Article_deb) {
      Article_List *parcours=mygroup->Article_deb;
      while (parcours && (parcours->numero<article)) parcours=parcours->next;
      if (parcours && (parcours->numero==article)) {
         parcours->flag|=FLAG_READ;
	 if (parcours->flag & FLAG_IMPORTANT) {
	   mygroup->important--;
	   parcours->flag &= ~FLAG_IMPORTANT;
	 }
	 virtuel=0;
      }
   }
   
   while(range1) {
     for(lu_index=0; lu_index<RANGE_TABLE_SIZE; lu_index++)
     {
       if ((range1->max[lu_index]>=article) && (range1->min[lu_index]<=article))
	 return;   /* l'article est deja lu */
       if ((range1->max[lu_index]==0)|| (range1->min[lu_index]>article))
	 break;
     }
     if (lu_index!=RANGE_TABLE_SIZE) break;
     lu_index=0; /* important si range1 devient NULL ensuite */
     pere=range1;
     range1=range1->next;
   }
   if (mygroup->not_read>0) {
      if ((virtuel) && (mygroup->virtual_in_not_read>0)) {
         mygroup->not_read--;
	 mygroup->virtual_in_not_read--;
      } else if (!virtuel) mygroup->not_read--;
   }
   if (range1) {
     if (range1->min[lu_index]==article+1) { range1->min[lu_index]=article;
      borne=1;}
     if (lu_index==0)  {
       if (pere) { range1=pere; lu_index=RANGE_TABLE_SIZE-1; }
       else first=1;
     } else lu_index--;
     if ((!first) && (range1->max[lu_index] == article-1)) {
       range1->max[lu_index]=article;
       borne++;
     }
   } else first=1;
   if (borne==1) return; /* On a juste changé un numéro */
   if (borne==2) { /* Fusion de deux ranges */
   		   /* Je ne comprend pas : pourquoi faire des allocations */
     range2=safe_calloc(1,sizeof(Range_List));
     for (i=0;i<=lu_index;i++) {
       range2->min[i]=range1->min[i];
       range2->max[i]=range1->max[i];
     }
     if (lu_index==RANGE_TABLE_SIZE-1)
     { range1=range1->next; /* qui existe car first==0 */
       range2->max[lu_index] = range1->max[0];
       range2->next=safe_calloc(1,sizeof(Range_List));
       /* Mais il FAUT conserver range2, donc pas faire range2=range2->next */
       copy_range_list(range1,1,range2->next,0);
       /* On restaure range1 pour pouvoir le libérer */
       if (pere) range1=pere->next; else range1=mygroup->read;
     } else {
       range2->max[lu_index]=range1->max[lu_index+1];
       if (lu_index==RANGE_TABLE_SIZE-2) {
	 range1=range1->next;
         if (range1) copy_range_list(range1,0,range2,lu_index+1);
         /* On restaure range1 pour pouvoir le libérer */
         if (pere) range1=pere->next; else range1=mygroup->read;
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

   /* Maintenant, borne=0, on doit insérer un élément dans la liste */
   range2=safe_calloc(1,sizeof(Range_List));
   if (!first) {
     if (lu_index!=RANGE_TABLE_SIZE-1) lu_index++; else {
       range1=range1->next;
       lu_index=0;
     }
   }
   range2->min[lu_index]=article;
   range2->max[lu_index]=article;
   if (!range1) { 
      if (pere) pere->next=range2; else mygroup->read=range2; 
      return;
   }
   for (i=0;i<lu_index;i++) {
     range2->min[i]=range1->min[i];
     range2->max[i]=range1->max[i];
   }

   if (lu_index==RANGE_TABLE_SIZE-1)
   {
     range2->next=safe_calloc(1,sizeof(Range_List));
     copy_range_list(range1,lu_index,range2->next,0);
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


/* Cette fonction marque lu tous les articles d'un groupe, si ce n'est pas */
/* le groupe courant...							   */
/* je me base sur NoArtNonLus... C'est méchant, mais au moins c'est zappé  */
/* jusqu'au bout...							   */
void zap_group_non_courant (Newsgroup_List *group) {
   int i, res, code;
   Range_List *range1, *range2;
   char *buf;
   Article_List *art;
   Thread_List *thr;

   if (group==Newsgroup_courant) return;
   /* On envoie un list active au serveur */
   buf=safe_malloc((MAX_NEWSGROUP_LEN+10)*sizeof(char));
   strcpy(buf, "active ");
   strcat(buf, group->name);
   res=write_command(CMD_LIST, 1, &buf);
   free(buf);
   if (res<3) return;

   code=return_code();
   if ((code<0) || (code>400)) return;

   res=read_server_for_list(tcp_line_read, 1, MAX_READ_SIZE-1);
   if (res<0) return;
   if (tcp_line_read[1]=='\r') {
      return; /* Pas glop, tout ca */
   }
   /* Normalement, une ligne de lecture suffit amplement */
   buf=strchr(tcp_line_read,' ');
   group->max=strtol(buf,&buf,10);
   group->min=strtol(buf,&buf,10);
   discard_server(); /* Si il y a plusieurs newsgroups, BEURK */
   
   if (group->max==0)  return; /* le groupe est vide */
   if (group->read==NULL) {
     group->read=safe_malloc(sizeof(Range_List));
     group->read->next=NULL;
   }
   group->read->min[0]=1;
   group->read->max[0]=group->max;
   for (i=1; i<RANGE_TABLE_SIZE; i++) group->read->max[i]=group->read->min[i]=0;

   range1=group->read->next;
   while (range1) { 
      range2=range1->next;
      free(range1);
      range1=range2;
   }
   group->read->next=NULL;
   art=group->Article_deb;
   while (art) {
     art->flag|=FLAG_READ;
     art->flag&= ~FLAG_IMPORTANT;
     art=art->next;
   }
   thr=group->Thread_deb;
   while (thr) {
     thr->non_lu=0;
     thr=thr->next_thread;
   }
   /* si de nouveaux messages étaient apparus, on ne les a pas marqué lus */
   /* Tant pis... Ça devrait pas changer beaucoup de choses... qui irait  */
   /* zapper un groupe après coup...					  */
   group->not_read=0;
   group->important=0;
}
   

/* Action de Menu_simple pour le goto */
void get_group_description (Newsgroup_List *legroupe) {
  char *buf[2],*ptr;
  int res, code;
  buf[0]="newsgroups";
  buf[1]=legroupe->name;
  res=write_command (CMD_LIST, 2, buf);
  if (res!=-2) {
      if (res<1) return;
      code=return_code();
      if ((code<0) || (code>400)) return;
      res=read_server(tcp_line_read, 1, MAX_READ_SIZE-1);
      if (res<1) return;
      if (tcp_line_read[0]!='.') {
          ptr=tcp_line_read+strlen(legroupe->name);
          legroupe->description=safe_malloc((strlen(ptr)-1)*sizeof(char));
          strncpy(legroupe->description,ptr, strlen(ptr)-2);
          legroupe->description[strlen(ptr)-2]='\0';
          discard_server();
      } else legroupe->description=safe_strdup("  (pas de description)  ");
  } else
     legroupe->description=safe_strdup("  (pas de description)  ");
}


void Ligne_carac_du_groupe (void *letruc, char *lachaine,
                                int taille)
{
  Newsgroup_List *legroupe=letruc;
  lachaine[0]='\0';
  if (legroupe->description==NULL) get_group_description(legroupe);
  if (legroupe->description) strncat(lachaine,legroupe->description,taille-1);
  lachaine[taille-1]='\0';
}

int calcul_order(char *nom_gr, char *str) {
    char *buf, *buf2;
    if (str==NULL) return 0;
    if ((buf=strstr(nom_gr,str))==NULL) return -1;
    while ((*buf!='\0') && ((buf2=strstr(buf+1,str))!=NULL)) buf=buf2;
    return (strlen(nom_gr)-strlen(str))*10-9*(buf-nom_gr);
}

int calcul_order_re(char *nom_gr, regex_t *sreg) {
    int ret, bon=0, orderact, order=-1;
    char *buf=nom_gr;
    regmatch_t pmatch[1];

    ret=regexec(sreg, nom_gr, 1, pmatch, 0);
    if (ret!=0) return -1;
    do {
      buf+=pmatch[0].rm_eo;
      if (pmatch[0].rm_eo==0) buf++; else {
         bon=1;
	 orderact=strlen(buf)*10+pmatch[0].rm_so+(buf-nom_gr)-pmatch[0].rm_eo;
	 if ((order==-1) || (orderact<order)) order=orderact;
      }
      ret=regexec(sreg, buf, 1, pmatch, REG_NOTBOL);
    } while ((ret==0) && (*buf!='\0'));
    if (bon) return order; else return -1;
}
