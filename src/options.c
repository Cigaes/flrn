/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      options.c : gestion des options
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

#include "flrn.h"
#define IN_OPTION_C
#include "flrn_files.h"
#include "flrn_comp.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_inter.h"
#include "flrn_menus.h"
#include "flrn_pager.h"
#include "flrn_color.h"
#include "flrn_slang.h"
#include "flrn_command.h"
#include "flrn_format.h"
#include "options.h"
#include "site_config.h"
#include "slang_flrn.h"
#include "version.h"
#include "tty_keyboard.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

static flrn_char *delim = fl_static("=: \t\n");

int size_header_list=ISIZE_HEADER_LIST;
Known_Headers *unknown_Headers;
void increase_size_header_list(int);

static void free_string_list_type (string_list_type *s_l_t);
static int parse_option_file (char *name, int flags, int flag_option);

static flrn_char *option_ligne=NULL;
static int deep_inclusion=0;

int var_comp(flrn_char *var, size_t len, Liste_Chaine *debut)
{
  Liste_Chaine *courant=debut, *nouveau;
  flrn_char *buf=var;
  int used;
  char *guess=NULL;
  int prefix_len=0;
  int match=0;
  int i,j;
  if (fl_strncmp(buf,"no",2)==0) {
     buf +=2;
     fl_strcat(debut->une_chaine,fl_static("no"));
  }
  used=fl_strlen(buf);
  if (used==0) return 0;
  for (i=0; i< NUM_OPTIONS; i++)
    if(fl_strncmp(buf, fl_static_tran(All_options[i].name),used)==0)
      if (!All_options[i].flags.lock) {
	if (match) {
	  if (!prefix_len) prefix_len=fl_strlen(guess);
	  for (j=0;j<prefix_len;j++)
	    if (guess[j] != All_options[i].name[j]){
	      prefix_len=j; break;
	    }
	}
	guess=All_options[i].name;
	match++;
	nouveau=safe_calloc(1,sizeof(Liste_Chaine));
	nouveau->une_chaine=safe_malloc((len+1)*sizeof(flrn_char));
	fl_strcpy(nouveau->une_chaine,debut->une_chaine);
	fl_strcat(nouveau->une_chaine,fl_static_tran(guess));
        fl_strcat(nouveau->une_chaine,fl_static(" "));
	nouveau->complet=1;
	nouveau->suivant=courant->suivant;
	courant->suivant=nouveau;
	courant=nouveau;
      }
  if (match==1) {
    free(debut->une_chaine);
    debut->une_chaine=courant->une_chaine;
    debut->suivant=courant->suivant;
    var[0]=fl_static('\0');
    free(courant);
    return 0;
  } else if (match >1) {
    debut->complet=0;
    used=fl_strlen(debut->une_chaine);
    fl_strncat(debut->une_chaine,fl_static_tran(guess),prefix_len);
    debut->une_chaine[used+prefix_len]=fl_static('\0');
    var[0]=fl_static('\0');
    return -1;
  }
  debut->complet=-1;
  return -2;
}

static flrn_char *get_optcmd_name(void * ptr,int num)
{
    static flrn_char result[30];
    char *bla;
    bla=((struct _Optcmd *)ptr)[num].name;
    strcpy(result,fl_static_tran(bla));
    return result;
}
/* on fait maintenant les checks sur len...
 * on pourait ajouter pour la completude la completion apres color
 * mais ca veut dire reecrire le code qui gere ca... */

int options_comp(char *str, size_t len, Liste_Chaine *debut)
{
  int res,res2,bon,i;
  Liste_Chaine *courant, *pere, *suivant;
  int result[NUMBER_OF_OPT_CMD];
  char *suite;
  for (i=0;i<NUMBER_OF_OPT_CMD;i++) result[i]=0;
  res = Comp_generic(debut, str,len,(void *)Optcmd_liste,NUMBER_OF_OPT_CMD,
      get_optcmd_name,delim,result,1);
  if (res==-3) return 0;
  if (res >= 0) {
    if (Optcmd_liste[res].comp) {
      return (*Optcmd_liste[res].comp)(str,len,debut);
    } else {
      if (str[0]) debut->complet=0;
      strcat(debut->une_chaine,str);
      return 0;
    }
  }
  if (res==-1) {
    bon=0;
    pere=debut;
    courant=debut->suivant;
    suivant=courant->suivant;
    for (i=0;i<NUMBER_OF_OPT_CMD;i++) {
       if (result[i]==0) continue;
       res2=0;
       if (Flcmds[i].comp) {
         suite=safe_strdup(str);
         res2=(*Optcmd_liste[i].comp)(suite,len,courant);
         free(suite);
         if (res2<-1) {
            free(courant->une_chaine);
            pere->suivant=courant->suivant;
            free(courant);
            courant=pere->suivant;
	    if (courant) suivant=courant->suivant;
            continue;
         }
       } else {
         strcat(courant->une_chaine,str);
	 if (str[0]) courant->complet=0;
       }
       if (res==-1) bon+=2; else bon++;
       pere=courant;
       while (pere->suivant!=suivant) pere=pere->suivant;
       courant=suivant;
       if (courant) suivant=courant->suivant;
    }
    if (bon>1) return -1; else if (bon) {
       free(debut->une_chaine);
       courant=debut->suivant;
       debut->suivant=courant->suivant;
       debut->une_chaine=courant->une_chaine;
       free(courant); 
       return 0;
    }
  }
  return -2;
}

static flrn_char *bindarg_names[] = {
  fl_static("add"),
  fl_static("menu"),
  fl_static("pager"),
  fl_static("command")
};

static char *get_name_liste(void *p,int i) {
  return ((char **)p)[i];
}

