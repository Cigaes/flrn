/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_color.c : gestion des couleurs (syntax-highlight)
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

#define DEF_FILD_NAMES

#include "flrn.h"
#include "flrn_slang.h"
#include "options.h"
#include "flrn_color.h"

static char UNUSED rcsid[]="$Id$";

/* I want to use this code and the old one together */
#define START_COLOR_NUM 11
#if START_COLOR_NUM <= NROF_FIELDS
#error START_COLOR_NUM too small
#endif


/* structure pour stoquer les couleurs */
/* attributs sert pour la couleur */
struct Obj_color_struct {
  char fg[15];
  char bg[15];
  FL_Char_Type attributs;
  FL_Char_Type attributs_mono;
};

/* les couleurs par défaut des champs */
struct Obj_color_struct Colors[NROF_FIELDS] =
	    { {"default","default",0,0},  /* Normal */
	      {"green","default",0,0},  /* Header */
	      {"default","default",FL_REV_MASK | FL_BOLD_MASK,
		FL_REV_MASK | FL_BOLD_MASK},/* Status */
	      {"brightred",  "default",0,0},  /* Error  */
	      {"magenta","default",0,0}, /* Quoted */
	      {"default","default",FL_BOLD_MASK,
		FL_BOLD_MASK}, /* Aff_fin */
	      {"blue",   "default",0,0}, /* signature */
	      {"default","default",0,0},  /* File */
	      {"default","default",0,0},  /* summary */
	      {"default","default",FL_REV_MASK,FL_REV_MASK},  /* search */
	    };



/* le délimiteur pour les options */
static char *delim = "=: \t\n";

/* une limite sur le numéro de () dans les regexp */
#define REG_MAX_SUB 10

/* la structure pour un objet a highlighter */
struct Highlight {
  regex_t regexp;
  int flags;
  struct Obj_color_struct colors; /* la couleur à utiliser */
  int pat_num;		/* le numéro du pattern de la regexp a colorier */
  int col_num;		/* le numéro de la couleur */
  int field_mask;	/* les valeurs de field possibles */
  struct Highlight *next;
} *highlight_first=NULL, *highlight_last=NULL;

/* les flags correspondant*/
#define HIGH_FLAGS_EXCLUDE 1
/* avec line, ne rien matcher d'autre sur la ligne */
#define HIGH_FLAGS_LINE 2
/* highlighter toute la ligne */
#define HIGH_FLAGS_CASE 4
/* etre case sensitive */
#define NUM_FLAGS 3
static char *flags_names[]={"exclude","line","case"};

/* les "couleurs" */
static const char *bold_str="bold";
static const char *blink_str="blink";
static const char *underline_str="underline";
static const char *reverse_str="reverse";

/* vire tout, libère la mémoire */
void free_highlights() {
  struct Highlight *current=highlight_first;
  while(current) {
    regfree(&current->regexp);
    highlight_first=current;
    current=current->next;
    free(highlight_first);
  }
  highlight_first=highlight_last=NULL;
}

static int parse_std(char *buf, int field) {
  int i;
  if (strcasecmp(buf,"std")==0)
    return field;
  if (strncasecmp(buf,"std-",4)==0)
    for (i=0;i<NROF_FIELDS;i++)
      if (strcasecmp(buf+4,Field_names[i])==0)
	return i;
  return -1;
}

/* ligne de la forme field[,field] flag[,flag...] fg bg attr[,attr...] regexp */
/* -1, il manque des arguments 
 * -2, bug dans field
 * -3, le champ flag est bidon
 * -4, la regexp compile pas
 * -5, attributs bidon...
 * -6, pas assez de sous-patterns
 * */
