/* flrn v 0.3                                                           */
/*              flrn_tags.c           08/11/98                          */
/*                                                                      */
/*  Gestion des tags et de l'historique.                                */
/*                                                                      */
/*                                                                      */


#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "flrn.h"
#include "group.h"
#include "flrn_tags.h"
#include "options.h"
#include "flrn_files.h"

Flrn_Tag tags[MAX_TAGS];
int max_tags_ptr=-1,tags_ptr=-1;

void load_history (void) {
  FILE *hist_file;
#define SIZE_HIST_LINE MAX_NEWSGROUP_LEN+20
  char buf[SIZE_HIST_LINE];
  char *buf1, *buf2;
  int num,compte=0;
  Newsgroup_List *parcours;
  
  if (Options.hist_file_name==NULL) return;
  hist_file=open_flrnfile(Options.hist_file_name,"r",0,NULL);
  if (hist_file==NULL) return;
  while (fgets(buf,SIZE_HIST_LINE,hist_file)) {
  /* ':' : rien  '!' : max_tags_ptr-1 (dernier message) */
    buf1=strrchr(buf,':');
    if (buf1==NULL) {
       buf1=strrchr(buf,'!');
       if (buf1==NULL) continue;
       else max_tags_ptr=compte;
    }
    *(buf1++)='\0';
    buf2=strchr(buf1,'/');
    if (buf2) {
      num=NUM_SPECIAL_TAGS+((unsigned char)(*(buf2+1)));
      *buf2='\0';
    }
    else num=compte;
    tags[num].article_deb_key=-1;
    parcours=Newsgroup_deb;
    while (parcours) {
       if (strcmp(parcours->name,buf)==0) break;
       parcours=parcours->next;
    }
    if (parcours==NULL) continue;
    tags[num].newsgroup_name=parcours->name;
    tags[num].numero=strtol(buf1,NULL,10);
    tags[num].article=NULL;
    if (num<NUM_SPECIAL_TAGS)
      compte++;
  }
  tags_ptr=max_tags_ptr;
  fclose(hist_file);
} 

void save_history (void) {
  FILE *hist_file;
  int num,num_last;

  /* On se place sur des messages corrects */
  if (max_tags_ptr==-1) return;
  num_last=max_tags_ptr;
  while (tags[num_last].numero<=0) {
     num_last+=NUM_SPECIAL_TAGS-1;
     num_last%=NUM_SPECIAL_TAGS;
     if (num_last==max_tags_ptr) break;
  }

  /* On sauve l'historique... sans les messages extérieurs */
  if (Options.hist_file_name==NULL) return;
  hist_file=open_flrnfile(Options.hist_file_name,"w",0,NULL);
  if (hist_file==NULL) return;
  if (tags[num_last].numero>0) { 
    for (num=0;num<NUM_SPECIAL_TAGS;num++)
      if ((tags[num].article_deb_key) && (tags[num].numero>0)) 
        fprintf(hist_file,"%s%c%ld\n",tags[num].newsgroup_name,
 	    			   num==num_last ? '!' : ':',
				   tags[num].numero);
  }
  for (num=NUM_SPECIAL_TAGS;num<NUM_SPECIAL_TAGS+256;num++) 
    if ((tags[num].article_deb_key) && (tags[num].numero>0))
	fprintf(hist_file,"%s:%ld/%c\n",tags[num].newsgroup_name,
				       tags[num].numero,
				       num);
  fclose(hist_file);
}