int bind_comp(flrn_char *str, size_t len, Liste_Chaine *debut)
{
  int res,res2,i,bon,offset;
  flrn_char *suite;
  Liste_Chaine *courant, *pere, *suivant;
  int result[sizeof(bindarg_names)/sizeof(bindarg_names[0])];
  for (i=0;i<sizeof(bindarg_names)/sizeof(bindarg_names[0]);i++)
    result[i]=0;
  if (len-fl_strlen(debut->une_chaine)<2) return -2;
  if ((str[1]==fl_static(' '))||(str[1]==fl_static('\0'))) {
    debut->une_chaine[strlen(debut->une_chaine)+2]=fl_static('\0');
    fl_strncat(debut->une_chaine,str,2);
    if (str[1]==fl_static(' ')) 
	return Comp_cmd_explicite(str+2,len,debut); else {
       debut->complet=0;
       return 0;
    }
  }
  if (str[0]==fl_static('\\')) {
    offset=fl_strcspn(str,delim);
    if (offset< len) {
      offset += fl_strspn(str+offset,delim);
      str[offset-1]=fl_static('\0');
      fl_strcat(debut->une_chaine,str);
      fl_strcat(debut->une_chaine,fl_static(" "));
      return Comp_cmd_explicite(str+offset,len,debut);
    }
    fl_strcat(debut->une_chaine,str);
    fl_strcat(debut->une_chaine,fl_static(" "));
    str[0]=fl_static('\0');
    return 0;
  }
  res = Comp_generic(debut,str,len,(void *)bindarg_names,
      sizeof(bindarg_names)/sizeof(bindarg_names[0]),
      get_name_liste,delim,result,1);
  if (res==-3) return 0;
  if (res>=0) {
    return bind_comp(str,len,debut);
  }
  if (res==-1) {
    bon=0;
    pere=debut;
    courant=debut->suivant;
    suivant=courant->suivant;
    for (i=0;i<sizeof(bindarg_names)/sizeof(bindarg_names[0]);i++) {
       if (result[i]==0) continue;
       suite=safe_flstrdup(str);
       res2=bind_comp(suite,len,courant);
       free(suite);
       if (res2<-1) {
          free(courant->une_chaine);
          pere->suivant=courant->suivant;
          free(courant);
          courant=pere->suivant;
	  if (courant) suivant=courant->suivant;
          continue;
       }
       if (res==-1) bon+=2; else bon++;
       pere=courant;
       while (pere->suivant!=suivant) pere=pere->suivant;
       courant=suivant;
       if (courant) suivant=courant->suivant;
    }
    if (bon>1) return -1; else if (bon) {
       free(debut->une_chaine);
       courant=debut->suivant;
       debut->suivant=courant->suivant;
       debut->une_chaine=courant->une_chaine;
       free(courant); 
       return 0;
    }
  }
  return -2;
}

/* ajout d'un header, éventuellement a unknown_header */
int Le_header(flrn_char *buf) {
  int i,l;
  flrn_char *buf2;

  if ((i=fl_strtol(buf,NULL,10))>0) return i-1;
  if (fl_strcasecmp(buf,fl_static("others"))==0) return NB_KNOWN_HEADERS;
  buf2=fl_strpbrk(buf,delim);
  if (buf2==NULL) l=fl_strlen(buf); else l=buf2-buf;
  for (i=0;i<NB_KNOWN_HEADERS;i++) {
    if ((l==Headers[i].header_len-1) && (fl_strncasecmp(buf,
		    fl_static_tran(Headers[i].header),l)==0))
      return i;
  }
  for (i=0;i<size_header_list;i++) {
    if ((l==unknown_Headers[i].header_len-1) && 
	    (fl_strncasecmp(buf,
	fl_static_tran(unknown_Headers[i].header),l)==0))
      return -i-2;
    if (unknown_Headers[i].header_len==0) break;
  }
  if (i==size_header_list) 
      increase_size_header_list(i+10);
  unknown_Headers[i].header=safe_malloc(l+2);
  /* unknown headers est une liste de chaînes en us-ascii... un
   * header n'est pas censé contenir d'accents */
  /* Note : on en tirera les conséquences quand il s'agira de les utiliser */
  strncpy(unknown_Headers[i].header,fl_static_rev(buf),l);
  unknown_Headers[i].header[l]=':';
  unknown_Headers[i].header[l+1]='\0';
  unknown_Headers[i].header_len=l+1;
  return -i-2;
}

int opt_do_include(flrn_char *buf, int flag)
{
  flrn_char *buf2;
  int found;

  if (buf == NULL) return -1;
  if (*buf==fl_static('"')) {
    buf++;
    buf2=fl_strchr(buf,fl_static('"')); 
    if (buf2) *buf2=fl_static('\0');  /* Pour enlever le '"' en fin */
  }
  if (*buf) {
    deep_inclusion++;
    if (deep_inclusion<10) {
      char *traduction;
      int resconv;
      resconv=conversion_to_file(buf,&traduction,0,(size_t)(-1));
      if (resconv>=0) {
        found=parse_option_file(traduction,0,flag);
	/* Attention, on change ici option_ligne */
	if (resconv==0) free(traduction);
      } else found=-1;
    }
    else found=-1;
    deep_inclusion--;
    if ((found==-1) && (!flag)) {
      char *traduction;
      int resconv;
      resconv=conversion_to_terminal(buf,&traduction,0,(size_t)(-1));
      fprintf(stderr,"Erreur : include %s impossible !\n",traduction);
      if (deep_inclusion>=10) fprintf(stderr,"Trop d'inclusions imbriquées.\n");
      if (resconv==0) free(traduction);
      sleep(1);
    }
    return found;
  }
  return 0;
}

#ifdef USE_SLANG_LANGUAGE
int opt_do_slang_parse(flrn_char *str, int flag)
{
   char *traduction;
   int resconv;
   resconv=conversion_to_file(str,&traduction,0,(size_t)(-1));
   if (source_SLang_file(traduction)<0) {
      if (!flag) {
        if (resconv==0) free(traduction);
        resconv=conversion_to_terminal(str,&traduction,0,(size_t)(-1));
        fprintf(stderr,"erreur du parse SLang de %s\n", traduction);
        sleep(1); 
      }
      if (resconv==0) free(traduction);
      return -1;
   }
   if (resconv==0) free(traduction);
   return 0;
}
#endif
  
/* 0 : OK   -1 : erreur, error a libérer */
int test_charset_modified(int i, flrn_char **error) {
        int ret_charset=0;
	/* cas des charsets */
	if (All_options[i].value.string==&(Options.terminal_charset)) {
	   ret_charset=Parse_termcharset_line(Options.terminal_charset);
	}
	if (All_options[i].value.string==&(Options.files_charset)) {
	   ret_charset=Parse_filescharset_line(Options.files_charset);
	}
	if (All_options[i].value.string==&(Options.post_charsets)) {
	   ret_charset=Parse_postcharsets_line(Options.post_charsets);
	}
	if (All_options[i].value.string==&(Options.editor_charset)) {
	   ret_charset=Parse_editorcharset_line(Options.editor_charset);
	}
	if (ret_charset==-1) {
	    flrn_char *msg;
	    const char *special;
	    special=_("Jeu de caractÃ¨res non reconnu : ");
	    msg=safe_malloc((strlen(special)+
			fl_strlen(*All_options[i].value.string))*
		    sizeof(flrn_char));
	    conversion_from_utf8(special,&msg,strlen(special)+1,(size_t)(-1));
	    strcat(msg,*All_options[i].value.string);
	    *error=msg;
	    return -1;
	}
	return 0;
}