/* clear tout seul efface tout */
/* func 1 = regcolor, 2=color, 3=mono */
int parse_option_color(int func, char *line)
{
  char *buf;
  char *buf2;
  int i;
  struct Highlight *new_pat;
  int res;
  int field=0; /* le champ pour la couleur std */

  buf = strtok(line,delim);
  if (buf == NULL) return -1;
  if (strcasecmp(buf,"clear")==0) {
    free_highlights();
    return 0;
  }
  /* si on n'en a pas besoin, on le libèreer plus tard */
  new_pat = safe_calloc(1,sizeof(struct Highlight));
  /* le premier champ, c'est field[,field] */
  /* on n'a pas de controle d'erreur pertinent... */
  while(buf) {
    buf2=strchr(buf,',');
    if (buf2) {*buf2=0; buf2++;}
    for (i=0;i<NROF_FIELDS;i++) {
      if (strcasecmp(buf,Field_names[i])==0) {
	new_pat->field_mask |= 1<<i; 
	field=i;
	break;
      }
    }
    if (i==NROF_FIELDS) {
      if (strcasecmp(buf,"-") && strcasecmp(buf,"all")) {
	free(new_pat);
	return -2;
      }
    }
    buf=buf2;
  }
  if (new_pat->field_mask == 0) { new_pat->field_mask = ~0;}

  buf = strtok(NULL,delim);
  if (func == 1) {
    if (buf == NULL) { free(new_pat); return -1;}
    /* le deuxième champ, ce sont les flags */
    for (i=0;i<NUM_FLAGS;i++) {
      if (strstr(buf,flags_names[i])) new_pat->flags |= 1<<i; 
    }
    if ((buf2=strpbrk(buf,"0123456789"))) {
      new_pat->pat_num = strtol(buf2,NULL,10);
      if (new_pat->pat_num >= REG_MAX_SUB) { free(new_pat); return -3; }
    } else new_pat->pat_num=0;
    buf = strtok(NULL,delim);
  }
  if (func !=3) {
    if (buf == NULL) { free(new_pat); return -1;}
    /* le 2/3eme champ : fg */
    strncpy(new_pat->colors.fg, buf, 14);
    buf = strtok(NULL,delim);
    if (buf == NULL) { free(new_pat); return -1;}
    /* le 3/4eme champ : bg */
    strncpy(new_pat->colors.bg, buf, 14);
    buf = strtok(NULL,delim);
    if ((i=parse_std(new_pat->colors.fg,field)) >=0 )
      strncpy(new_pat->colors.fg,Colors[i].fg,14);
    if ((i=parse_std(new_pat->colors.bg,field)) >=0 )
      strncpy(new_pat->colors.bg,Colors[i].bg,14);
  }
  /* le 2/4/5eme champ : attr */
  while(buf) {
    buf2=strchr(buf,',');
    if (buf2) {*buf2=0; buf2++;}
    if ((i=parse_std(buf,field)) >=0 ) {
      new_pat->colors.attributs|=Colors[i].attributs;
      new_pat->colors.attributs_mono|=Colors[i].attributs_mono;
    }
    else
    if (strcasecmp(buf,bold_str)==0) {
      new_pat->colors.attributs|=FL_BOLD_MASK;
      new_pat->colors.attributs_mono|=FL_BOLD_MASK;
    }
    else
    if (strcasecmp(buf,blink_str)==0) {
      new_pat->colors.attributs|=FL_BLINK_MASK;
      new_pat->colors.attributs_mono|=FL_BLINK_MASK;
    }
    else
    if (strcasecmp(buf,underline_str)==0) {
      new_pat->colors.attributs|=FL_ULINE_MASK;
      new_pat->colors.attributs_mono|=FL_ULINE_MASK;
    }
    else
    if (strcasecmp(buf,reverse_str)==0) {
      new_pat->colors.attributs|=FL_REV_MASK;
      new_pat->colors.attributs_mono|=FL_REV_MASK;
    }
    else
    if (strcasecmp(buf,"-") && strcasecmp(buf,"normal"))
      return -5;
    buf=buf2;
  }

  if (func ==1 ) {
    buf = strtok(NULL,"\n");
    if (buf == NULL) { free(new_pat); return -1;}
    buf += strspn(buf,delim);
    if (*buf == '"') {
      char *buf2;
      buf ++;
      buf2 = rindex(buf,'"');
      if(buf2) *buf2='\0';
    }
    if (*buf=='\0') { free(new_pat); return -4;}

    /* le 6eme champ : regexp */
    res = regcomp(&new_pat->regexp,buf,REG_EXTENDED|
      ((new_pat->flags&HIGH_FLAGS_LINE)?REG_NOSUB:0) |
      ((new_pat->flags&HIGH_FLAGS_CASE)?0:REG_ICASE));
    if (res !=0) {
      free(new_pat); return -4;
    }
    /* On est presque arrivé... Juste le cas des sous-exp à traiter... */
    if (!(new_pat->flags&HIGH_FLAGS_LINE) && 
    		(new_pat->pat_num>(new_pat->regexp).re_nsub)) {
      regfree(&new_pat->regexp); free(new_pat); return -6;
    }
    /*
    new_pat->colors.attributs_mono=new_pat->colors.attributs;
    */ /* Je veux pouvoir avoir des trucs différents grace a std-... */
    if (highlight_first) {
      highlight_last->next=new_pat;
      highlight_last=new_pat;
    } else {
      highlight_first = highlight_last=new_pat;
    }
  } else {
    for (i=0;i<NROF_FIELDS;i++) {
      if (new_pat->field_mask & (1<<i)) {
        for (i=0;i<NROF_FIELDS;i++)
	  if (new_pat->field_mask & (1<<i)) 
	  {
	    if (func==2) {
	      strncpy(Colors[i].fg,new_pat->colors.fg,14);
	      strncpy(Colors[i].bg,new_pat->colors.bg,14);
	      Colors[i].attributs= new_pat->colors.attributs;
	    } else {
	      Colors[i].attributs_mono= new_pat->colors.attributs_mono;
	    }
	  }
      }
    }
    free(new_pat);
  }
  return 0;
}

