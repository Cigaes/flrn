/* flrn v 0.1                                                           */
/*              options.c           22/11/97                            */
/*                                                                      */
/* Gestion des options...                                               */
/*                                                                      */
/*                                                                      */

#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

#include "flrn.h"
#define IN_OPTION_C
#include "art_group.h"
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
#include "options.h"
#include "site_config.h"

static char *delim = "=: \t\n";

Known_Headers unknown_Headers[MAX_HEADER_LIST];

static void free_string_list_type (string_list_type *s_l_t);
static int parse_option_file (char *name, int flags, int flag_option);

static char *option_ligne = NULL;

int var_comp(char *var, int len, Liste_Chaine *debut)
{
  Liste_Chaine *courant=debut, *nouveau;
  char *buf=var;
  int used;
  char *guess=NULL;
  int prefix_len=0;
  int match=0;
  int i,j;
  if (strncmp(buf,"no",2)==0) {
     buf +=2;
     strcat(debut->une_chaine,"no");
  }
  used=strlen(buf);
  if (used==0) return 0;
  for (i=0; i< NUM_OPTIONS; i++)
    if(strncmp(buf, All_options[i].name,used)==0)
      if (!All_options[i].flags.lock) {
	if (match) {
	  if (!prefix_len) prefix_len=strlen(guess);
	  for (j=0;j<prefix_len;j++)
	    if (guess[j] != All_options[i].name[j]){
	      prefix_len=j; break;
	    }
	}
	guess=All_options[i].name;
	match++;
	nouveau=safe_calloc(1,sizeof(Liste_Chaine));
	nouveau->une_chaine=safe_malloc((len+1)*sizeof(char));
	strcpy(nouveau->une_chaine,debut->une_chaine);
	strcat(nouveau->une_chaine,guess);
        strcat(nouveau->une_chaine," ");
	nouveau->complet=1;
	nouveau->suivant=courant->suivant;
	courant->suivant=nouveau;
	courant=nouveau;
      }
  if (match==1) {
    free(debut->une_chaine);
    debut->une_chaine=courant->une_chaine;
    debut->suivant=courant->suivant;
    var[0]='\0';
    free(courant);
    return 0;
  } else if (match >1) {
    debut->complet=0;
    used=strlen(debut->une_chaine);
    strncat(debut->une_chaine,guess,prefix_len);
    debut->une_chaine[used+prefix_len]='\0';
    var[0]='\0';
    return -1;
  }
  debut->complet=-1;
  return -2;
}

static char *get_optcmd_name(void * ptr,int num)
{
  return ((struct _Optcmd *)ptr)[num].name;
}
/* on fait maintenant les checks sur len...
 * on pourait ajouter pour la completude la completion apres color
 * mais ca veut dire reecrire le code qui gere ca... */