int opt_do_set(flrn_char *str, int flag)
{
  int reverse, found;
  flrn_char *buf;
  flrn_char *end;
  flrn_char *end2;
  flrn_char *dummy;
  int i;

  buf=fl_strtok_r(str,delim,&dummy);
  if (buf==NULL) {
    if (!flag) { fprintf(stderr,"set utilisé sans argument\n");
      sleep(1);}
    return -1;
  }
  end = fl_strtok_r(NULL,fl_static("\n"),&dummy);
  if (end) end += fl_strspn(end,delim);

  if (fl_strncmp(buf,fl_static("no"),2)==0) { buf +=2; reverse=1;}
    else reverse=0;

  for (i=0; i< NUM_OPTIONS; i++){
    found=0;
    /* on cherche le nom de la variable */
    if ((fl_strncmp(buf, fl_static_tran(All_options[i].name),
		    strlen(All_options[i].name))==0)
     && (!fl_isalpha((int)*(buf+strlen(All_options[i].name))))) {
      if(flag && All_options[i].flags.lock) {
	Aff_error_fin(fl_static("On ne peut changer cette variable"),1,-1);
	return -1;
      }
      if (All_options[i].flags.obsolete) {
         if (flag) {
	    Aff_error_fin(fl_static_tran(All_options[i].desc),1,-1);
	    return -1;
         }
	 fprintf(stderr,"Option %s obsolète : %s\n",
		 All_options[i].name,All_options[i].desc);
	 if (All_options[i].value.integer==NULL) return -1; /* cool_arrows */
	 sleep(1);
      }
      /* variable entiere ? */
      if ((All_options[i].type==OPT_TYPE_BOOLEAN) ||
	  (All_options[i].type==OPT_TYPE_INTEGER)) {
	int oldvalue=*All_options[i].value.integer;

	buf=fl_strtok_r(end,delim,&dummy);

	if (buf) {
	  *All_options[i].value.integer=0;
	  if (All_options[i].type==OPT_TYPE_INTEGER)
	    *All_options[i].value.integer=fl_strtol(buf,NULL,10);
	  else *All_options[i].value.integer=fl_strtol(buf,NULL,10)?1:0;
	  if (fl_strncmp(buf,fl_static("on"),3)==0) 
	      *All_options[i].value.integer=1;
	  if (fl_strncmp(buf,fl_static("yes"),4)==0)
	      *All_options[i].value.integer=1;
	  if (fl_strncmp(buf,fl_static("oui"),4)==0)
	      *All_options[i].value.integer=1;
	    /* hack, pour color !!! */
	  if (fl_strncmp(buf,fl_static("auto"),5)==0)
	      *All_options[i].value.integer=-1;
	} else *All_options[i].value.integer=1;

	if (reverse != All_options[i].flags.reverse)
	  *All_options[i].value.integer = !*All_options[i].value.integer;
	All_options[i].flags.modified |=
	  (oldvalue!=*All_options[i].value.integer);
	return 0; /* Ok, tout s'est bien passé */
      } else if (All_options[i].type==OPT_TYPE_STRING) {
	/* pour aller apres le strtok :( */
	if (end && (*end==fl_static('"'))) {
	  end++;
	  end2=strchr(end,fl_static('"')); 
	  if (end2) *end2=fl_static('\0');  /* Pour enlever le '"' en fin */
	}
	if (end && *end) {
	  if (All_options[i].flags.allocated)
	    *All_options[i].value.string=
	      safe_realloc(*All_options[i].value.string,
		(fl_strlen(end)+1)*sizeof(flrn_char));
	  else
	    *All_options[i].value.string=
	      safe_malloc((fl_strlen(end)+1)*sizeof(flrn_char));

	  All_options[i].flags.allocated=1;
          fl_strcpy(*All_options[i].value.string,end);
	} else {
	  if (All_options[i].flags.allocated)
	    free(*All_options[i].value.string);
	  *All_options[i].value.string=NULL;
	}
	All_options[i].flags.modified=1;
	/* cas des charsets */
	{
	    flrn_char *bla;
	    if (test_charset_modified(i,&bla)==-1) {
	       if (flag) Aff_error_fin (bla,1,-1);
	       else {
		  char *trad;
		  int rc;
		  rc=conversion_to_terminal(bla,&trad,0,(size_t)(-1));
	          fprintf(stderr,"Jeu de caractère non reconnu : %s\n",trad);
		  if (rc==0) free(trad);
	          sleep(1);
	       }
	       free(bla);
	    }
	}
	return 0;
      }
    }
  }
  if(flag) Aff_error_fin (fl_static("Variable non reconnue"),1,-1);
  else { char *traduction;
         int resconv;
	 resconv=conversion_to_terminal(buf,&traduction,0,(size_t)(-1));
         fprintf(stderr,"Variable non reconnue : %s\n",traduction);
	 if (resconv==0) free(traduction);
         sleep(1);}
  return -1;
}

static int opt_aff_color_error(int res, int flag) {
  if (res <0) {
    const char *special;
    switch(res) {
      case -1:  special = fl_static("Ã‰chec de *color. Pas assez de champs");
		break;
      case -2:  special = fl_static("Ã‰chec de *color. Fields invalides");
		break;
      case -3:  special = fl_static("Ã‰chec de *color. Flags invalides");
		break;
      case -4:  special = fl_static("Ã‰chec de *color. Regexp invalide");
		break;
      case -5:  special=_("Ã‰chec de *color. Attribut buguÃ©");
		break;
      case -6:  special=_("Ã‰chec de *color. Pas assez de sous-expressions");
		break;
      default: special=_("Ã‰chec de (reg)color : erreur inconnue");
	       break;
    }
    if (flag)
	Aff_error_fin_utf8(special,1,-1);
    else {
      flrn_char *err;
      char *trad1,*trad2;
      int rc,resc1, resc2;
      rc=conversion_from_utf8(special, &err, 0, (size_t)(-1));
      resc1=conversion_to_terminal(option_ligne,&trad1,0,(size_t)(-1));
      resc2=conversion_to_terminal(err,&trad2,0,(size_t)(-1));
      fprintf(stderr,"%s: %s\n",trad1,trad2);
      if (resc1==0) free(trad1);
      if (resc2==0) free(trad2);
      if (rc==0) free(err);
      sleep(1);
    }
  }
  return (res==0)?0:-1;
}