void new_init_color() {
  int field=START_COLOR_NUM;
  struct Highlight *current=highlight_first;

  if (Options.color==0) Set_Use_Ansi_Colors(0);
  else if (Options.color==1) Set_Use_Ansi_Colors(1);
  while(current) {
    if (Get_Use_Ansi_Colors()) {
       Color_set(field, NULL, current->colors.fg, current->colors.bg);
       Color_add_attribute(field, current->colors.attributs);
    }
    else
       Mono_set(field, NULL, current->colors.attributs_mono);
    current->col_num = field;
    current=current->next; field++;
  }
}

/* Initialise les couleurs */
void Init_couleurs() {
   int field;

   new_init_color(); /* on initialise les nouvelles couleurs */
   if (Options.color==0) Set_Use_Ansi_Colors(0);
   else if (Options.color==1) Set_Use_Ansi_Colors(1);
   for (field=0; field<NROF_FIELDS; field++)
     if (Get_Use_Ansi_Colors()) {
        Color_set(field, NULL, Colors[field].fg, Colors[field].bg);
        Color_add_attribute(field, Colors[field].attributs);
     }
     else
        Mono_set(field, NULL, Colors[field].attributs_mono);
}


/* affiche une ligne en couleur
 * to_print dit s'il faut afficher
 * la ligne est copiée dans format_line si format_line != NULL
 * field est le type de texte, line la ligne, len la longueur
 * bol=1 si on est au début de la ligne (ie on a le droit à un match de ligne)
 * def_color est la couleur si rien ne match
 * on retourne la couleur de base de la ligne, a repasser si on la continue */