int options_comp(char *str, int len, Liste_Chaine *debut)
{
  int res,res2,bon,i;
  Liste_Chaine *courant, *pere, *suivant;
  int result[NUMBER_OF_OPT_CMD];
  char *suite;
  for (i=0;i<NUMBER_OF_OPT_CMD;i++) result[i]=0;
  res = Comp_generic(debut, str,len,(void *)Optcmd_liste,NUMBER_OF_OPT_CMD,
      get_optcmd_name,delim,result);
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

static char *bindarg_names[] = {
  "add",
  "menu",
  "pager",
  "command"
};

static char *get_name_liste(void *p,int i) {
  return ((char **)p)[i];
}

int bind_comp(char *str, int len, Liste_Chaine *debut)
{
  int res,res2,i,bon,offset;
  char *suite;
  Liste_Chaine *courant, *pere, *suivant;
  int result[sizeof(bindarg_names)/sizeof(bindarg_names[0])];
  for (i=0;i<sizeof(bindarg_names)/sizeof(bindarg_names[0]);i++)
    result[i]=0;
  if (len-strlen(debut->une_chaine)<2) return -2;
  if ((str[1]==' ')||(str[1]=='\0')) {
    debut->une_chaine[strlen(debut->une_chaine)+2]='\0';
    strncat(debut->une_chaine,str,2);
    if (str[1]==' ') return Comp_cmd_explicite(str+2,len,debut); else {
       debut->complet=0;
       return 0;
    }
  }
  if (str[0]=='\\') {
    offset=strcspn(str,delim);
    if (offset< len) {
      offset += strspn(str+offset,delim);
      str[offset-1]='\0';
      strcat(debut->une_chaine,str);
      strcat(debut->une_chaine," ");
      return Comp_cmd_explicite(str+offset,len,debut);
    }
    strcat(debut->une_chaine,str);
    strcat(debut->une_chaine," ");
    str[0]='\0';
    return 0;
  }
  res = Comp_generic(debut,str,len,(void *)bindarg_names,
      sizeof(bindarg_names)/sizeof(bindarg_names[0]),
      get_name_liste,delim,result);
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
       suite=safe_strdup(str);
       res2=bind_comp(suite,len,courant);
       free(suite);
       if (res2<-1) {
          free(courant->une_chaine);
          pere->suivant=courant->suivant;
          free(courant);
          courant=pere->suivant;
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
int Le_header(char *buf) {
  int i,l;
  char *buf2;

  if ((i=atoi(buf))>0) return i-1;
  if (strcasecmp(buf,"others")==0) return NB_KNOWN_HEADERS;
  buf2=strpbrk(buf,delim);
  if (buf2==NULL) l=strlen(buf); else l=buf2-buf;
  for (i=0;i<NB_KNOWN_HEADERS;i++) {
    if ((l==Headers[i].header_len-1) && (strncasecmp(buf,Headers[i].header,l)==0))
      return i;
  }
  for (i=0;i<MAX_HEADER_LIST;i++) {
    if ((l==unknown_Headers[i].header_len-1) && (strncasecmp(buf,unknown_Headers[i].header,l)==0))
      return -i-2;
    if (unknown_Headers[i].header_len==0) break;
  }
  if (i==MAX_HEADER_LIST) return -i-2; /* Pas grave... */
  unknown_Headers[i].header=safe_malloc(l+2);
  strncpy(unknown_Headers[i].header,buf,l);
  unknown_Headers[i].header[l]=':';
  unknown_Headers[i].header[l+1]='\0';
  unknown_Headers[i].header_len=l+1;
  return -i-2;
}

int opt_do_include(char *buf, int flag)
{
  char *buf2;
  int found;
  static int deep_inclusion;

  if (buf == NULL) return -1;
  if (*buf=='"') {
    buf++;
    buf2=strchr(buf,'"'); 
    if (buf2) *buf2='\0';  /* Pour enlever le '"' en fin */
  }
  if (*buf) {
    deep_inclusion++;
    if (deep_inclusion<10)
      found=parse_option_file(buf,0,flag);
    else found=-1;
    deep_inclusion--;
    if ((found==-1) && (!flag)) {
      fprintf(stderr,"Erreur : include %s impossible !\n",buf);
      sleep(1);
    }
    return found;
  }
  return 0;
}

int opt_do_set(char *str, int flag)
{
  int reverse, found;
  char *buf;
  char *end;
  char *end2;
  int i;

  buf=strtok(str,delim);
  if (buf==NULL) {
    if (!flag) { fprintf(stderr,"set utilisé sans argument\n");
      sleep(1);}
    return -1;
  }
  end = strtok(NULL,"\n");
  if (end) end += strspn(end,delim);

  if (strncmp(buf,"no",2)==0) { buf +=2; reverse=1;}
    else reverse=0;

  for (i=0; i< NUM_OPTIONS; i++){
    found=0;
    /* on cherche le nom de la variable */
    if (strncmp(buf, All_options[i].name, strlen(All_options[i].name))==0){
      if(flag && All_options[i].flags.lock) {
	Aff_error_fin("On ne peut changer cette variable",1,-1);
	return -1;
      }
      /* variable entiere ? */
      if ((All_options[i].type==OPT_TYPE_BOOLEAN) ||
	  (All_options[i].type==OPT_TYPE_INTEGER)) {
	int oldvalue=*All_options[i].value.integer;

	buf=strtok(end,delim);

	if (buf) {
	  *All_options[i].value.integer=0;
	  if (All_options[i].type==OPT_TYPE_INTEGER)
	    *All_options[i].value.integer=atoi(buf);
	  else *All_options[i].value.integer=atoi(buf)?1:0;
	  if (strncmp(buf,"on",3)==0) *All_options[i].value.integer=1;
	  if (strncmp(buf,"yes",4)==0) *All_options[i].value.integer=1;
	  if (strncmp(buf,"oui",4)==0) *All_options[i].value.integer=1;
	    /* hack, pour color !!! */
	  if (strncmp(buf,"auto",5)==0) *All_options[i].value.integer=-1;
	} else *All_options[i].value.integer=1;

	if (reverse != All_options[i].flags.reverse)
	  *All_options[i].value.integer = !*All_options[i].value.integer;
	All_options[i].flags.modified |=
	  (oldvalue!=*All_options[i].value.integer);
	return 0; /* Ok, tout s'est bien passé */
      } else if (All_options[i].type==OPT_TYPE_STRING) {
	/* pour aller apres le strtok :( */
	if (end && (*end=='"')) {
	  end++;
	  end2=strchr(end,'"'); 
	  if (end2) *end2='\0';  /* Pour enlever le '"' en fin */
	}
	if (end && *end) {
	  if (All_options[i].flags.allocated)
	    *All_options[i].value.string=
	      safe_realloc(*All_options[i].value.string,
		(strlen(end)+1));
	  else
	    *All_options[i].value.string=
	      safe_malloc((strlen(end)+1));

	  All_options[i].flags.allocated=1;
          strcpy(*All_options[i].value.string,end);
	} else {
	  if (All_options[i].flags.allocated)
	    free(*All_options[i].value.string);
	  *All_options[i].value.string=NULL;
	}
	All_options[i].flags.modified=1;
	return 0;
      }
    }
  }
  if(flag) Aff_error_fin ("Variable non reconnue",1,-1);
  else { fprintf(stderr,"Variable non reconnue : %s\n",buf);
      sleep(1);}
  return -1;
}

static int opt_aff_color_error(int res, int flag) {
  if (res <0) {
    char *err="Echec de *color : erreur inconnue";
    switch(res) {
      case -1:  err = "Echec de *color. Pas assez de champs";
		break;
      case -2:  err = "Echec de *color. Fields invalides";
		break;
      case -3:  err = "Echec de *color. Flags invalides";
		break;
      case -4:  err = "Echec de *color. Regexp invalide";
		break;
      case -5:  err = "Echec de *color. Attribut bugué";
		break;
      case -6:  err = "Echec de *color. Pas assez de sous-expressions";
		break;
    }
    if (flag)
	Aff_error_fin(err,1,-1);
    else {
      fprintf(stderr,"%s: %s\n",option_ligne,err);
      sleep(1);
    }
  }
  return (res==0)?0:-1;
}

int opt_do_color(char * str, int flag)
{
  int res;
  res=parse_option_color(2,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_mono(char * str, int flag)
{
  int res;
  res=parse_option_color(3,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_regcolor(char * str, int flag)
{
  int res;
  res=parse_option_color(1,str);
  return opt_aff_color_error(res,flag);
}

int opt_do_my_hdr(char * str, int flag)
{
  char *buf2;
  string_list_type *parcours, *parcours2;
  int len;

  if (str==NULL) return -1;
  buf2=strchr(str,':');
  if ((buf2==NULL) || (buf2!=strpbrk(str,delim))) {
    char *err="Echec de my_hdr : header invalide.";
    if (flag)
      Aff_error_fin(err,1,-1);
    else {
      fprintf(stderr,"%s: %s\n",option_ligne,err);
      sleep(1);
    }
    return -1;
  }

  len=(++buf2)-str;
  buf2+=strspn(buf2," \t");

  parcours2=NULL;
  parcours=Options.user_header;

  while (parcours) {
    if (strncasecmp(parcours->str,str,len)==0) break;
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
      parcours->str=safe_realloc(parcours->str,len+strlen(buf2)+2);
      strncpy(parcours->str,str,len);
      (parcours->str)[len]=' ';
      strcpy(parcours->str+len+1,buf2);
    }
  } else if (*buf2!='\0') {
    parcours=safe_malloc(sizeof(string_list_type));
    parcours->str=safe_malloc(len+strlen(buf2)+2);
    strncpy(parcours->str,str,len);
    (parcours->str)[len]=' ';
    strcpy(parcours->str+len+1,buf2);
    parcours->next=NULL;
    if (parcours2) parcours2->next=parcours; else
      Options.user_header=parcours;
  }
  return 0;
}

int opt_do_my_flags(char *str, int flag)
{
  string_list_type *parcours, *parcours2;

  if (str==NULL) return -1;
  if (strncasecmp(str,"clear",5)==0) {
    free_string_list_type(Options.user_flags);
    Options.user_flags=NULL;
    return 0;
  }
  parcours2=Options.user_flags;
  while (parcours2 && (parcours2->next)) 
    parcours2=parcours2->next;
  parcours=safe_malloc(sizeof(string_list_type));
  parcours->str=safe_strdup(str);
  parcours->next=NULL;
  if (parcours2) parcours2->next=parcours; else
    Options.user_flags=parcours;
  return 0;
}


int opt_do_header(char *str, int flag)
{
  int weak=0, hidden=0;
  int i=0;
  char *buf;

  buf=strtok(str,delim);
  if (buf==NULL) {
    Options.header_list[0]=-1;
    return 0;
  }
  if (strcasecmp(buf,"weak")==0) weak=1; else
    if (strcasecmp(buf,"hide")==0) hidden=1; else
	if (strcasecmp(buf,"list")!=0) Options.header_list[i++]=Le_header(buf);

  while((buf=strtok(NULL,delim)) && (i<MAX_HEADER_LIST-1))
  {
    if (weak)
      Options.weak_header_list[i++]=Le_header(buf);
    else if (hidden)
      Options.hidden_header_list[i++]=Le_header(buf);
    else
      Options.header_list[i++]=Le_header(buf);
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

int opt_do_bind(char *str, int flag)
{
  int lettre;
  char *buf, *buf2, *buf3;
  int res, mode=-1; /* mode=0 : commande mode=1 : menu mode=2 : pager */
  int add=0;

  buf=strtok(str,delim);
  if (!buf) return -1;
  if (strcasecmp(buf,"add")==0) {
    add =1;
    buf=strtok(NULL,delim);
    if (!buf) return -1;
  }
  if (strcasecmp(buf,"menu")==0) mode=CONTEXT_MENU; else
    if (strcasecmp(buf,"pager")==0) mode=CONTEXT_PAGER; else
      if (strcasecmp(buf,"command")==0) mode=CONTEXT_COMMAND;
  if (mode!=-1) buf=strtok(NULL,delim); else
    mode=0;
  if (!buf) return -1;
  if (strcasecmp(buf,"add")==0) {
    add =1;
    buf=strtok(NULL,delim);
    if (!buf) return -1;
  }
  lettre = *buf;
  if (buf[1]) {
    if (*buf == '\\') buf++;
    if (isdigit((int) *buf))
      lettre = strtol(buf,NULL,0);
    else
      lettre = parse_key_name(buf);
    if (lettre <0) lettre =0;
    if (lettre >=MAX_FL_KEY) lettre = MAX_FL_KEY-1;
  }
  buf2=strtok(NULL,delim);
  if (!buf2) return -1;
  if (*buf2=='\\') buf2++;
  buf3=strtok(NULL,"\n");
  if (buf3) 
    buf3+=strspn(buf3, delim);

  res=(mode==CONTEXT_MENU ? Bind_command_menu(buf2,lettre,buf3,add) :
	mode==CONTEXT_PAGER ? Bind_command_pager(buf2,lettre,buf3,add) : 
		  Bind_command_explicite(buf2,lettre,buf3,add));
  if (res <0) {
    if (flag) Aff_error_fin("Echec de la commande bind.",1,-1); else
    {
      fprintf(stderr,"Echec du bind : %s\n",option_ligne);
      sleep(1);
    }
  }
  return (res <0) ? -1 : 0;
}

/* Analyse d'une ligne d'option. Prend en argument la ligne en question */
/* on fait maintenant appel aux fonctions do_opt_* pour faire le boulot */
static void raw_parse_options_line (char *ligne, int flag)
{
  char *buf;
  char *eol;
  int i;

  if (ligne[0]==OPT_COMMENT_CHAR) return;


  buf=strtok(ligne,delim);
  if (buf==NULL) return;
/* recherche du name * */
  if (strcmp(buf,WORD_NAME_PROG)==0) {
     buf=strtok(NULL,delim);
     if (buf==NULL) {
       if (!flag) { fprintf(stderr,"name utilisé sans argument\n");
	 sleep(1);}
       return;
     }
     if (strcmp(name_program,buf)!=0) return;
     buf=strtok(NULL,delim);
     if (buf==NULL) return;
  }

  /* le reste de la ligne, a passer a opt_do_* */
  eol=strtok(NULL,"\n");
  if (eol) eol += strspn(eol,delim);

  /* recherche de l'option */
  for (i=0; i< NUMBER_OF_OPT_CMD; i++) {
    if(strcmp(buf,Optcmd_liste[i].name)==0) {
      (void) (*Optcmd_liste[i].parse)(eol,flag);
      return;
    }
  }
  /* on n'a pas trouve */
  if (flag) Aff_error_fin("Option non reconnue",1,-1); else
  {
    fprintf(stderr,"Option non reconnue : %s\n",buf);
    sleep(1);
  }
  /* On peut se le permettre : flrn n'est pas lancé */
}

/* pour afficher des messages plus clairs */
void parse_options_line (char *ligne, int flag)
{
  option_ligne = safe_strdup(ligne);
  raw_parse_options_line (ligne, flag);
  free(option_ligne);
  return;
}

static char *print_option(int i, char *buf, int buflen) {
  if ((All_options[i].type==OPT_TYPE_INTEGER) ||
      (All_options[i].type==OPT_TYPE_BOOLEAN)) {
    if (*All_options[i].value.integer == All_options[i].flags.reverse) {
      snprintf(buf,buflen,"no%s\n",All_options[i].name);
    } else {
      if ((All_options[i].type==OPT_TYPE_BOOLEAN) ||
	  (*All_options[i].value.integer == !All_options[i].flags.reverse)) {
	snprintf(buf,buflen,"%s\n",All_options[i].name);
    } else
      snprintf(buf,buflen,"%s=%d\n",All_options[i].name,
	  (All_options[i].flags.reverse)?(!*All_options[i].value.integer):
	  (*All_options[i].value.integer));
    }
  } else if (All_options[i].type==OPT_TYPE_STRING) {
    /* on met un format beton pour éviter les pb si en fait
     * on utilise sprintf */
    if (*All_options[i].value.string)
      snprintf(buf,buflen,"%.15s=\"%.60s\"\n",All_options[i].name,
	  *All_options[i].value.string);
    else snprintf(buf,buflen,"%s=\"\"\n",All_options[i].name);
  }
  return buf;
}

/* affiche les options */
void dump_variables(FILE *file) {
  int i;
  char buf[80];
  string_list_type *parcours;
  fprintf(file,"# Variables :\n");
  for (i=0; i< NUM_OPTIONS; i++){
    fprintf(file,"set %s",print_option(i,buf,80));
  }
  fprintf(file,"\nheader list: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.header_list[i]+1) fprintf(file," %d",Options.header_list[i]+1);
    else break;
  }
  fprintf(file,"\nheader weak: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.weak_header_list[i]+1) fprintf(file," %d",Options.weak_header_list[i]+1);
    else break;
  }
  fprintf(file,"\nheader hide: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.hidden_header_list[i]+1) fprintf(file," %d",Options.hidden_header_list[i]+1);
    else break;
  }
  parcours=Options.user_header;
  while (parcours) {
    fprintf(file,"\nmy_hdr %s",parcours->str);
    parcours=parcours->next;
  }
  fprintf(file,"\n\n# C'est tout, il ne manque plus que les couleurs.\n");
  return;
}

/* Affiche le nom d'un header */
void print_header_name(FILE *file, int i) {
  if (i>=0) {
    if (i==NB_KNOWN_HEADERS) fputs("others",file); else
    fputs(Headers[i].header,file);
  } else {
    i=-i-2;
    if (i==MAX_HEADER_LIST) return;
    fputs(unknown_Headers[i].header,file);
  }
}

/* Affiche les options modifiées, comme un beau .flrn(rc) */
void dump_flrnrc(FILE *file) {
  int i;
  char buf[80];
  string_list_type *parcours;
  fprintf(file,"# Variables, modifiables avec set :\n\n");
  for (i=0; i< NUM_OPTIONS; i++){
    if (!All_options[i].flags.modified) continue;
    fprintf(file,"# %s\n",All_options[i].desc);
    fprintf(file,"set %s",print_option(i,buf,80));
  }
  fprintf(file,"\n\n# Affichage des headers\nheader list: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.header_list[i]+1)  {
        putc(' ',file);
    	print_header_name(file,Options.header_list[i]);
    } else break;
  }
  fprintf(file,"\nheader weak: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.header_list[i]+1)  {
        putc(' ',file);
    	print_header_name(file,Options.header_list[i]);
    } else break;
  }
  fprintf(file,"\nheader hide: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.header_list[i]+1)  {
        putc(' ',file);
    	print_header_name(file,Options.header_list[i]);
    } else break;
  }
  fprintf(file,"\n\n# Headers ajoutés dans les posts");
  parcours=Options.user_header;
  while (parcours) {
    fprintf(file,"\nmy_hdr %s",parcours->str);
    parcours=parcours->next;
  }
  fprintf(file,"\n\n# Flags ajoutés pour voir les messages");
  parcours=Options.user_flags;
  while (parcours) {
    fprintf(file,"\nmy_flags %s",parcours->str);
    parcours=parcours->next;
  }
  fprintf(file,"\n\n# Il reste les couleurs...\n");
  dump_colors_in_flrnrc(file);
  return;
}

int change_value(void *value, char **nom, int i, char *name, int len, int key) {
  int num=(int)(long) value;
  char buf[80];
  int changed =0;
  if (key!=13) return 0;
  if (All_options[num].flags.lock) {
    snprintf(name,len,"   L'option %s ne peut pas être changée",
	All_options[num].name);
    return 0;
  }
  switch (All_options[num].type) {
    case OPT_TYPE_BOOLEAN : 
    	*All_options[num].value.integer = !(*All_options[num].value.integer);
    	changed=1;
	break;
    case OPT_TYPE_INTEGER : {
	int new_value, ret;
	buf[0]='\0';
	Cursor_gotorc(Screen_Rows-2,0);
	Screen_erase_eol();
	Screen_write_string("Nouvelle valeur: ");
	ret=getline(buf,60,Screen_Rows-2,17);
	if (ret!=0) break;
	new_value=strtol(buf,NULL,10);
	changed=(new_value!=*All_options[num].value.integer);
	*All_options[num].value.integer=new_value;
	break;
	}
    case OPT_TYPE_STRING : {
	int ret;
	buf[0]='\0';
	Cursor_gotorc(Screen_Rows-2,0);
	Screen_erase_eol();
	Screen_write_string("Nouvelle valeur: ");
	ret=getline(buf,60,Screen_Rows-2,17);
	if (ret!=0) break;
	if (*All_options[num].value.string) 
	  changed=(strcmp(buf,*All_options[num].value.string)!=0);
	else changed=(strlen(buf)!=0);
	if (changed) {
	   if (All_options[num].flags.allocated) 
	   	free(*All_options[num].value.string);
	   if (strlen(buf)!=0) {
	     All_options[num].flags.allocated=1;
	     *All_options[num].value.string=safe_strdup(buf);
	   } else {
	     All_options[num].flags.allocated=0;
	     *All_options[num].value.string=NULL;
	   }
	}
	break;
	}
    default : 
    	strncpy
	   (name,"   Impossible de changer cette option. Non implémenté.",len);
  }
  if (changed) {
    free(*nom);
    print_option(num,buf,80);
    *nom = safe_strdup(buf);
  }
  return 0;
}

void aff_desc_option (void *value, char *ligne, int len) {
  int num=(int)(long) value;

  snprintf(ligne,len,All_options[num].desc);
}

void menu_config_variables() {
  int i;
  char buf[80];
  Liste_Menu *menu=NULL, *courant=NULL;
  int *valeur;

  for (i=0; i< NUM_OPTIONS; i++){
    print_option(i,buf,80);
    courant=ajoute_menu(courant,safe_strdup(buf),(void *)i);
    if (!menu) menu=courant;
  }
  valeur = (int *) Menu_simple(menu, NULL, aff_desc_option, change_value, "<entrée> pour changer une option, q pour quitter.");
  Libere_menu_noms(menu);
  return;
}

/* Ouvre et parse un fichier... Renvoie -1 si pas de fichier */
/* flags = flags d'appel de open_flrnfile (2 si config initiale, 0 sinon */
/* flag_option = flag d'appel pour parse_option_line */
static int parse_option_file (char *name, int flags, int flag_option) {
  char buf1[MAX_BUF_SIZE];
  FILE *flrnfile;

  flrnfile=open_flrnfile(name,"r",flags,NULL);
  if (flrnfile==NULL) return -1;
  while (fgets(buf1,MAX_BUF_SIZE,flrnfile)) 
    parse_options_line(buf1,flag_option);
  fclose(flrnfile);
  return 0;
}


void init_options() {
  char *buf;
  int i;

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
  for (i=0;i<MAX_HEADER_LIST;i++) {
    unknown_Headers[i].header_len=0;
    unknown_Headers[i].header=NULL;
  }
  parse_option_file(NULL,2,0);
}  

/* ne sert a rien, mais bon */
void free_options() {
  int i;
  for (i=0; i< NUM_OPTIONS; i++)
    if (All_options[i].flags.allocated) {
      free(*All_options[i].value.string);
      All_options[i].flags.allocated=0;
      *All_options[i].value.string=NULL;
    }
  free_string_list_type(Options.user_header);
  free_string_list_type(Options.user_flags);
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
