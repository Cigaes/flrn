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
#include "art_group.h"
#define IN_OPTION_C
#include "options.h"
#include "site_config.h"

static char *delim = "=: \t\n";

Known_Headers unknown_Headers[MAX_HEADER_LIST];


void var_comp(char *var, int len)
{
  char *buf=var;
  int used;
  char *guess=NULL;
  int prefix_len=0;
  int match=0;
  int i,j;
  if (strncmp(buf,"no",2)==0) buf +=2;
  used=strlen(buf);
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
      }
  if (match==1) {
    if (strlen(guess) < 1+len) { strcpy(buf, guess);
      strcat(buf," ");
    }
  } else if (match >1) {
    if (prefix_len < len) {strncpy(buf, guess, prefix_len);
      buf[prefix_len]='\0';
    }
  }
  return;
}

/* il faut que len soit plus grand que touts les noms de commandes
 * on pourait ajouter pour la completude la completion apres color
 * mais ca veut dire reecrire le code qui gere ca... */
void options_comp(char *option, int len)
{
  int used;
  int res;
  char *my_option=safe_malloc(len);
  char *buf;

  strcpy(my_option,option);
  buf=strtok(my_option,delim);
  if (!buf) {free(my_option); return;}
  used=strlen(buf);

  if (strncmp(buf,OPT_SET,used)==0) {
    if (used<OPT_SET_LEN) {
      strcpy(option,OPT_SET); strcat(option," ");
      free(my_option);
      return;}
    if((buf=strtok(NULL,delim))) {
      if (!strtok(NULL,delim)) {
	var_comp(buf, len - 3 - (buf - option));
	sprintf(option,"%s %s",OPT_SET,buf);
      }
    } else {strcpy(option,OPT_SET); strcat(option," ");}
    free(my_option);
    return;
  } else
  if (strncmp(buf,OPT_SET_COLOR,used)==0) {
    if (used<OPT_SET_COLOR_LEN) {
      strcpy(option,OPT_SET_COLOR); strcat(option," ");
      free(my_option);
      return;}
    if (!strtok(NULL,delim)) {
      strcpy(option,OPT_SET_COLOR); strcat(option," ");
    }
  } else
  if (strncmp(buf,OPT_MY_HEADER,used)==0) {
    /* On ne fait pas de complétion pour my_header, même si ca pourrait */
    /* se faire dans le cas de remove... A noter que my_header est en   */
    /* concurrence avec mono...						*/
    if (used<OPT_MY_HEADER_LEN) {
      strcpy(option,OPT_MY_HEADER); strcat(option," ");
      free(my_option);
      return;}
    if (!strtok(NULL,delim)) {
      strcpy(option,OPT_MY_HEADER); strcat(option," ");
    }
  } else
  if (strncmp(buf,OPT_HEADER,used)==0) {
    if (used<OPT_HEADER_LEN) {
      strcpy(option,OPT_HEADER); strcat(option," ");
      free(my_option);
      return;}
    if (!strtok(NULL,delim)) {
      strcpy(option,OPT_HEADER); strcat(option," ");
    }
  } else
  if (strncmp(buf,OPT_BIND,used)==0) {
    char *buf1;
    if (used<OPT_BIND_LEN) {
      strcpy(option,OPT_BIND); strcat(option," ");
      free(my_option);
      return;}
    if((buf1=strtok(NULL,delim))) {
      if ((buf=strtok(NULL,delim))) {
        if (*buf=='\\') buf++;
	res = Comp_cmd_explicite(buf,len - (buf-option) -3);
	sprintf(option,"%s %s %s",OPT_BIND,buf1,buf);
	if (!res) strcat(option," ");
      }
    } else {
      strcpy(option,OPT_BIND); strcat(option," ");
    }
    free(my_option);
    return;
  }
  free(my_option);
  return;
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


/* Analyse d'une ligne d'option. Prend en argument la ligne en question */
void parse_options_line (char *ligne, int flag)
{
  char *buf;
  int i;
  int found, reverse;

  if (ligne[0]==OPT_COMMENT_CHAR) return;
  buf=strtok(ligne,delim);
  if (buf==NULL) return;
  if(strcmp(buf,OPT_SET)==0) {
    buf=strtok(NULL,delim);
    if (buf==NULL) {
      if (!flag) { fprintf(stderr,"set utilisé sans argument\n");
	sleep(1);}
      return;
    }
    if (strncmp(buf,"no",2)==0) { buf +=2; reverse=1;}
    else reverse=0;
    for (i=0; i< NUM_OPTIONS; i++){
      found=0;
      if (strncmp(buf, All_options[i].name, strlen(All_options[i].name))==0){
	if(flag && All_options[i].flags.lock) {
	  Aff_error("On ne peut changer cette variable");
	  return;
	}
	if ((All_options[i].type==OPT_TYPE_BOOLEAN) ||
	    (All_options[i].type==OPT_TYPE_INTEGER)) {
	  buf=strtok(NULL,delim);
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
	  return;
	} else if (All_options[i].type==OPT_TYPE_STRING) {
	  char *buf2;
	  /* pour aller apres le strtok :( */
	  buf =strtok(NULL,"\n");
	  if (buf)
	    buf+=strspn(buf, delim);
	  if (buf && (*buf=='"')) {
	    buf++;
	    buf2=strchr(buf,'"'); 
	    if (buf2) *buf2='\0';  /* Pour enlever le '"' en fin */
	  }
          if (buf && *buf) {
	    if (All_options[i].flags.allocated)
	      *All_options[i].value.string=
		safe_realloc(*All_options[i].value.string,
		  (strlen(buf)+1));
	    else
	      *All_options[i].value.string=
		safe_malloc((strlen(buf)+1));
	  All_options[i].flags.allocated=1;
          strcpy(*All_options[i].value.string,buf);
	  } else {
	    if (All_options[i].flags.allocated)
	      free(*All_options[i].value.string);
	    *All_options[i].value.string=NULL;
	  }
	  return;
	}
      }
    }
    if (!found) {
      if(flag) Aff_error ("Variable non reconnue");
      else { fprintf(stderr,"Variable non reconnue : %s\n",buf);
	sleep(1);}
    }
    return;
  }
  if ((strcmp(buf,OPT_SET_NEWCOLOR)==0) ||
      (strcmp(buf,OPT_SET_COLOR)==0) ||
      (strcmp(buf,OPT_SET_MONO)==0)){
    int res;
    int func;
    if(strcmp(buf,OPT_SET_NEWCOLOR)==0) func=1;
    else if(strcmp(buf,OPT_SET_COLOR)==0) func=2;
	else func=3;
    buf=strtok(NULL,"\n");
    if (buf==NULL) return;
    res=parse_option_color(func,buf);
    if (res <0) {
      char *err="Echec de *color : erreur inconnue";
      switch(res) {
        case -1:  err = "Echec de *color. Pas assez de champs";
		  break;
        case -2:  err = "Echec de *color. Field invalides";
		  break;
        case -3:  err = "Echec de *color. Flag invalides";
		  break;
        case -4:  err = "Echec de *color. Regexp invalide";
		  break;
        case -5:  err = "Echec de *color. Attribut bugué";
		  break;
      }
      if (flag)
	  Aff_error(err);
      else {
	fprintf(stderr,"%s: %s\n",ligne,err);
	sleep(1);
      }
    }
    return;
  } else
  if (strcmp(buf,OPT_MY_HEADER)==0) {
    char *buf2;
    user_hdr_type *parcours, *parcours2;
    int len;

    buf=strtok(NULL,"\n");
    if (buf==NULL) return;
    buf+=strspn(buf,delim);
    buf2=strchr(buf,':');
    if ((buf2==NULL) || (buf2!=strpbrk(buf,delim))) {
       char *err="Echec de my_hdr : header invalide.";
       if (flag)
         Aff_error(err);
       else {
         fprintf(stderr,"%s: %s\n",ligne,err);
	 sleep(1);
       }
       return;
    }
    len=(++buf2)-buf;
    buf2+=strspn(buf2," \t");
    parcours2=NULL;
    parcours=Options.user_header;
    while (parcours) {
      if (strncasecmp(parcours->str,buf,len)==0) break;
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
	strncpy(parcours->str,buf,len);
	(parcours->str)[len]=' ';
	strcpy(parcours->str+len+1,buf2);
      }
    } else if (*buf2!='\0') {
      parcours=safe_malloc(sizeof(user_hdr_type));
      parcours->str=safe_malloc(len+strlen(buf2)+2);
      strncpy(parcours->str,buf,len);
      (parcours->str)[len]=' ';
      strcpy(parcours->str+len+1,buf2);
      parcours->next=NULL;
      if (parcours2) parcours2->next=parcours; else
        Options.user_header=parcours;
    }
    return;
  } else
  if (strcmp(buf,OPT_HEADER)==0) {
    int weak=0, hidden=0;
    i=0;

    buf=strtok(NULL,delim);
    if (buf==NULL) {
      Options.header_list[0]=-1;
      return;
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
    return;
  } else
  if (strcmp(buf,OPT_BIND)==0) {
    int lettre;
    char *buf2, *buf3;
    int res, mode=-1; /* mode=0 : commande mode=1 : menu mode=2 : pager */
    buf=strtok(NULL,delim);
    if (!buf) return;
    if (strncasecmp(buf,"menu",7)==0) mode=1; else
      if (strncasecmp(buf,"pager",5)==0) mode=2; else
	if (strncasecmp(buf,"command",7)==0) mode=0;
    if (mode!=-1) buf=strtok(NULL,delim); else
      mode=0;
    lettre = *buf;
    if (buf[1]) {
      if (*buf == '\\') buf++;
      if (isdigit(*buf))
	lettre = strtol(buf,NULL,0);
      else
	lettre = parse_key_name(buf);
      if (lettre <0) lettre =0;
      if (lettre >=MAX_FL_KEY) lettre = MAX_FL_KEY-1;
    }
    buf2=strtok(NULL,delim);
    if (!buf2) return;
    if (*buf2=='\\') buf2++;
    buf3=strtok(NULL,"\n");
    if (buf3) 
      buf3+=strspn(buf3, delim);

    res=(mode==1 ? Bind_command_menu(buf2,lettre,buf3) :
	 mode==2 ? Bind_command_pager(buf2,lettre,buf3) : 
	           Bind_command_explicite(buf2,lettre,buf3));
    if (res <0) {
      if (flag) Aff_error("Echec de la commande bind."); else
      {
	fprintf(stderr,"Echec du bind : %s\n",ligne);
	sleep(1);
      }
    }
    return;
  }
  if (flag) Aff_error("Option non reconnue"); else
  {
    fprintf(stderr,"Option non reconnue : %s\n",ligne);
    sleep(1);
  }
  /* On peut se le permettre : flrn n'est pas lancé */
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
  user_hdr_type *parcours;
  fprintf(file,"# Variables :\n");
  for (i=0; i< NUM_OPTIONS; i++){
    fprintf(file,"set %s",print_option(i,buf,80));
  }
  fprintf(file,"\nheader: ");
  for (i=0; i<MAX_HEADER_LIST; i++) {
    if (Options.header_list[i]+1) fprintf(file," %d",Options.header_list[i]+1);
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

int change_value(void *value, char **nom, int i, char *name, int len) {
  int num=(int)(long) value;
  char buf[80];
  int changed =0;
  if (All_options[num].flags.lock) {
    snprintf(name,len,"   L'option %s ne peut pas être changée",
	All_options[num].name);
    return 0;
  }
  if (All_options[num].type == OPT_TYPE_BOOLEAN) {
    *All_options[num].value.integer = !(*All_options[num].value.integer);
    changed=1;
  } else {
    strncpy(name,"   Impossible de changer cette option. Non implémenté.",len);
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
  valeur = (int *) Menu_simple(menu, NULL, aff_desc_option, change_value);
  Libere_menu_noms(menu);
  return;
}

void init_options() {
  char *buf,buf1[MAX_BUF_SIZE];
  FILE *flrnfile;
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
  flrnfile=open_flrnfile(NULL,"r+",2,NULL);
  if (flrnfile==NULL) return;
  while (fgets(buf1,MAX_BUF_SIZE,flrnfile)) 
    parse_options_line(buf1,0);
  fclose(flrnfile);
}  

/* ne sert a rien, mais bon */
void free_options() {
  int i;
  user_hdr_type *parcours, *parcours2;
  for (i=0; i< NUM_OPTIONS; i++)
    if (All_options[i].flags.allocated) {
      free(*All_options[i].value.string);
      All_options[i].flags.allocated=0;
      *All_options[i].value.string=NULL;
    }
  parcours=Options.user_header;
  while (parcours) {
    parcours2=parcours->next;
    free(parcours->str);
    free(parcours);
    parcours=parcours2;
  }
}
