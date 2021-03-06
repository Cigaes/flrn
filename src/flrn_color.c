/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
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
#include "enc_strings.h"

static char UNUSED rcsid[]="$Id$";

/* I want to use this code and the old one together */
#define START_COLOR_NUM 13
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

/* les couleurs par d�faut des champs */
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
	      {"red","default",0,0},  /* at_mine */
	      {"default","default",0,0},  /* at_other */
	    };



/* le d�limiteur pour les options */
static flrn_char *delim = fl_static("=: \t\n");

/* une limite sur le num�ro de () dans les regexp */
#define REG_MAX_SUB 10

/* la structure pour un objet a highlighter */
struct Highlight {
  regex_t regexp;
  int high_flags;
  struct Obj_color_struct colors; /* la couleur � utiliser */
  int pat_num;		/* le num�ro du pattern de la regexp a colorier */
  int col_num;		/* le num�ro de la couleur */
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
static char *high_flags_names[]={"exclude","line","case"};

/* les "couleurs" */
static const char *bold_str="bold";
static const char *blink_str="blink";
static const char *underline_str="underline";
static const char *reverse_str="reverse";

/* vire tout, lib�re la m�moire */
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
int parse_option_color(int func, flrn_char *line)
{
  flrn_char *buf;
  flrn_char *buf2;
  flrn_char *point;
  int i;
  struct Highlight *new_pat;
  int res;
  int field=0; /* le champ pour la couleur std */

  buf = fl_strtok_r(line,delim,&point);
  if (buf == NULL) return -1;
  if (fl_strcasecmp(buf,fl_static("clear"))==0) {
    free_highlights();
    return 0;
  }
  /* si on n'en a pas besoin, on le lib�reer plus tard */
  new_pat = safe_calloc(1,sizeof(struct Highlight));
  /* le premier champ, c'est field[,field] */
  /* on n'a pas de controle d'erreur pertinent... */
  while(buf) {
    buf2=fl_strchr(buf,',');
    if (buf2) {*buf2=0; buf2++;}
    for (i=0;i<NROF_FIELDS;i++) {
      if (fl_strcasecmp(buf,fl_static_tran(Field_names[i]))==0) {
	new_pat->field_mask |= 1<<i; 
	field=i;
	break;
      }
    }
    if (i==NROF_FIELDS) {
      if (fl_strcasecmp(buf,fl_static("-")) && 
	      fl_strcasecmp(buf,fl_static("all"))) {
	free(new_pat);
	return -2;
      }
    }
    buf=buf2;
  }
  if (new_pat->field_mask == 0) { new_pat->field_mask = ~0;}

  buf = fl_strtok_r(NULL,delim,&point);
  if (func == 1) {
    if (buf == NULL) { free(new_pat); return -1;}
    /* le deuxi�me champ, ce sont les flags */
    for (i=0;i<NUM_FLAGS;i++) {
      if (fl_strstr(buf,fl_static_tran(high_flags_names[i])))
	  new_pat->high_flags |= 1<<i; 
    }
    if ((buf2=fl_strpbrk(buf,fl_static("0123456789")))) {
      new_pat->pat_num = fl_strtol(buf2,NULL,10);
      if (new_pat->pat_num >= REG_MAX_SUB) { free(new_pat); return -3; }
    } else new_pat->pat_num=0;
    buf = fl_strtok_r(NULL,delim,&point);
  }
  if (func !=3) {
    if (buf == NULL) { free(new_pat); return -1;}
    /* le 2/3eme champ : fg */
    flraw_strncpy(new_pat->colors.fg, buf, 14);
    buf = fl_strtok_r(NULL,delim,&point);
    if (buf == NULL) { free(new_pat); return -1;}
    /* le 3/4eme champ : bg */
    flraw_strncpy(new_pat->colors.bg, buf, 14);
    buf = fl_strtok_r(NULL,delim,&point);
    if ((i=parse_std(new_pat->colors.fg,field)) >=0 )
      strncpy(new_pat->colors.fg,Colors[i].fg,14);
    if ((i=parse_std(new_pat->colors.bg,field)) >=0 )
      strncpy(new_pat->colors.bg,Colors[i].bg,14);
  }
  /* le 2/4/5eme champ : attr */
  while(buf) {
    buf2=fl_strchr(buf,fl_static(','));
    if (buf2) {*buf2=fl_static('\0'); buf2++;}
    if ((i=parse_std(fl_static_rev(buf),field)) >=0 ) {
      new_pat->colors.attributs|=Colors[i].attributs;
      new_pat->colors.attributs_mono|=Colors[i].attributs_mono;
    }
    else
    if (fl_strcasecmp(buf,fl_static_tran(bold_str))==0) {
      new_pat->colors.attributs|=FL_BOLD_MASK;
      new_pat->colors.attributs_mono|=FL_BOLD_MASK;
    }
    else
    if (fl_strcasecmp(buf,fl_static_tran(blink_str))==0) {
      new_pat->colors.attributs|=FL_BLINK_MASK;
      new_pat->colors.attributs_mono|=FL_BLINK_MASK;
    }
    else
    if (fl_strcasecmp(buf,fl_static_tran(underline_str))==0) {
      new_pat->colors.attributs|=FL_ULINE_MASK;
      new_pat->colors.attributs_mono|=FL_ULINE_MASK;
    }
    else
    if (fl_strcasecmp(buf,fl_static_tran(reverse_str))==0) {
      new_pat->colors.attributs|=FL_REV_MASK;
      new_pat->colors.attributs_mono|=FL_REV_MASK;
    }
    else
    if (fl_strcasecmp(buf,fl_static("-")) && 
	    fl_strcasecmp(buf,fl_static("normal")))
      return -5;
    buf=buf2;
  }

  if (func ==1 ) {
    buf = fl_strtok_r(NULL,fl_static("\n"),&point);
    if (buf == NULL) { free(new_pat); return -1;}
    buf += fl_strspn(buf,delim);
    if (*buf == fl_static('"')) {
      char *buf2;
      buf ++;
      buf2 = fl_strrchr(buf,fl_static('"'));
      if(buf2) *buf2=fl_static('\0');
    }
    if (*buf==fl_static('\0')) { free(new_pat); return -4;}

    /* le 6eme champ : regexp */
    res = fl_regcomp(&new_pat->regexp,buf,REG_EXTENDED|
      ((new_pat->high_flags&HIGH_FLAGS_LINE)?REG_NOSUB:0) |
      ((new_pat->high_flags&HIGH_FLAGS_CASE)?0:REG_ICASE));
    if (res !=0) {
      free(new_pat); return -4;
    }
    /* On est presque arriv�... Juste le cas des sous-exp � traiter... */
    if (!(new_pat->high_flags&HIGH_FLAGS_LINE) && 
    		(new_pat->pat_num>(new_pat->regexp).re_nsub)) {
      fl_regfree(&new_pat->regexp); free(new_pat); return -6;
    }
    /*
    new_pat->colors.attributs_mono=new_pat->colors.attributs;
    */ /* Je veux pouvoir avoir des trucs diff�rents grace a std-... */
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


/* Recherche les patterns pour la cr�ation d'une ligne en couleur.
 * add_line_fun sert � envoyer un r�sultat
 * field est le type de texte, line la ligne, len sa longueur
 * on est toujours au d�but de la ligne.
 * def_color : couleur du non_match
 * flag  : 1 -> se limiter � une ligne
 */
struct High_Match {
    int pre_len;
    int mat_len;
    struct Highlight *cur;
};

int create_Color_line (add_line_fun add_line,
	int field, flrn_char *line, size_t len, int def_color) {
    regmatch_t pmatch[REG_MAX_SUB];
/*    wflrn_char *wide_line=NULL;  */
    int color_line=def_color; /* la couleur du reste de la ligne */
    struct Highlight *current=highlight_first,*cur=NULL;
    int excl_line=0;	/* a 1 : ne pas highlighter la zone */
    int matched_line=0;	/* on a match� une regexp de ligne */
    int prefix_len=len,passe=0; /* la zone d'avant tous les matchs */
    int match_len=0;	/* la taille de la zone a colorer */
    struct High_Match *high;
    struct High_Match *final;
    int high_num,final_num,i;
    flrn_char *pline=line;

    high=safe_malloc(sizeof(struct High_Match)*(len+1));
    final=safe_malloc(sizeof(struct High_Match)*(len+1));
    high_num=0;
    final_num=0;

    /* une premi�re chose � faire est de transcrire la ligne
     * en wflrn_char, parce que ce n'est qu'avec �a qu'on a la bonne
     * longueur */
/*    flstring_to_widestr(line,len,&wide_line,&wl_len);  */
    while ((len>0) && (current)) {
       /* est-ce que la regexp match et field est bon ? */
       if ((current->field_mask & (1<<field)) &&
	  (fl_regexec(&current->regexp,pline,REG_MAX_SUB,
		   pmatch,(pline==line)?0:REG_NOTBOL)==0))
       {
          if (current->high_flags & HIGH_FLAGS_LINE) {
	/* on garde que le premier match ligne */
	    if ((!matched_line) && (pline==line)) {
	       color_line=current->col_num;
	       excl_line = (current->high_flags & HIGH_FLAGS_EXCLUDE)?1:0;
	       if (excl_line) break;
	       matched_line=1;
	    }
          } else {
	  /* on garde un autre match si il est enti�rement avant */
	     if ((pmatch[current->pat_num].rm_so != -1) &&
	        (((cur==NULL) && (pmatch[current->pat_num].rm_so <= prefix_len))
	         || (pmatch[current->pat_num].rm_eo <= prefix_len)
	         || (match_len==0))) {
	        if(cur != NULL) {
	           high[high_num].pre_len=prefix_len+passe;
	           high[high_num].mat_len=match_len;
	           high[high_num].cur=cur;
	           high_num++;
	        }
	        cur=current;
	        prefix_len= pmatch[current->pat_num].rm_so;
	        match_len = ((pmatch[current->pat_num].rm_eo <len)?
	            pmatch[current->pat_num].rm_eo:len) - prefix_len;
	  /* Dans le cas o� prefix_len et match_len sont nuls, on "triche" */
	  /* en mettant prefix_len � 1 */
	  /* On fait ensuite une modif sur certains tests pour arranger les */
	  /* choses */
	        if ((match_len==0) && (prefix_len==0)) prefix_len=1;
	     }
          }
       }
       current=current->next;
       if (current==NULL) {
	   /* OK */
	   if (cur==NULL) break; /* rien n'a match� du tout */
	   if (match_len>0) {
	      final[final_num].pre_len=prefix_len+passe;
	      final[final_num].mat_len=match_len;
	      final[final_num].cur=cur;
	      final_num++;
	   }
	   len-=(prefix_len+match_len);
	   passe+=(prefix_len+match_len);
	   pline+=(prefix_len+match_len);
	   high_num--;
	   if (high_num>=0) {
	       cur=high[high_num].cur;
	       prefix_len=high[high_num].pre_len-passe;
	       match_len=high[high_num].mat_len;
	       current=cur->next; /* en toute logique, cur->next != NULL */
	   } else {
               cur=NULL;
	       prefix_len=len;
	       current=highlight_first;
	       high_num=0;
	   }
       }
    }
    /* si j'ai bien fait les choses, on a tout ce qu'il faut dans final */
    free(high);
    /* Dans le cas o� on a match� un "exclude", on met final_num � 0 */
    if (excl_line) final_num=0;
    pline=line;
    len+=passe;
    passe=0;
    for (i=0;i<final_num;i++) {
        if (final[i].pre_len>passe)
	    add_line(pline,final[i].pre_len-passe,color_line);
	passe=final[i].pre_len;
	pline=line+passe;
	if (final[i].mat_len>0)
	    add_line(pline,final[i].mat_len,(final[i].cur)->col_num);
	passe+=final[i].mat_len;
	pline+=final[i].mat_len;
    }
    add_line(pline,len-passe,color_line);
    free(final);
    return color_line;
}
    


#if 0
/* affiche une ligne en couleur
 * to_print dit s'il faut afficher
 * la ligne est copi�e dans format_line si format_line != NULL
 * field est le type de texte, line la ligne, len la longueur
 * bol=1 si on est au d�but de la ligne (ie on a le droit � un match de ligne)
 * def_color est la couleur si rien ne match
 * on retourne la couleur de base de la ligne, a repasser si on la continue */
/* high est une liste {color,prefix_len,match_len} des patterns a consid�rer */
int raw_Aff_color_line(int to_print, unsigned short *format_line,
    int *format_len, int field, flrn_char *line, int len, int bol,
    int def_color, int *high, int fill_to_len) {
  regmatch_t pmatch[REG_MAX_SUB];
  int prefix_len=len;	/* le nombre de caract�res avant la zone a colorer */
  int match_len=0;	/* la taille de la zone a colorer */
  int color=-1;		/* la couleur pour la zone */
  int color_line=def_color; /* la couleur du reste de la ligne */
  struct Highlight *current=highlight_first;
  int excl_line=0;	/* a 1 : ne pas highlighter la zone */
  int matched_line=0;	/* on a match� une regexp de ligne */
  int high_num=0;
  int high_allocated=0;

  if (high==NULL) {
    high=safe_malloc(sizeof(int)*3*(len+1));
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
	/* on garde un autre match si il est enti�rement avant */
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
	  /* Dans le cas o� prefix_len et match_len sont nuls, on "triche" */
	  /* en mettant prefix_len � 1 */
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
#endif

Colored_Char_Type *cree_chaine_mono (const char *str, int field, size_t len,
	size_t *nbcct) {
   size_t j;
   const char *buf=str;
   Colored_Char_Type *toto;

   if (len==(size_t)-1) len=strlen(str);
   toto=safe_malloc(len*sizeof(Colored_Char_Type));
   for (j=0;(*buf) && (buf-str<len);j++) 
       toto[j]=create_Colored_Char_Type(&buf,field);
   *nbcct=j;
   return toto;
}

/* Tout pour l'�criture du .flrnrc :-( */
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

/* FIXME: virer ce code, et/ou relire le .flrn � ce moment pour essayer
 * de faire quelque chose de plus astucieux */
void dump_colors_in_flrnrc (FILE *file) {
  int i,comma,mask;
  struct Highlight *current=highlight_first;

  fprintf(file,"# Couleurs des champs g�n�raux :");
  for (i=0;i<NROF_FIELDS;i++) {
    fprintf(file,"\ncolor %s %s %s",Field_names[i],Colors[i].fg,Colors[i].bg);
    ecrit_attributs(file,Colors[i].attributs);
    fprintf(file,"\nmono %s",Field_names[i]);
    ecrit_attributs(file,Colors[i].attributs);
  }
  fprintf(file,"\n# Les regcolors, dans la mesure du possible, �a marche pas toujours...");
  /* on ne copie pas les std-bla, donc on peu avoir de GROS probl�mes */
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
    mask=current->high_flags;
    if (mask==0) {
      if (current->pat_num==0) fprintf(file," -");
    } else
    for (i=0;i<NUM_FLAGS;i++) {
      if (mask%2) {
        fprintf(file,"%c%s",(comma ? ',' : ' '),high_flags_names[i]);
	comma=1;
      }
      mask>>=1;
    }
    fprintf(file," %s %s",current->colors.fg,current->colors.bg);
    ecrit_attributs(file,
    		current->colors.attributs & current->colors.attributs_mono);
		/* LA est le probl�me */
    fprintf(file," une_regexp_iconnue...");
    current=current->next;
  }
  fprintf(file,"\n\nVoila, c'est fini...\n");
}

size_t Transpose_flchars_on_colchars(flrn_char **flstr, Colored_Char_Type 
	**ccstr, size_t len, int field, size_t rst) {
    int resconv;	
    char *bla=NULL;
    resconv=conversion_to_terminal(*flstr,&bla,0,len);
    rst=Transpose_Colored_String(ccstr, bla, field, rst);
    if (resconv==0) free(bla);
    if (len==(size_t)(-1)) *flstr+=fl_strlen(*flstr);
        else *flstr+=len;
    return rst;
}

Colored_Char_Type *Search_in_line (Colored_Char_Type *line,
	flrn_char *out, size_t len, regex_t *sreg) {
   flrn_char *ptr=out;
   int ret=0, bol=1;
   regmatch_t pmatch[1];
   Colored_Char_Type *result=NULL, *ptr3=NULL;
   size_t rst=0;
   
   while ((ret==0) && (*ptr)) {
     ret=fl_regexec(sreg, ptr, 1, pmatch, bol ? 0 : REG_NOTBOL);
     if (ret==0) {
        if (bol) {
	   result=safe_malloc(sizeof(Colored_Char_Type)*len);
	   memcpy(result,line,sizeof(Colored_Char_Type)*len);
	   ptr3=result;
	   rst=len;
	}
	rst-=Transpose_flchars_on_colchars(&ptr,&ptr3,pmatch[0].rm_so,-1,rst);
	if (pmatch[0].rm_eo!=0) {
	  rst-=Transpose_flchars_on_colchars(&ptr,&ptr3,
		pmatch[0].rm_eo-pmatch[0].rm_so,FIELD_SEARCH,rst);
	} else 
	  rst-=Transpose_flchars_on_colchars(&ptr,&ptr3,1,-1,rst);
     }
     bol=0;
   }
   if (result)
       rst-=Transpose_flchars_on_colchars(&ptr,&ptr3,(size_t)(-1),-1,rst);
   return result;
}
