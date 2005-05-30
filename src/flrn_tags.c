/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_tags.c : tags et historique
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

#include "flrn.h"
#include "group.h"
#include "flrn_tags.h"
#include "options.h"
#include "flrn_files.h"
#include "tty_keyboard.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

Flrn_Tag tags[MAX_TAGS];
int max_tags_ptr=-1,tags_ptr=-1;
Flrn_Special_Tag *special_tags=NULL;

Flrn_Tag *add_special_tags(struct key_entry *key, Flrn_Tag *tag) {
  Flrn_Special_Tag *parcours=special_tags;
  if (key==NULL) return NULL;
  while (parcours) {
      if (key_are_equal(key,&(parcours->key))) break;
      parcours=parcours->next;
  }
  if (parcours) {
      if (tag) memcpy(&(parcours->tag), tag, sizeof(Flrn_Tag));
      return &(parcours->tag);
  }
  parcours=safe_calloc(1,sizeof(Flrn_Special_Tag));
  if (tag) memcpy(&(parcours->tag), tag, sizeof(Flrn_Tag));
  memcpy(&(parcours->key), key, sizeof(struct key_entry));
  if (key->entry==ENTRY_ALLOCATED_STRING)
      parcours->key.value.allocstr=safe_flstrdup(key->value.allocstr);
  parcours->next=special_tags;
  special_tags=parcours;
  return &(parcours->tag);
}

Flrn_Tag *get_special_tags(struct key_entry *key) {
  Flrn_Special_Tag *parcours=special_tags;
  if (key==NULL) return NULL;
  while (parcours) {
      if (key_are_equal(key,&(parcours->key))) break;
      parcours=parcours->next;
  }
  if (parcours) return &(parcours->tag);
  return NULL;
}

void load_history (void) {
  FILE *hist_file;
#define SIZE_HIST_LINE MAX_NEWSGROUP_LEN+20
  char buf[SIZE_HIST_LINE];
  char *buf1, *buf2;
  flrn_char *trad;
  int num=0,compte=0,rc;
  Newsgroup_List *parcours;
  struct key_entry key;
  Flrn_Tag atag,*stag;
  
  if (Options.hist_file_name==NULL) return;
  hist_file=open_flrnfile(Options.hist_file_name,"r",0,NULL);
  if (hist_file==NULL) return;
  key.entry=ENTRY_ERROR_KEY;
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
	/* on enleve le '\n' */
        char *zero=buf2+strlen(buf2)-1;
	if (*zero=='\n') *zero='\0';
	switch (*(buf2+1)) {
	    case 'k' : key.entry=ENTRY_SLANG_KEY;
		       key.value.slang_key=strtol(buf2+2,NULL,10);
		       break;
	    case 's' : key.entry=ENTRY_STATIC_STRING;
		       trad=&(key.value.ststr[0]);
		       conversion_from_utf8(buf2+2,
			       &trad,4,(size_t)(-1));
		       break;
	    case 'a' : key.entry=ENTRY_ALLOCATED_STRING;
		       rc=conversion_from_utf8(buf2+2,
			       &(key.value.allocstr),0,(size_t)(-1));
		       if (rc>0) key.value.allocstr=safe_flstrdup
			            (key.value.allocstr);
		       break;
	 }
         *buf2='\0';
    }
    else num=compte;
    if (key.entry==ENTRY_ERROR_KEY) stag=&(tags[num]);
    else stag=&atag;
    stag->article_deb_key=-1;
    rc=conversion_from_utf8(buf,&trad,0,(size_t)(-1));
    parcours=Newsgroup_deb;
    while (parcours) {
       if (strcmp(parcours->name,trad)==0) break;
       parcours=parcours->next;
    }
    if (parcours==NULL) {
	if (rc==0) stag->newsgroup_name=trad; else  /* TODO : faudra trouver
						              un moyen pour
							      liberer ça */
	stag->newsgroup_name=safe_flstrdup(trad);
    }
    else {
	stag->newsgroup_name=parcours->name;
	if (rc==0) free(trad);
    }
    stag->numero=strtol(buf1,NULL,10);
    stag->article=NULL;
    if (key.entry!=ENTRY_ERROR_KEY) {
        add_special_tags(&key,stag);
	Free_key_entry(&key);
    }
    else if (num<MAX_TAGS-1)
      compte++;
  }
  tags_ptr=max_tags_ptr;
  fclose(hist_file);
} 

void save_history (void) {
  FILE *hist_file;
  int num,num_last;
  int rc;
  char *trad;
  Flrn_Special_Tag *parcours=special_tags;

  /* On se place sur des messages corrects */
  if (max_tags_ptr==-1) return;
  num_last=max_tags_ptr;
  while (tags[num_last].numero<=0) {
     num_last+=MAX_TAGS-1;
     num_last%=MAX_TAGS;
     if (num_last==max_tags_ptr) break;
  }

  /* On sauve l'historique... sans les messages extérieurs */
  if (Options.hist_file_name==NULL) return;
  hist_file=open_flrnfile(Options.hist_file_name,"w",0,NULL);
  if (hist_file==NULL) return;
  if (tags[num_last].numero>0) { 
    for (num=0;num<MAX_TAGS;num++)
      if ((tags[num].article_deb_key) && (tags[num].numero>0)) {
	  rc=conversion_to_utf8(tags[num].newsgroup_name,&trad,0,(size_t)(-1));
          fprintf(hist_file,"%s%c%ld\n",trad,
 	    			   num==num_last ? '!' : ':',
				   tags[num].numero);
	  if (rc==0) free(trad);
      }
  }
  while (parcours) {
    if ((parcours->tag.article_deb_key) && (parcours->tag.numero>0)) {
	rc=conversion_to_utf8(parcours->tag.newsgroup_name,&trad,
		0,(size_t)(-1));
	fprintf(hist_file,"%s:%ld/",trad, parcours->tag.numero);
	if (rc==0) free(trad);
	if (parcours->key.entry==ENTRY_SLANG_KEY) 
	    fprintf(hist_file,"k%d\n",parcours->key.value.slang_key);
	else if (parcours->key.entry==ENTRY_STATIC_STRING) {
	    rc=conversion_to_utf8(parcours->key.value.ststr,&trad,
		    0, (size_t)(-1));
	    fprintf(hist_file,"s%s\n",trad);
	    if (rc==0) free(trad);
	} else if (parcours->key.entry==ENTRY_ALLOCATED_STRING) {
	    rc=conversion_to_utf8(parcours->key.value.allocstr,
		    &trad,0,(size_t)(-1));
	     fprintf(hist_file,"a%s\n",trad);
	     if (rc==0) free(trad);
	} else putc('\n',hist_file);
    }
    parcours=parcours->next;
  }
  fclose(hist_file);
}