/* high est une liste {color,prefix_len,match_len} des patterns a considérer */
int raw_Aff_color_line(int to_print, unsigned short *format_line,
    int *format_len, int field, char *line, int len, int bol, int def_color,
    int *high, int fill_to_len) {
  regmatch_t pmatch[REG_MAX_SUB];
  int prefix_len=len;	/* le nombre de caractères avant la zone a colorer */
  int match_len=0;	/* la taille de la zone a colorer */
  int color=-1;		/* la couleur pour la zone */
  int color_line=def_color; /* la couleur du reste de la ligne */
  struct Highlight *current=highlight_first;
  int excl_line=0;	/* a 1 : ne pas highlighter la zone */
  int matched_line=0;	/* on a matché une regexp de ligne */
  int high_num=0;
  int high_allocated=0;

  if (high==NULL) {
    high=safe_malloc(sizeof(int)*3*len);
    *high=-1;
    high_allocated=1;
  }
  for(high_num=0; high[high_num*3] !=-1; high_num++);
  if (high_num) {
    high_num--;
    color=high[high_num*3];
    prefix_len=high[high_num*3+1];
    match_len=high[high_num*3+2];
  }

  /* regarde si la chaine est plus courte que len */
  if (!fill_to_len)
  {
    int i;
    for(i=0;i<len && line[i];i++);
    len=i;
    prefix_len=i;
  }

  while((prefix_len > 0) && current) {
    /* est-ce que la regexp match et field est bon ? */
    if ((current->field_mask & (1<<field)) &&
	(regexec(&current->regexp,line,REG_MAX_SUB,pmatch,bol?0:REG_NOTBOL)==0))
    {
      if (current->flags & HIGH_FLAGS_LINE) {
	/* on garde que le premier match ligne */
	if ((!matched_line) && (bol)) {
	  color_line=current->col_num;
	  excl_line = (current->flags & HIGH_FLAGS_EXCLUDE)?1:0;
	  matched_line=1;
	}
      } else {
	/* on garde un autre match si il est entièrement avant */
	if ((pmatch[current->pat_num].rm_so != -1) &&
	    (((color==-1) && (pmatch[current->pat_num].rm_so <= prefix_len))
	    || (pmatch[current->pat_num].rm_eo <= prefix_len)
	    || (match_len==0))) {
	  if(color != -1) {
	    high[high_num*3]=color;
	    high[high_num*3+1]=prefix_len;
	    high[high_num*3+2]=match_len;
	    high_num++;
	  }
	  color=current->col_num;
	  prefix_len= pmatch[current->pat_num].rm_so;
	  match_len = ((pmatch[current->pat_num].rm_eo <len)?
	      pmatch[current->pat_num].rm_eo:len) - prefix_len;
	  /* Dans le cas où prefix_len et match_len sont nuls, on "triche" */
	  /* en mettant prefix_len à 1 */
	  /* On fait ensuite une modif sur certains tests pour arranger les */
	  /* choses */
	  if ((match_len==0) && (prefix_len==0)) prefix_len=1;
	}
      }
    }
    current=current->next;
  }
  /* s'il y avait le flag EXCLUDE sur la ligne, on ignore les autre matchs */
  if (excl_line) prefix_len = len;
  /* il faut afficher */
  if (to_print) {
    Screen_set_color(color_line);
    Screen_write_nstring(line,prefix_len);
    if (prefix_len < len) {
      if (color != -1);
	Screen_set_color(color);
      Screen_write_nstring(line+prefix_len,match_len);
    }
  }
  /* on stoque la ligne */
  if (format_len) *format_len=0;
  if (format_line) {
    int i;
    for (i=0;i<prefix_len && line[i];i++) {
      format_line[i]=(color_line<<8) + (unsigned char) line[i];
    }
    for (;i<prefix_len+match_len && line[i];i++) {
      format_line[i]=(color<<8) + (unsigned char) line[i];
    }
    for (;i<prefix_len+match_len ;i++) {
      format_line[i]=(color<<8) + ' ';
    }
    if(format_len) *format_len=i;
  }

  /* on n'est pas au bout, donc on se rappelle */
  if (prefix_len + match_len < len) {
    int add=0;
    {
       int i;
       for(i=0;i<high_num;i++) {
	 high[i*3+1] -= prefix_len+match_len;
       }
    }
    high[high_num*3]=-1;
    color_line = raw_Aff_color_line(to_print,
	(format_line)?(format_line+prefix_len+match_len):NULL, &add,
	field, line+prefix_len+match_len,
	len - prefix_len - match_len, 0,color_line,high,1);
    if(format_len) *format_len += add;
  }
  if(high_allocated)
    free(high);
  /* on a fini ! */
  return color_line;
}

int Aff_color_line(int to_print, unsigned short *format_line, int *format_len,
    int field, char *line, int len, int bol, int def_color) {
  return raw_Aff_color_line(to_print,format_line,format_len,field,line,len,
      bol,def_color,NULL,0);
}

unsigned short *cree_chaine_mono (const char *str, int field, int len) {
   int i;
   unsigned short *toto;

   if (len==-1) len=strlen(str);
   toto=safe_malloc(len*sizeof(short));
   for (i=0;i<len;i++) toto[i]=(field<<8) + (unsigned char)str[i];
   return toto;
}