int opt_do_color(flrn_char * str, int flag)
{
  int res;
  res=parse_option_color(2,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_mono(flrn_char * str, int flag)
{
  int res;
  res=parse_option_color(3,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_regcolor(flrn_char * str, int flag)
{
  int res;
  res=parse_option_color(1,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_my_hdr(flrn_char * str, int flag)
{
  flrn_char *buf2;
  string_list_type *parcours, *parcours2;
  int len;

  if (str==NULL) return -1;
  buf2=fl_strchr(str,fl_static(':'));
  if ((buf2==NULL) || (buf2!=fl_strpbrk(str,delim))) {
    char *err=fl_static("Echec de my_hdr : header invalide.");
    if (flag)
      Aff_error_fin(err,1,-1);
    else {
      char *trad1,*trad2;
      int resc1, resc2;
      resc1=conversion_to_terminal(option_ligne,&trad1,0,(size_t)(-1));
      resc2=conversion_to_terminal(err,&trad2,0,(size_t)(-1));
      fprintf(stderr,"%s: %s\n",trad1,trad2);
      if (resc1==0) free(trad1);
      if (resc2==0) free(trad2);
      sleep(1);
    }
    return -1;
  }

  len=(++buf2)-str;
  buf2+=fl_strspn(buf2,fl_static(" \t"));

  parcours2=NULL;
  parcours=Options.user_header;

  while (parcours) {
    if (fl_strncasecmp(parcours->str,str,len)==0) break;
    parcours2=parcours;
    parcours=parcours->next;
  }
  if (parcours) {
    if (*buf2=='\0') {
      free(parcours->str);
      if (parcours2) parcours2->next=parcours->next; else
	Options.user_header=parcours->next;
      free(parcours);
    } else {
      parcours->str=safe_realloc(parcours->str,(len+fl_strlen(buf2)+2)
	       *sizeof(flrn_char));
      fl_strncpy(parcours->str,str,len);
      (parcours->str)[len]=fl_static(' ');
      fl_strcpy(parcours->str+len+1,buf2);
    }
  } else if (*buf2!=fl_static('\0')) {
    parcours=safe_malloc(sizeof(string_list_type));
    parcours->str=safe_malloc((len+strlen(buf2)+2)*sizeof(flrn_char));
    fl_strncpy(parcours->str,str,len);
    (parcours->str)[len]=fl_static(' ');
    fl_strcpy(parcours->str+len+1,buf2);
    parcours->next=NULL;
    if (parcours2) parcours2->next=parcours; else
      Options.user_header=parcours;
  }
  return 0;
}

int opt_do_my_flags(flrn_char *str, int flag)
{
  string_list_type *parcours, *parcours2;

  if (str==NULL) return -1;
  if (fl_strncasecmp(str,fl_static("clear"),5)==0) {
    free_string_list_type(Options.user_flags);
    Options.user_flags=NULL;
    return 0;
  }
  parcours2=Options.user_flags;
  while (parcours2 && (parcours2->next)) 
    parcours2=parcours2->next;
  parcours=safe_malloc(sizeof(string_list_type));
  parcours->str=safe_flstrdup(str);
  parcours->next=NULL;
  if (parcours2) parcours2->next=parcours; else
    Options.user_flags=parcours;
  return 0;
}

void increase_size_header_list(int newsize) {
    int *new_list;
    new_list=safe_calloc(newsize,sizeof(int));
    memcpy(new_list,Options.header_list,sizeof(int)*size_header_list);
    if (Options.header_list!=&(deb_header_list[0])) free(Options.header_list);
    Options.header_list=new_list;
    new_list=safe_calloc(newsize,sizeof(int));
    memcpy(new_list,Options.weak_header_list,sizeof(int)*size_header_list);
    if (Options.weak_header_list!=&(deb_weak_header_list[0])) 
	 free(Options.weak_header_list);
    Options.weak_header_list=new_list;
    new_list=safe_calloc(newsize,sizeof(int));
    memcpy(new_list,Options.hidden_header_list,sizeof(int)*size_header_list);
    if (Options.hidden_header_list!=&(deb_hidden_header_list[0])) 
	 free(Options.hidden_header_list);
    Options.hidden_header_list=new_list;
    unknown_Headers = 
	safe_realloc(unknown_Headers,newsize*sizeof(Known_Headers));
    if (newsize>size_header_list)
      memset((unknown_Headers+size_header_list),0,
	      (newsize-size_header_list)*sizeof(Known_Headers));
    size_header_list=newsize;
}

int opt_do_header(flrn_char *str, int flag)
{
  int weak=0, hidden=0;
  int i=0;
  flrn_char *buf;
  flrn_char *dummy;

  buf=fl_strtok_r(str,delim,&dummy);
  if (buf==NULL) {
    Options.header_list[0]=-1;
    return 0;
  }
  if (fl_strcasecmp(buf,fl_static("weak"))==0) weak=1; else
    if (fl_strcasecmp(buf,fl_static("hide"))==0) hidden=1; else
	if (fl_strcasecmp(buf,fl_static("list"))!=0) 
     Options.header_list[i++]=Le_header(buf);

  while ((buf=fl_strtok_r(NULL,delim,&dummy)))
  {
    if (weak)
      Options.weak_header_list[i++]=Le_header(buf);
    else if (hidden)
      Options.hidden_header_list[i++]=Le_header(buf);
    else
      Options.header_list[i++]=Le_header(buf);
    if (i==size_header_list) increase_size_header_list(size_header_list+10);
  }
  if (weak)
    Options.weak_header_list[i]=-1; 
  else
  if (hidden)
    Options.hidden_header_list[i]=-1; 
  else
    Options.header_list[i]=-1;
  return 0;
}

int opt_do_bind(flrn_char *str, int flag)
{
  flrn_char *buf, *buf2, *buf3, *dummy;
  int res, mode=-1; /* mode=0 : commande mode=1 : menu mode=2 : pager */
  int add=0, lettre;
  struct key_entry key;
  size_t ntc;

  buf=fl_strtok_r(str,delim,&dummy);
  if (!buf) return -1;
  if (fl_strcasecmp(buf,fl_static("add"))==0) {
    add =1;
    buf=fl_strtok_r(NULL,delim,&dummy);
    if (!buf) return -1;
  }
  if (fl_strcasecmp(buf,fl_static("menu"))==0) mode=CONTEXT_MENU; else
    if (fl_strcasecmp(buf,fl_static("pager"))==0) mode=CONTEXT_PAGER; else
      if (fl_strcasecmp(buf,fl_static("command"))==0) mode=CONTEXT_COMMAND;
  if (mode!=-1) buf=fl_strtok_r(NULL,delim,&dummy); else
    mode=0;
  if (!buf) return -1;
  if (fl_strcasecmp(buf,fl_static("add"))==0) {
    add =1;
    buf=fl_strtok_r(NULL,delim,&dummy);
    if (!buf) return -1;
  }

  ntc=next_flch(buf,0);
  if (buf[ntc]==fl_static('\0')) {
      parse_key_string(buf,&key);
  } else {
    if (*buf == fl_static('\\')) buf++;
    if (fl_isdigit((int) *buf))
      lettre = fl_strtol(buf,NULL,0);
    else
      lettre = parse_key_name(buf);
    if (lettre <0) lettre =0;
    if (lettre >=MAX_FL_KEY) lettre = MAX_FL_KEY-1;
    key.entry=ENTRY_SLANG_KEY;
    key.value.slang_key=lettre;
  }
  buf2=fl_strtok_r(NULL,delim,&dummy);
  if (!buf2) return -1;
  if (*buf2==fl_static('\\')) buf2++;
  buf3=fl_strtok_r(NULL,fl_static("\n"),&dummy);
  if (buf3) 
    buf3+=fl_strspn(buf3, delim);

  res=(mode==CONTEXT_MENU ? Bind_command_menu(buf2,&key,buf3,add) :
	mode==CONTEXT_PAGER ? Bind_command_pager(buf2,&key,buf3,add) : 
		  Bind_command_explicite(buf2,&key,buf3,add));
  Free_key_entry(&key);
  if (res <0) {
    if (flag) Aff_error_fin_utf8
	(_("Ã‰chec de la commande bind."),1,-1); else
    {
      flrn_char *err;
      char *trad1,*trad2;
      int rc,resc1,resc2;
      rc=conversion_from_utf8(_("Echec du bind : %s"),&err,0,(size_t)(-1));
      resc2=conversion_to_terminal(err,&trad2,0,(size_t)(-1));
      resc1=conversion_to_terminal(option_ligne,&trad1,0,(size_t)(-1));
      fprintf(stderr,trad2,trad1);
      if (resc1==0) free(trad1);
      if (resc2==0) free(trad2);
      if (rc==0) free(err);
      sleep(1);
    }
  }
  return (res <0) ? -1 : 0;
}

int opt_do_autocmd(flrn_char *str, int flag)
{
   flrn_char *buf,*buf2,*dummy;
   autocmd_list_type *newautocmd;
   int i;

   buf=fl_strtok_r(str,delim,&dummy);
   if (!buf) return -1;
   newautocmd=safe_calloc(1,sizeof(autocmd_list_type));
   if (fl_strcasecmp(buf,fl_static("enter"))==0) 
       newautocmd->autocmd_flags=AUTOCMD_ENTER;
      else if (fl_strcasecmp(buf,fl_static("leave"))==0)
	  newautocmd->autocmd_flags=AUTOCMD_LEAVE;
        else {
	  if (flag) Aff_error_fin(fl_static("autocmd invalide."),1,-1); else
	  {
	     char *trad1;
	     int resc1;
	     resc1=conversion_to_terminal(buf,&trad1,0,(size_t)(-1));
	     fprintf(stderr,"Mot inconnu pour autocmd : %s\n",trad1);
	     if (resc1==0) free(trad1);
	     sleep(1);
	  }
	  free(newautocmd);
	  return -1;
        }
   buf=fl_strtok_r(NULL,delim,&dummy);
   if (!buf) {
      free(newautocmd);
      return -1;
   }
   if (fl_regcomp(&(newautocmd->match),buf,REG_EXTENDED|REG_NOSUB)) {
      if (flag) Aff_error_fin(fl_static("Regexp invalide pour autocmd.")
	      ,1,-1);
      else {
	 char *trad1;
	 int resc1;
	 resc1=conversion_to_terminal(buf,&trad1,0,(size_t)(-1));
         fprintf(stderr,"Regexp invalide pour autocmd : %s\n",trad1);
	 if (resc1==0) free(trad1);
         sleep(1);
      }
      free(newautocmd);
      return -1;
   }
   buf=fl_strtok_r(NULL,delim,&dummy);
   if (!buf) {
     fl_regfree(&(newautocmd->match));
     free(newautocmd);
     return -1;
   }
   buf2=fl_strtok_r(NULL,fl_static("\n"),&dummy);
   if (buf2) buf2+=fl_strspn(buf2,delim);
   for (i=0;i<NB_FLCMD;i++) {
       /* FIXME : lecture d'une vraie commande via Lit_commande_explicite */
      if (fl_strcmp(buf,fl_static_tran(Flcmds[i].nom))==0) {
         newautocmd->cmd=i;
	 newautocmd->arg=safe_flstrdup(buf2);
	 if (Options.user_autocmd==NULL)
	    Options.user_autocmd=newautocmd;
	 else {
	    autocmd_list_type *parcours=Options.user_autocmd;
   	    while (parcours->next) parcours=parcours->next;
	    /* oui, on veut ajouter à la fin */
	    parcours->next=newautocmd;
	 }
	 return 0;
       }
   }
   fl_regfree(&(newautocmd->match));
   if (flag) Aff_error_fin(fl_static("Commande invalide pour autocmd.")
	   ,1,-1);
   else {
      char *trad1;
      int resc1;
      resc1=conversion_to_terminal(buf,&trad1,0,(size_t)(-1));
      fprintf(stderr,"Commande invalide pour autocmd : %s\n",trad1);
      if (resc1==0) free(trad1);
      sleep(1);
   }
   free(newautocmd);
   return -1;
}

static const flrn_char *delim_cond=fl_static(" <>=\t");
static int parse_conditionnelle (flrn_char *ligne, int flag)
{
    int len;
    len=fl_strlen(ligne);
    if (len==0) return 1;
    if (ligne[len-1]==fl_static('\n')) ligne[len-1]='\0';
    len=fl_strcspn(ligne,delim_cond);
    if (fl_strncmp(ligne,fl_static("name"),len)==0) {
	ligne+=len;
	ligne+=fl_strspn(ligne,delim_cond);

	return (fl_strstr(name_program,ligne)!=NULL);
    }
    if (fl_strncmp(ligne,fl_static("version"),len)==0) {
	int cp=0,nb;
	ligne+=len;
	while (1) {
	   while ((*ligne) && (fl_strchr(" \t",*ligne))) ligne++;
	   if (*ligne==fl_static('\0')) return 1;
	   if (*ligne=='=') cp|=1;
	   else if (*ligne=='<') cp|=2;
	   else if (*ligne=='>') cp|=4;
	   else break;
	   *ligne++;
	}
	nb=fl_strtol(ligne,NULL,10);
	return (((cp | 1) && (nb==version_number)) ||
		((cp | 2) && (nb>version_number)) ||
		((cp | 4) && (nb<version_number)));
    }
    if (fl_strncmp(ligne,fl_static("has"),len)==0) {
	ligne+=len;
	while ((*ligne) && (fl_strchr(" \t",*ligne))) ligne++;
	return version_has(ligne);
    }
    if (fl_strncmp(ligne,fl_static("hasnot"),len)==0) {
	ligne+=len;
	while ((*ligne) && (fl_strchr(" \t",*ligne))) ligne++;
	return 1-version_has(fl_static_rev(ligne));
    }
    return 0;
}



/* Analyse d'une ligne d'option. Prend en argument la ligne en question */
/* on fait maintenant appel aux fonctions do_opt_* pour faire le boulot */
static void raw_parse_options_line (flrn_char *ligne, int flag, int *actif,
	  int *inactif)
{
  flrn_char *buf;
  flrn_char *eol;
  flrn_char *dummy;
  int i;

  if ((char) ligne[0]==OPT_COMMENT_CHAR) return;


  buf=fl_strtok_r(ligne,delim,&dummy);
  if (buf==NULL) return;

  /* tests des conditionnelles */
  if (actif) 
  {
    if (fl_strcmp(buf,fl_static("endif"))==0) {
	if (*inactif) (*inactif)--; else
	    if (*actif) (*actif)--; else
	        if (!flag) { fprintf(stderr,"endif sans correspondant\n");
		                sleep(1);}

	return;
    }
    if (fl_strcmp(buf,fl_static("if"))==0) {
	eol=fl_strtok_r(NULL,"\n",&dummy);
        if (eol) eol += fl_strspn(eol,delim);
	if ((*inactif) || 
		(parse_conditionnelle (eol, flag)==0))
           (*inactif)++;
        else
   	   (*actif)++;
	return;
    }

    if (*inactif) return;
  }
/* recherche du name * */
  if (fl_strcmp(buf,fl_static(WORD_NAME_PROG))==0) {
     buf=fl_strtok_r(NULL,delim,&dummy);
     if (buf==NULL) {
       if (!flag) { fprintf(stderr,"name utilisé sans argument\n");
	 sleep(1);}
       return;
     }
     if (fl_strcmp(name_program,buf)!=0) return;
     buf=fl_strtok_r(NULL,delim,&dummy);
     if (buf==NULL) return;
  }

  /* le reste de la ligne, a passer a opt_do_* */
  eol=fl_strtok_r(NULL,"\n",&dummy);
  if (eol) eol += fl_strspn(eol,delim);

  /* recherche de l'option */
  for (i=0; i< NUMBER_OF_OPT_CMD; i++) {
    if(fl_strcmp(buf,fl_static_tran(Optcmd_liste[i].name))==0) {
      (void) (*Optcmd_liste[i].parse)(eol,flag);
      return;
    }
  }
  /* on n'a pas trouve */
  if (flag) Aff_error_fin("Option non reconnue",1,-1); else
  {
    char *bla=NULL; int res;
    res = conversion_to_terminal(buf,&bla,0,(size_t)(-1));
    if (bla) {
      fprintf(stderr,"Option non reconnue : %s\n",bla);
      if (res==0) free(bla);
    }
    sleep(1);
  }
  /* On peut se le permettre : flrn n'est pas lancé */
}

/* pour afficher des messages plus clairs */
/* seul "truc" : avec des includes, il ne faut pas oublier le free AVANT
   un nouveau strdup, pour éviter deux free sur le meme pointeur */
void parse_options_line (flrn_char *ligne, int flag, int *actif, 
	int *inactif)
{
  if (option_ligne) free(option_ligne);
  option_ligne = safe_flstrdup(ligne);
  raw_parse_options_line (ligne, flag, actif, inactif);
  free(option_ligne);
  option_ligne=NULL;
  return;
}

static flrn_char * print_option(int i, flrn_char *buf, size_t buflen) {
  if ((All_options[i].type==OPT_TYPE_INTEGER) ||
      (All_options[i].type==OPT_TYPE_BOOLEAN)) {
    if (*All_options[i].value.integer == All_options[i].flags.reverse) {
      fl_snprintf(buf,buflen,fl_static("no%s"),All_options[i].name);
    } else {
      if (((All_options[i].type==OPT_TYPE_BOOLEAN) && 
		   (*All_options[i].value.integer!=-1))  ||
	  (*All_options[i].value.integer == !All_options[i].flags.reverse)) {
	fl_snprintf(buf,buflen,fl_static("%s"),All_options[i].name);
    } else
      fl_snprintf(buf,buflen,fl_static("%s=%d"),All_options[i].name,
	  (All_options[i].flags.reverse)?(!*All_options[i].value.integer):
	  (*All_options[i].value.integer));
    }
  } else if (All_options[i].type==OPT_TYPE_STRING) {
    /* on met un format beton pour éviter les pb si en fait
     * on utilise sprintf */
    if (*All_options[i].value.string) {
	 size_t rest;
         fl_snprintf(buf,buflen,fl_static("%s=\""),All_options[i].name);
	 rest=buflen-fl_strlen(buf);
	 fl_strncat(buf,*All_options[i].value.string, rest);
	 if (fl_strlen(*All_options[i].value.string)<rest-2) 
	    fl_strcat(buf,fl_static("\""));
	 buf[buflen-1]=fl_static('\0');
    }
    else fl_snprintf(buf,buflen,fl_static("%s=\"\""),All_options[i].name);
  }
  return buf;
}

void print_header_name(FILE *file, int i) {
  if (i>=0) {
    if (i==NB_KNOWN_HEADERS) fputs("others",file); else
    fputs(Headers[i].header,file);
  } else {
    i=-i-2;
    if (i==size_header_list) return; /* ne devrait pas arriver */
    fputs(unknown_Headers[i].header,file);
  }
}

/* affiche les options */
void dump_variables(FILE *file) {
  int i,resc,r0;
  flrn_char buf[80];
  char *trad;
  flrn_char *t0;
  r0=conversion_from_utf8(_("# Variables, modifiables avec set :\n\n"),
	  &t0,0,(size_t)(-1));
  resc=conversion_to_file(t0,&trad,0,(size_t)(-1));
  fputs(trad,file);
  if (resc==0) free(trad);
  if (r0==0) free(t0);
  for (i=0; i< NUM_OPTIONS; i++) {
    if (!All_options[i].flags.obsolete) {
	resc=conversion_to_file(
		print_option(i,buf,80),&trad,0,(size_t)(-1));
        fprintf(file,"set %s\n",trad);
	if (resc==0) free(trad);
    }
  }
  r0=conversion_from_utf8(_("\n\n# Affichage des headers"),
	  &t0,0,(size_t)(-1));
  resc=conversion_to_file(t0,&trad,0,(size_t)(-1));
  fputs(trad,file);
  if (resc==0) free(trad);
  if (r0==0) free(t0);
  fprintf(file,"\nheader list: ");
  for (i=0; i<size_header_list; i++) {
    if (Options.header_list[i]==-1) break;
    putc(' ',file);
    print_header_name(file,Options.header_list[i]);
  }
  fprintf(file,"\nheader weak: ");
  for (i=0; i<size_header_list; i++) {
    if (Options.weak_header_list[i]==-1) break;
    putc(' ',file);
    print_header_name(file,Options.weak_header_list[i]);
  }
  fprintf(file,"\nheader hide: ");
  for (i=0; i<size_header_list; i++) {
    if (Options.hidden_header_list[i]==-1) break;
    putc(' ',file);
    print_header_name(file,Options.hidden_header_list[i]);
  }
  return;
}

/* Affiche le nom d'un header */
/* Affiche les options modifiées, comme un beau .flrn(rc) */
void dump_flrnrc(FILE *file) {
  int rc,r0;
  char *trad;
  flrn_char *t0;
  string_list_type *parcours;
  dump_variables(file);
  r0=conversion_from_utf8(_("\n\n# Headers ajoutÃ©s dans les posts"),
	  &t0,0,(size_t)(-1));
  rc=conversion_to_file(t0,&trad,0,(size_t)(-1));
  fputs(trad,file);
  if (rc==0) free(trad);
  if (r0==0) free(t0);
  parcours=Options.user_header;
  while (parcours) {
    rc=conversion_to_file(parcours->str,&trad,0,(size_t)(-1));
    fprintf(file,"\nmy_hdr %s",trad);
    if (rc==0) free(trad);
    parcours=parcours->next;
  }
  r0=conversion_from_utf8(_("\n\n# Flags ajoutÃ©s dans les messages"),
	  &t0,0,(size_t)(-1));
  rc=conversion_to_file(t0,&trad,0,(size_t)(-1));
  fputs(trad,file);
  if (rc==0) free(trad);
  if (r0==0) free(t0);
  parcours=Options.user_flags;
  while (parcours) {
    rc=conversion_to_file(parcours->str,&trad,0,(size_t)(-1));
    fprintf(file,"\nmy_flags %s",trad);
    if (rc==0) free(trad);
    parcours=parcours->next;
  }
  r0=conversion_from_utf8(_("\n\n# Il reste les couleurs...\n"),
	  &t0,0,(size_t)(-1));
  rc=conversion_to_file(t0,&trad,0,(size_t)(-1));
  fputs(trad,file);
  if (rc==0) free(trad);
  if (r0==0) free(t0);
  dump_colors_in_flrnrc(file);
  if (Options.user_autocmd) {
      r0=conversion_from_utf8(_("\n\n# Il reste les autocmd aussi, Ã  faire.\n"),
	      &t0,0,(size_t)(-1));
      rc=conversion_to_file(t0,&trad,0,(size_t)(-1));
      fputs(trad,file);
      if (rc==0) free(trad);
      if (r0==0) free(t0);
  }
  return;
}

/* TODO : changer ça, mais avec les menus */
int change_value(Liste_Menu *debut_menu, Liste_Menu **courant, 
	flrn_char **name, int *tofree, Cmd_return *la_commande) {
  int num=(int)(long)((*courant)->lobjet);
  char buf[60];
  flrn_char flbuf[80];
  int changed =0;

  *name=NULL; *tofree=0;
  if (la_commande->cmd[CONTEXT_MENU]==-1) {
     if (la_commande->before) free(la_commande->before);
     if (la_commande->after) free(la_commande->after);
     return -2;
  }
  if (All_options[num].flags.lock) {
    int rc;
    flrn_char *bla;
    size_t l;
    rc=conversion_from_utf8(_("L'option %s ne peut pas Ãªtre changÃ©e"),
	    &bla,0,(size_t)(-1));
    l=fl_strlen(bla)+strlen(All_options[num].name)+2;
    *name=safe_malloc(l*sizeof(flrn_char));
    fl_snprintf(*name,l,bla,All_options[num].name);
    if (rc==0) free(bla);
    *tofree=1;
    return 0;
  }
  switch (All_options[num].type) {
    case OPT_TYPE_BOOLEAN : 
    	*All_options[num].value.integer = !(*All_options[num].value.integer);
    	changed=1;
	break;
    case OPT_TYPE_INTEGER : {
	int new_value, ret, rc, rc2, col;
	flrn_char *t0;
	char *bla;

	buf[0]='\0';
	Cursor_gotorc(Screen_Rows-2,0);
	Screen_erase_eol();
	rc=conversion_from_utf8(_("Nouvelle valeur: "),
		&t0,0,(size_t)(-1));
	rc2=conversion_to_terminal(t0,&bla,0,(size_t)(-1));
	col=str_estime_width(bla,0,(size_t)(-1));
	Screen_write_string(bla);
	if (rc2==0) free(bla);
	if (rc==0) free(t0);
	ret=flrn_getline(flbuf,80,buf,60,Screen_Rows-2,col);
	if (ret!=0) break;
	new_value=fl_strtol(flbuf,NULL,10);
	changed=(new_value!=*All_options[num].value.integer);
	*All_options[num].value.integer=new_value;
	break;
	}
    case OPT_TYPE_STRING : {
	int ret, rc, rc2, col;
	flrn_char *t0;
	char *bla;
	buf[0]='\0';
	Cursor_gotorc(Screen_Rows-2,0);
	Screen_erase_eol();
	rc=conversion_from_utf8(_("Nouvelle valeur: "),
		 &t0,0,(size_t)(-1));
	rc2=conversion_to_terminal(t0,&bla,0,(size_t)(-1));
	col=str_estime_width(bla,0,(size_t)(-1));
	Screen_write_string(bla);
	if (rc2==0) free(bla);
	if (rc==0) free(t0);
	ret=flrn_getline(flbuf,80,buf,60,Screen_Rows-2,col);
	if (ret!=0) break;
	if (*All_options[num].value.string) 
	  changed=(fl_strcmp(flbuf,*All_options[num].value.string)!=0);
	else changed=(fl_strlen(flbuf)!=0);
	if (changed) {
	   if (All_options[num].flags.allocated) 
	   	free(*All_options[num].value.string);
	   if (fl_strlen(flbuf)!=0) {
	     All_options[num].flags.allocated=1;
	     *All_options[num].value.string=safe_flstrdup(flbuf);
	   } else {
	     All_options[num].flags.allocated=0;
	     *All_options[num].value.string=NULL;
	   }
	   {
	       flrn_char *bla;
	       if (test_charset_modified(num,&bla)==-1) {
		   *name=bla;
		   *tofree=1;
	       }
	   }
	}
	break;
	}
    default : {
	  int rc;
          rc=conversion_from_utf8(_("   Impossible de changer cette option. Non implÃ©mentÃ©."),
	       name,0,(size_t)(-1));
          if (rc==0) *tofree=1;
	  return 0;
    }
  }
  if (changed) {
      print_option(num,flbuf,80);
      change_menu_line(*courant,0,flbuf);
  }
  return changed;
}

int aff_desc_option (void *value, flrn_char **ligne) {
  int num=(int)(long) value;
  int res;
  res=conversion_from_utf8(_(All_options[num].desc),ligne,0,(size_t)(-1));
  return (res==0);
}

static struct format_elem_menu fmt_option_menu_e [] =
    { { 80, 80, -1, 2, 0, 7 } };
struct format_menu fmt_option_menu = { 1, &(fmt_option_menu_e[0]) };
void menu_config_variables() {
  int i, rc;
  flrn_char buf[80], *ptr, *trad;
  Liste_Menu *menu=NULL, *courant=NULL;
  int *valeur;

  ptr=&(buf[0]);
  for (i=0; i< NUM_OPTIONS; i++){
    if (All_options[i].flags.obsolete) continue;
    print_option(i,buf,80);
    courant=ajoute_menu(courant,&fmt_option_menu,&ptr,(void *)i);
    if (!menu) menu=courant;
  }
  num_help_line=13;
  rc=conversion_from_utf8(
	  _("<entrÃ©e> pour changer une option, q pour quitter."), &trad,
	  0, (size_t)(-1));
  valeur = (int *) Menu_simple(menu, NULL, aff_desc_option, change_value, 
	  trad);
  if (rc==0) free(trad);
  Libere_menu(menu);
  return;
}





/* Ouvre et parse un fichier... Renvoie -1 si pas de fichier */
/* flags = flags d'appel de open_flrnfile (2 si config initiale, 0 sinon */
/* flag_option = flag d'appel pour parse_option_line */
static int parse_option_file (char *name, int flags, int flag_option) {
  char buf1[MAX_BUF_SIZE];
  flrn_char *traduction=NULL;
  FILE *flrnfile;
  int res, actif=0, inactif=0;

  flrnfile=open_flrnfile(name,"r",flags,NULL);
  if (flrnfile==NULL) return -1;
  while (fgets(buf1,MAX_BUF_SIZE,flrnfile)) {
    /* traduction -> flrn_char */
    res=conversion_from_file(buf1,&traduction,0,(size_t)(-1));
    if (traduction) {
      parse_options_line(traduction,flag_option,&actif,&inactif);
      if (res==0) free(traduction);
    }
  }
  fclose(flrnfile);
  return 0;
}

int get_and_push_option(flrn_char *param,
	  void (push_string)(flrn_char *), void (push_int)(int)) {
    /* on commence par reconnaitre une variable */
    flrn_char *buf,*dummy;
    int i;

    buf=fl_strtok_r(param,delim,&dummy);
    if (buf==NULL) return (-1);
    for (i=0; i< NUM_OPTIONS; i++){
	if ((fl_strncmp(buf, fl_static_tran(All_options[i].name),
			strlen(All_options[i].name))==0)
	   && (!fl_isalpha((int)*(buf+strlen(All_options[i].name))))) {
	    if ((All_options[i].type==OPT_TYPE_BOOLEAN) ||
		(All_options[i].type==OPT_TYPE_INTEGER)) {
	        push_int(*All_options[i].value.integer);
		return 0;
	    }
	    if (All_options[i].type==OPT_TYPE_STRING) {
		push_string(*All_options[i].value.string);
		return 1;
	    }
	    return (-2);
	}
    }
    return -1;
}

const flrn_char *Help_Lines[17];
const char *menu_default_help_line=N_(" Menu: \\quit : quitter le menu     \\select : selectionner un element");

void load_help_line_file() {
  int i, rc;
  FILE *flrnfile;
  char buf1[83];
  flrn_char *trad;

  /* TODO : memory leak */
  conversion_from_utf8(_(menu_default_help_line),&trad,0,(size_t)(-1));
  for (i=0;i<14;i++) Help_Lines[i]=trad;
  conversion_from_utf8(_(" ? : aide   \\quit : quitter flrn   \\next-unread : article suivant"),&trad,0,(size_t)(-1));
  Help_Lines[0]=Help_Lines[3]=trad;
  conversion_from_utf8(_(" ? : aide   \\quit : quitter flrn   \\show-tree pour revenir à l'affichage normal"),&trad,0,(size_t)(-1));
  Help_Lines[1]=trad;
  conversion_from_utf8(_(" ? : aide   ^C : quitter le pager   \\pgdown : descend d'une page"),&trad,0,(size_t)(-1));
  Help_Lines[2]=trad;
  conversion_from_utf8(_(" ^C ou \\quit : quitter le pager     \\pgdown : descend d'une page"),&trad,0,(size_t)(-1));
  Help_Lines[4]=trad;
  conversion_from_utf8(_(" ^C : sortir du défilement       autre touche : page suivante "),&trad,0,(size_t)(-1));
  Help_Lines[14]=trad;
  conversion_from_utf8(_(" Appuyez sur une touche pour continuer ... "),&trad,
	  0,(size_t)(-1));
  Help_Lines[15]=trad;
  conversion_from_utf8(_("   q : quitter l'aide         m : menu      0-9 : rubriques de l'aide "),&trad,0,(size_t)(-1));
  Help_Lines[16]=trad;
  if (Options.help_lines_file==NULL) return;
  flrnfile=open_flrnfile(Options.help_lines_file,"r",0,NULL);
  if (flrnfile==NULL) return;
  for (i=0;i<17;i++) {
      if (fgets(buf1,83,flrnfile)==NULL) break;
      rc=conversion_from_file(buf1,&trad,0,(size_t)(-1));
      if (rc>0) trad=safe_flstrdup(trad);
      Help_Lines[i]=trad;
      /* TODO : corriger le memory leak */
  }
  fclose(flrnfile);
}


void init_options() {
  char *buf;
  int i;

  unknown_Headers = safe_calloc(size_header_list,sizeof(Known_Headers));

  buf=getenv("NNTPSERVER");
  if (buf)
  {
    /* J'ai bien le droit de garder ce pointeur, non ? */
    /* Et surtout, je n'ai encore lu aucun fichier de config, sinon, ca */
    /* ferait mal */
    Options.serveur_name=buf;
/*    strncpy(Options.serveur_name,buf,MAX_SERVER_LEN);
    Options.serveur_name[MAX_SERVER_LEN-1]=0; */
  }
  for (i=0;i<size_header_list;i++) {
    unknown_Headers[i].header_len=0;
    unknown_Headers[i].header=NULL;
  }
  parse_option_file(NULL,2,0);
}  

/* ne sert a rien, mais bon */
void free_options() {
  int i;
  autocmd_list_type *parcours, *parcours2;
  for (i=0; i< NUM_OPTIONS; i++)
    if (All_options[i].flags.allocated) {
      free(*All_options[i].value.string);
      All_options[i].flags.allocated=0;
      *All_options[i].value.string=NULL;
    }
  free_string_list_type(Options.user_header);
  free_string_list_type(Options.user_flags);
  Options.user_header=Options.user_flags=NULL;
  parcours=Options.user_autocmd;
  while (parcours) {
     regfree(&(parcours->match));
     if (parcours->arg) free(parcours->arg);
     parcours2=parcours;
     parcours=parcours->next;
     free(parcours2);
  }
  Options.user_autocmd=NULL;
  free(unknown_Headers);
  if (size_header_list!=ISIZE_HEADER_LIST) {
      free(Options.header_list);
      free(Options.weak_header_list);
      free(Options.hidden_header_list);
  }
}

void free_string_list_type (string_list_type *s_l_t) {
  string_list_type *parcours, *parcours2;

  if (s_l_t==NULL) return;
  parcours=s_l_t;
  while (parcours) {
    parcours2=parcours->next;
    free(parcours->str);
    free(parcours);
    parcours=parcours2;
  }
}  