/* Tout pour l'écriture du .flrnrc :-( */
void ecrit_attributs(FILE *file, FL_Char_Type att) {
  int comma=0;
  if (att==0) {
    fprintf(file," normal");
    return;
  }
  if (att & FL_BOLD_MASK) {
    fprintf(file,"%c%s",(comma ? ',' : ' '),bold_str);
    comma=1;
  }
  if (att & FL_BLINK_MASK) {
    fprintf(file,"%c%s",(comma ? ',' : ' '),blink_str);
    comma=1;
  }
  if (att & FL_ULINE_MASK) {
    fprintf(file,"%c%s",(comma ? ',' : ' '),underline_str);
    comma=1;
  }
  if (att & FL_REV_MASK) {
    fprintf(file,"%c%s",(comma ? ',' : ' '),reverse_str);
    comma=1;
  }
}

/* FIXME: virer ce code, et/ou relire le .flrn à ce moment pour essayer
 * de faire quelque chose de plus astucieux */
void dump_colors_in_flrnrc (FILE *file) {
  int i,comma,mask;
  struct Highlight *current=highlight_first;

  fprintf(file,_("# Couleurs des champs généraux :"));
  for (i=0;i<NROF_FIELDS;i++) {
    fprintf(file,"\ncolor %s %s %s",Field_names[i],Colors[i].fg,Colors[i].bg);
    ecrit_attributs(file,Colors[i].attributs);
    fprintf(file,"\nmono %s",Field_names[i]);
    ecrit_attributs(file,Colors[i].attributs);
  }
  fprintf(file,_("\n# Les regcolors, dans la mesure du possible, ça marche pas toujours..."));
  /* on ne copie pas les std-bla, donc on peu avoir de GROS problèmes */
  while (current) {
    comma=0;
    mask=current->field_mask;
    fprintf(file,"\nregcolor");
    if (mask==~0) fprintf(file," all");
    else
    for (i=0;i<NROF_FIELDS;i++) {
      if (mask%2) {
       fprintf(file,"%c%s",(comma ? ',' : ' '),Field_names[i]);
       comma=1;
      }
      mask>>=1;
    }
    comma=0;
    if (current->pat_num) {
       fprintf(file," %d",current->pat_num);
       comma=1;
    }
    mask=current->flags;
    if (mask==0) {
      if (current->pat_num==0) fprintf(file," -");
    } else
    for (i=0;i<NUM_FLAGS;i++) {
      if (mask%2) {
        fprintf(file,"%c%s",(comma ? ',' : ' '),flags_names[i]);
	comma=1;
      }
      mask>>=1;
    }
    fprintf(file," %s %s",current->colors.fg,current->colors.bg);
    ecrit_attributs(file,
    		current->colors.attributs & current->colors.attributs_mono);
		/* LA est le problème */
    fprintf(file," une_regexp_iconnue...");
    current=current->next;
  }
  fprintf(file,_("\n\nVoila, c'est fini...\n"));
}


unsigned short *Search_in_line (unsigned short *line, char *out, long len, 
				regex_t *sreg) {
   char *ptr=out;
   int ret=0, bol=1, i;
   regmatch_t pmatch[1];
   unsigned short *result=NULL, *ptr2=line, *ptr3=NULL;
   
   while ((ret==0) && (len!=0)) {
     ret=regexec(sreg, ptr, 1, pmatch, bol ? 0 : REG_NOTBOL);
     if (ret==0) {
        if (bol) {
	   result=safe_malloc(sizeof(unsigned int)*len);
	   ptr3=result;
	}
	for (i=0;i<pmatch[0].rm_so;i++) {
	   *(ptr3++)=*(ptr2++);
	   ptr++;
	}
	for (;i<pmatch[0].rm_eo;i++) {
	   *(ptr3++)=((unsigned char)(*ptr++))+(FIELD_SEARCH<<8);
	   ptr2++;
	}
	if (pmatch[0].rm_eo==0) { /* un match vide :-( */
	   *(ptr3++)=*(ptr2++); ptr++; len--;
	}
	len-=pmatch[0].rm_eo;
     }
     bol=0;
   }
   if (result) { /* On recopie la fin de la chaine*/
      for (;*ptr;ptr++) 
         *(ptr3++)=*(ptr2++);
   }
   return result;
}
