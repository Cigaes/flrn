/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2000  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_slang.c : liens avec les fonction d'E/S de slang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/*  Liens avec les fonctions slang					   */
/*  Ce fichier ne devrait pas être indispensable... si on passe à ncurses  */

#include <stdio.h>
#include <stdlib.h>
#include <slang.h>
#include <ctype.h>
#include "flrn.h"
#include "flrn_slang.h"
#include "flrn_color.h"
#include "enc/enc_strings.h"
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif


static UNUSED char rcsid[]="$Id$";

int KeyBoard_Quit;
int Screen_Rows, Screen_Cols, Screen_Rows2;
int Screen_Tab_Width;

void Screen_suspend() { SLsmg_suspend_smg(); }
void Screen_resume() { SLsmg_resume_smg(); }
void Screen_get_size(int hl) { SLtt_get_screen_size(); 
			 Screen_Rows=SLtt_Screen_Rows-hl;
			 Screen_Rows2=SLtt_Screen_Rows;
			 Screen_Cols=SLtt_Screen_Cols;
		       }
int Screen_init_smg() { int a; a=SLsmg_init_smg(); Screen_Tab_Width=SLsmg_Tab_Width; return a; }
void Screen_reset() {  SLsmg_reset_smg(); }

void Get_terminfo(int hl) { SLtt_get_terminfo();
		      Screen_Rows=SLtt_Screen_Rows-hl;
		      Screen_Rows2=SLtt_Screen_Rows;
		      Screen_Cols=SLtt_Screen_Cols;
		    }
void Set_term_cannot_scroll (int a) { SLtt_Term_Cannot_Scroll=a; }
void Screen_write_char(char c) { SLsmg_write_char(c); }
void Screen_write_string(char *s) { SLsmg_write_string(s); }
void Screen_write_nstring(char *s, int n) { SLsmg_write_nstring(s,n); }
void Screen_write_color_chars(Colored_Char_Type *s, unsigned int n) 
		{ SLsmg_write_color_chars(s,n); }
void Cursor_gotorc(int r, int c) { SLsmg_gotorc(r,c); }
void Screen_erase_eol()	{ SLsmg_erase_eol(); }
void Screen_erase_eos()	{ SLsmg_erase_eos(); }
void Screen_refresh() { SLsmg_refresh(); }
void Screen_touch_lines (int d, unsigned int n) { SLsmg_touch_lines(d,n); }

void Set_Use_Ansi_Colors(int n) { SLtt_Use_Ansi_Colors=n; }
int  Get_Use_Ansi_Colors() { return SLtt_Use_Ansi_Colors; }

void Color_set(int n, char *s1, char *s2, char *s3)
	{ SLtt_set_color(n,s1,s2,s3); }
void Color_add_attribute(int n, FL_Char_Type a)
	{ SLtt_add_color_attribute(n,a); }
void Mono_set (int n, char *s, FL_Char_Type a)
	{ SLtt_set_mono (n,s,a); }

void Screen_set_color(int n) { SLsmg_set_color(n); }

void set_Display_Eight_Bit(int n) { SLsmg_Display_Eight_Bit = n; }

/* Regestion du clavier */
int Put_tty_single_input (int a, int b, int c) { return SLang_init_tty(a,b,c); }
void React_suspend_char (int a) { SLtty_set_suspend_state(a); }
int init_clavier() { return SLkp_init(); }
void Set_Ignore_User_Abort (int n) { SLang_Ignore_User_Abort=n; }
int Get_Ignore_User_Abort () { return SLang_Ignore_User_Abort; }
int Set_Abort_Signal(void (*a)(int)) 
{ 
  SLang_set_abort_signal(a); 
  return 0; /* selon versions de Slang, la fonction peut revoyer un entier */
}
int Keyboard_getkey() { int n; SLKeyBoard_Quit=KeyBoard_Quit;
  			n=SLkp_getkey(); 
  			KeyBoard_Quit=SLKeyBoard_Quit; return n; }
void Reset_keyboard() { SLang_reset_tty(); }
void Screen_beep() { SLtt_beep(); }
void Screen_set_screen_start(int *r, int *c) { SLsmg_set_screen_start(r,c); };



/**************************************************************************/
/**************************************************************************/
/*   Gestion des scrolls ... cf pager.c dans les demos de slang.         **/
/**************************************************************************/

/* Fenetre de scroll */
File_Line_Type *Text_scroll=NULL;
/* on s'inspire de slang, amis on change les types et on simplifie */
typedef struct
{
  unsigned int swt_flags;
  File_Line_Type *top_window_line; /* list element at top of window */
  File_Line_Type *lines;           /* first list element */
  unsigned int nrows;             /* number of rows in window */
  unsigned int line_num;          /* current line number (visible) */
  unsigned int num_lines;         /* total number of lines (visible) */
  unsigned int window_row;        /* row of current_line in window */
}
Scroll_Window_Type;

/* Pattern du scroll */
static regex_t *regexp_scroll=NULL;

static Scroll_Window_Type Line_Window; /* On va essayer de limiter */
                                /* l'usage de cette variable a ce fichier */

void free_text_scroll() {
     File_Line_Type *line, *next;
     Line_Elem_Type *lline, *lnext;
     
     line=Text_scroll;
     while (line != NULL) {
        next=line->next;
	lline=line->line;
	while (lline != NULL) {
	    lnext=lline->next;
            if (lline->data != NULL) free (lline->data);
            if (lline->data_save != NULL) free (lline->data_save);
	    if ((lline->data_base != NULL) 
		    && (lline->base_mlen!=0)) free (lline->data_base);
	    free(lline);
	    lline=lnext;
	}
        free(line);
        line=next;
     }
     Text_scroll=NULL;
     if (regexp_scroll) {
        regfree(regexp_scroll);
        free(regexp_scroll);
     }
     regexp_scroll=NULL;
}

/* fonction de compatibilité... */
static File_Line_Type *ajoute_line (File_Line_Type *lini,
	flrn_char *buf, char *cbuf, 
	size_t data_len, size_t base_len, int flag,
	int col, int field) {
   File_Line_Type *line=lini;
   Line_Elem_Type *lline;
   int resconv,i;
   char *trad=NULL;
     
   if (line==NULL) {
     line = (File_Line_Type *) safe_calloc (1,sizeof (File_Line_Type));
     lline = (Line_Elem_Type *) safe_calloc (1,sizeof (Line_Elem_Type));
     line->line=lline;
   } else {
       lline=line->line;
       if (lline) {
	   while (lline->next) lline=lline->next;
	   lline->next=
	       (Line_Elem_Type *) safe_calloc (1,sizeof (Line_Elem_Type));
	   lline=lline->next;
       } else 
	   lline=(Line_Elem_Type *) safe_calloc (1,sizeof (Line_Elem_Type));
   }
/*   line->data = SLmake_string (buf); */  /* use a slang routine */
   if (cbuf==NULL) {
       resconv=conversion_to_terminal(buf,&trad,0,(size_t)(-1));
   } else {
       trad=cbuf;
       resconv=1;
   }
   if (data_len==0) data_len=strlen(trad);
   lline->data = safe_calloc(data_len+1,sizeof(Colored_Char_Type));
   lline->data_mlen=data_len;
   i=0;
   {
     const char *ptr=trad;
     while ((*ptr) && (i<data_len)) {
       lline->data[i++]=create_Colored_Char_Type(&ptr,field);
     }
   }
   lline->data_len=i;
   if (resconv==0) free(trad);
   if (flag) {
       lline->data_base=buf;
       lline->base_mlen=0;
   } else 
   if (base_len==0) {
       lline->data_base=safe_flstrdup(buf);
       lline->base_mlen=fl_strlen(buf);
   } else {
       lline->data_base=safe_calloc(base_len+1,sizeof(flrn_char));
       fl_strncpy(lline->data_base,buf,base_len);
       lline->base_mlen=base_len;
   }
   lline->base_len=fl_strlen(buf);
   lline->col=col;
   return line;
}

File_Line_Type *Ajoute_formated_line (flrn_char *flbuf, char *buf,
	size_t flbuf_len, size_t buflen, int flag, int col, int field,
	int nm, File_Line_Type *ini) {
    int i;
    File_Line_Type *line;
    File_Line_Type *created=ajoute_line
	(ini, flbuf, buf, flbuf_len, buflen, flag, col, field);
    if (created==NULL) return NULL;
    if (ini==NULL) {
	Line_Window.num_lines++;
	if ((Text_scroll==NULL) || (nm==0)) {
	    created->next=Text_scroll;
	    if (Text_scroll) Text_scroll->prev=created;
	    Text_scroll=created;
	    return Text_scroll;
	}
	line=Text_scroll;
	i=1;
	while ((line->next) && ((i<nm) || (nm==-1))) { line=line->next; i++; }
	created->prev=line;
	created->next=line->next;
	if (line->next) line->next->prev=created;
	line->next=created;
    }
    return created;
}
	


/* on fait des mallocs. mlen>=len et mfllen>=fllen... */
static File_Line_Type *create_color_line (Colored_Char_Type *buf,
	flrn_char *flbuf, size_t len, size_t mlen, 
	size_t fllen, size_t mfllen, int col) {
   File_Line_Type *line;
   Line_Elem_Type *lline;
     
   line = (File_Line_Type *) safe_calloc (1,sizeof (File_Line_Type));
   lline = (Line_Elem_Type *) safe_calloc (1,sizeof (Line_Elem_Type));
   line->line=lline;
   lline->data = safe_calloc((mlen+1),sizeof(Colored_Char_Type));
   lline->data_base = safe_calloc((mfllen+1),sizeof(Colored_Char_Type));
   lline->data_mlen=mlen;
   lline->base_mlen=mfllen;
   if (buf) memcpy(lline->data,buf,sizeof(Colored_Char_Type) * len);
   lline->data[len]=(Colored_Char_Type)0;
   lline->data_len=len;
   if (flbuf) memcpy(lline->data_base,flbuf,sizeof(flrn_char) * fllen);
   lline->data_base[fllen]=(Colored_Char_Type)0;
   lline->base_len=fllen;
   lline->col=col;
   return line;
}

/* Cette fonction ajoute une ligne au texte a scroller */
/* Elle renvoie NULL en cas d'échec.                   */
File_Line_Type *Ajoute_line(flrn_char *buf, int col, int field) {
    File_Line_Type *line=Text_scroll;

    if (line==NULL) {
       Text_scroll=ajoute_line(NULL,buf,NULL,0,0,0,col,field);
       if (Text_scroll) {
          Line_Window.num_lines++;
       }
       return Text_scroll;
    }
    while (line->next) line=line->next;
    line->next=ajoute_line(NULL,buf,NULL,0,0,0,col,field);
    if (line->next) {
        Line_Window.num_lines++;
        line->next->prev=line;
    }
    return (line->next);
} 

/* Cette fonction ajoute une ligne au texte a scroller */
/* Elle renvoie NULL en cas d'échec.                   */
/* si a_allouer!=0, on veut allouer plus...	       */
/* si flbuf==NULL, on recrée la chaîne directement */
File_Line_Type *Ajoute_color_Line(Colored_Char_Type *buf,
	        flrn_char *flbuf, size_t len, size_t mlen,
	        size_t fllen, size_t mfllen, int col) {
    File_Line_Type *line=Text_scroll;
    int allocated=0;

    if (mlen==0) mlen=len;
    if ((flbuf==NULL) && (buf!=NULL)) {
	size_t i;
	mfllen=mlen;
	fllen=len;
	flbuf=safe_malloc(sizeof(flrn_char)*(mlen+1));
	for (i=0;i<len;i++) 
          flbuf[i]=(flrn_char) (buf[i] & 0xFF);
	flbuf[i]=(flrn_char)0;
	allocated=1;
    }
    else if (mfllen==0) mfllen=fllen;
    if (line==NULL) {
       Text_scroll=create_color_line(buf,flbuf,len,mlen,fllen,mfllen,
	       col);
       if (Text_scroll) {
          Line_Window.num_lines++;
       }
       if (allocated) free(flbuf);
       return Text_scroll;
    }
    while (line->next) line=line->next;
    line->next=create_color_line(buf,flbuf,len,mlen,fllen,mfllen,col);
    if (line->next) {
        Line_Window.num_lines++;
        line->next->prev=line;
    }
    if (allocated) free(flbuf);
    return (line->next);
} 

/* Cette fonction rajoute un bout à la n-ième ligne du scrolling */
/* Elle suppose que suffisammenent de place a été alloué */
/* deb est la colonne du début du rajout (-1 si suite)	*/
/* si n=-1, on rajoute en fin */
/* Elle renvoie NULL en cas d'échec.                   */
File_Line_Type *Rajoute_color_Line(Colored_Char_Type *buf, 
	flrn_char *flbuf, int n, size_t len, size_t fllen, int deb) {
    File_Line_Type *line=Text_scroll;
    Line_Elem_Type *lline;
    int allocated=0;
    int i=0;

    if (line==NULL) return NULL; /* pas de ligne */
    while ((i<n) || ((n==-1) && (line->next))) {
       line=line->next;
       if (line==NULL) return NULL; /* pas de ligne */
       i++;
    }
    lline=line->line;
    while (lline->next) lline=lline->next;
    if (deb!=-1) {
	lline->next=(Line_Elem_Type *) safe_calloc (1,sizeof (Line_Elem_Type));
	lline=lline->next;
	lline->col=deb;
    }

    if ((flbuf==NULL) && (buf!=NULL)) {
	size_t i;
	fllen=len;
	flbuf=safe_malloc(sizeof(flrn_char)*(fllen+1));
	for (i=0;i<len;i++) 
          flbuf[i]=(flrn_char) (buf[i] & 0xFF);
	flbuf[i]=(flrn_char)0;
	allocated=1;
    }

    if (len+lline->data_len>lline->data_mlen) {
         lline->data=safe_realloc(lline->data,
		 sizeof(Colored_Char_Type)*(len+lline->data_len+1));
	 lline->data_mlen=len+lline->data_len;
    }
    if (buf) memcpy(lline->data+lline->data_len,buf,len*
	    sizeof(Colored_Char_Type));
    lline->data_len+=len;
    lline->data[lline->data_len]=(Colored_Char_Type)0;

    if (fllen+lline->base_len>lline->base_mlen) {
         lline->data_base=safe_realloc(lline->data_base,
		 sizeof(flrn_char)*(fllen+lline->base_len+1));
	 lline->base_mlen=fllen+lline->base_len;
    }
    if (flbuf) memcpy(lline->data_base+lline->base_len,flbuf,fllen*sizeof(flrn_char));
    lline->base_len+=fllen;
    lline->data_base[lline->base_len]=(flrn_char)0;

    return line;
} 

/* Applique la regexp_scroll sur une ligne */
static void Apply_regexp_scroll (File_Line_Type *line) {
    Colored_Char_Type  *retour=NULL;
    Line_Elem_Type *lline;
   
    lline=line->line;
    while (lline) {
      if (lline->data_save) {
         if (lline->data) free(lline->data);
         lline->data=lline->data_save;
         lline->data_save=NULL;
      }
      if (lline->data==NULL) { lline=lline->next; continue; }
      if (regexp_scroll) {
         retour=Search_in_line (lline->data,lline->data_base,lline->data_mlen,
		 regexp_scroll);
      }
      if (retour) {
         lline->data_save=lline->data;
         lline->data=retour;
	 retour=NULL;
      }
      lline=lline->next;
   }
}

File_Line_Type *Change_line(File_Line_Type * line,int a,flrn_char *buf,
	Colored_Char_Type *cbuf, size_t cclen) {
    Line_Elem_Type *lline;
    int i;
    /*
    for(i=0;i<n;i++) {
      if(line==NULL) return NULL;
      line=line->next;
    }  */
    if (line==NULL) {
       return NULL;
    }
    lline=line->line;
    for (i=0;i<a;i++) {
       if (lline==NULL) return NULL;
       lline=lline->next;
    }
    if (lline->data_save) {
       free(lline->data_save);
       lline->data_save=NULL;
    }
    memset(lline->data_base,0,sizeof(flrn_char)*lline->base_mlen);
    if (buf) fl_strncpy(lline->data_base,buf,lline->base_mlen-1);
    lline->base_len=fl_strlen(lline->data_base);
    memset(lline->data,0,sizeof(Colored_Char_Type)*lline->data_mlen);
    if (cbuf)
      for(i=0;(i<lline->data_mlen) && (i<cclen) && (cbuf[i]);i++) {
        lline->data[i]=cbuf[i];
      }
    lline->data_len=cclen;
    if (regexp_scroll) Apply_regexp_scroll(line);
    return (line);
} 

#if 0
char * Read_Line(char * out, File_Line_Type *line) {
  int i;
  for(i=0;i<line->data_len;i++)
    out[i]=line->data[i];
  out[line->data_len]='\0';
  return out;
}
#endif

/* échange de deux lignes */
void Swap_lines(File_Line_Type *line1,File_Line_Type *line2) {
    File_Line_Type *n1,*p1;
     /* Text_scroll */
    if (Text_scroll==line1) Text_scroll=line2;
    else if (Text_scroll==line2) Text_scroll=line1;
    if (Line_Window.top_window_line==line1) Line_Window.top_window_line=line2;
    else if (Line_Window.top_window_line==line2)
	Line_Window.top_window_line=line1;
    if (Line_Window.lines==line1) Line_Window.lines=line2;
    else if (Line_Window.lines==line2) Line_Window.lines=line1;
    /* on modifie les next et prev des voisins */
    n1=line1->next; p1=line1->prev;
    if ((n1) && (n1!=line2)) n1->prev=line2;
    if ((p1) && (p1!=line2)) p1->next=line2;
    n1=line2->next; p1=line2->prev;
    if ((n1) && (n1!=line1)) n1->prev=line1;
    if ((p1) && (p1!=line1)) p1->next=line1;
    /* on modifie les lignes elle-meme */
    if (line1->next!=line2) line2->next=line1->next;
    else line2->next=line1;
    if (line1->prev!=line2) line2->prev=line1->prev;
    else line2->prev=line1;
    if (n1!=line1) line1->next=n1; else line1->next=line2;
    if (p1!=line1) line1->prev=p1; else line1->prev=line2;
}


/* Recherche de quelque chose... mais quoi ? */
/* retour : -1 si le pattern est faux , 0 s'il est bon */
int New_regexp_scroll (flrn_char *pattern) {
     File_Line_Type *line=Text_scroll;
    
     if (regexp_scroll) {
        regfree(regexp_scroll);
     } else regexp_scroll=safe_calloc(1,sizeof(regex_t));
     if (fl_regcomp(regexp_scroll,pattern,REG_EXTENDED)) {
        free(regexp_scroll);
	regexp_scroll=NULL;
     }
     while (line) {
        Apply_regexp_scroll(line);
	line=line->next;
     }
     return (regexp_scroll ? 0 : -1);
}


/* Recherche d'un truc matchant... */
/* retourne NULL si rien */
File_Line_Type *Search_in_Lines (File_Line_Type *start) {
    File_Line_Type *line=start->next;
    
    while ((line) && (line->line->data_save==NULL)) {
       line=line->next;
    }
    if (line) return line;
    line=Text_scroll;
    while ((line) && (line!=start) && (line->line->data_save==NULL)) {
       line=line->next;
    }
    if ((line==NULL) || (line->line->data_save)) return line;
    return NULL;
}

/* Libere la derniere ligne de line */
void Retire_line(File_Line_Type *line) {
    Line_Elem_Type *lline,*nll;
     
    if (line==NULL) return;
    while (line->next) line=line->next;
    lline=line->line;
    while (lline) {
      if (lline->data) 
        free(lline->data);
      if (lline->data_save) free(lline->data_save);
      if (lline->data_base) free(lline->data_base);
      nll=lline->next;
      free(lline);
      lline=nll;
    }
    if (line->prev) line->prev->next=NULL;
    Line_Window.num_lines--;
    if (line==Text_scroll) Text_scroll=NULL;
    free(line);
    return;
}

void Init_Scroll_window(int num, int beg, int nrw){
  
    memset ((char *)&Line_Window, 0, sizeof (Scroll_Window_Type));
          
    Line_Window.top_window_line=Text_scroll;
    Line_Window.lines = Text_scroll;
    Line_Window.line_num = 1;
    Line_Window.num_lines = num;
    Line_Window.window_row=beg;
    Line_Window.nrows=nrw;
}

static void Update_scroll_display() {
   int row;
   File_Line_Type *line;
   Line_Elem_Type *lline;
	     
   /* On va bloquer les signaux temporairement */
   SLsig_block_signals ();
   row=Line_Window.window_row;
   line= Line_Window.top_window_line;
   while (row < Line_Window.nrows+Line_Window.window_row) 
   {
       SLsmg_gotorc(row,0);
       SLsmg_erase_eol();
       if ((line) && ((lline=line->line))) 
       {
	  while (lline) {
	     if (lline->col>=0) SLsmg_gotorc(row,lline->col);
             SLsmg_write_color_chars(lline->data, lline->data_len);
	     lline=lline->next;
	  }
          line=line->next;
       }
       row++;
   }
/* SLsmg_refresh();     Le refresh est fait séparément désormais */
   SLsig_unblock_signals();
}

/* ob_update =1 : l'update est obligatoire... */
int Do_Scroll_Window(int n, int ob_update) {
   int i;
   if (Line_Window.top_window_line == NULL)
     return 0;
   if (n<0) {
     for (i=0;(i>n) && (Line_Window.top_window_line->prev);i--) {
       Line_Window.top_window_line=Line_Window.top_window_line->prev;
     }
     Line_Window.line_num +=i;
   } else {
     for (i=0;(i<n) && (Line_Window.top_window_line->next);i++) {
       Line_Window.top_window_line=Line_Window.top_window_line->next;
     }
     Line_Window.line_num +=i;
   }
   if (i || ob_update) Update_scroll_display();
   return i;
}

/* ob_update =1 : l'update est obligatoire... */
int Do_Search(int just_do, int *le_scroll, int from_top) {
   int i;
   File_Line_Type *line=Line_Window.top_window_line;
   Line_Elem_Type *lline;
   for (i=0;i<from_top;i++) {
      if (line->next==NULL) break;
      line=line->next;
   }
   from_top=i;
   i=0;
   *le_scroll=0;
   if (regexp_scroll==NULL) return -1;
   if (Line_Window.top_window_line == NULL)
     return -2;
   if (!just_do) {
      if (line->next) {
         line=line->next;
	 i=1;
      } else {
         line=Line_Window.lines;
	 i=-Line_Window.line_num+1-from_top;
      }
   }
   while (1) {
      lline=line->line;
      while (lline) {
	  if (lline->data_save!=NULL) break;
	  lline=lline->next;
      }
      if (lline) break;
      if (line->next) {
         line=line->next;
         i++;
      } else {
         line=Line_Window.lines;
	 i=-Line_Window.line_num+1-from_top;
      }
      if (i==0) break;
   }
   *le_scroll=i;
   if (lline==NULL) return -2;
   return 0;
}

/* Un truc bien crade */
int Parcours_du_menu(int a) {
   static File_Line_Type *line;
   if (a<0) return 0;
   if (a==0) line=Line_Window.lines;
   else while (a) {
      if (line) line=line->next;
      if (line==NULL) line=Line_Window.lines;
      a--;
   }
   return (line->line->data_save!=NULL);
}

int Number_current_line_scroll() {
  return (Line_Window.line_num);
}

/* autre */
/* on va mettre les noms de touches ici... */
#define FIRST_FL_KEY_OFFSET 0x101
static const char *fl_key_names[] = {
  "up","down","left","right",
  "pageup","pagedown","home","end",
  "a1","a2","b2","c1",
  "c3","redo","undo","backspace","Enter","ic","delete"};
#define FIRST_FN_OFFSET 0x200

  /* Parse Ca, C-a, F10 */
int parse_key_name(flrn_char *name) {
  int i;
  if (fl_strcmp(name,"espace")==0) return 32;
  if (fl_strcmp(name,"enter")==0) return '\r';
  for (i=0;i<sizeof(fl_key_names)/sizeof(fl_key_names[0]);i++)
    if (fl_strcmp(name,
		fl_static_tran(fl_key_names[i]))==0)
	return i+FIRST_FL_KEY_OFFSET;
  if ((*name==fl_static('F')) || (*name ==fl_static('f')))
    return fl_strtol(name+1,NULL,10) + FIRST_FN_OFFSET;
  if ((*name==fl_static('M')) || (*name ==fl_static('m'))) {
    if (fl_strcmp(name,"M-esp")==0) return 160;
    if ((name[1] == fl_static('-')) && name[2]) 
	return ((unsigned char)name[2])|128;
    return ((unsigned char)name[1])|128;
  }
  if ((*name==fl_static('C')) || (*name ==fl_static('c'))
	  || (*name == fl_static('^'))) {
    if ((name[1] == fl_static('-')) && name[2])
	return toupper((char)(name[2]))-'A'+1;
    return toupper((char)(name[1]))-'A'+1;
  }
  return 0;
}

/* Taille maximum de la chaine rendue : 9 */
/* Tout est envoyé non alloué */
const flrn_char *get_name_char(int ch,int flag) {
  static flrn_char chaine[16];

  if (flag) fl_strcpy(chaine,"\\key-"); else chaine[0]='\0';
  if ((ch<0) || (ch>=MAX_FL_KEY)) return NULL;
  if (ch<FIRST_FL_KEY_OFFSET) {
     if (ch==32) { fl_strcat(chaine,fl_static("espace")); return chaine; }
     if (ch=='\r') { fl_strcat(chaine,fl_static("enter")); return chaine; }
     if ((ch<32) || (ch==127)) {
        fl_strcat(chaine,fl_static("C-?"));
	if (ch!=127) chaine[2+flag*5]=(ch==127 ? fl_static('?') :
		(flrn_char)(ch+'@'));
        return chaine;
     }
     if (ch<127) {
        chaine[0]=(flrn_char) ch;
	chaine[1]=fl_static('\0');
	return chaine;
     }
     if (ch==160) return fl_static("M-esp");
     if ((ch>160) && (ch<255)) {
        fl_strcat(chaine,fl_static("M- "));
	chaine[2+flag*5]=(flrn_char)(ch-128);
	return chaine;
     }
  } else {
     if (ch-FIRST_FL_KEY_OFFSET<sizeof(fl_key_names)/sizeof(fl_key_names[0])) {
	 fl_strcat(chaine,fl_key_names[ch-FIRST_FL_KEY_OFFSET]);
         return chaine;
     }
     if ((ch>=FIRST_FN_OFFSET) && (ch<FIRST_FN_OFFSET+21)) {
        fl_snprintf(chaine,15,fl_static("%sF%i"),
		(flag ? "\\key-" : ""),ch-FIRST_FN_OFFSET);
	return chaine;
     }
  }
  fl_snprintf(chaine,15,"%s(%4i)",(flag ? "\\key-" : ""),ch);
  return chaine;
}


/* Creation d'un caractère coloré */
Colored_Char_Type create_Colored_Char_Type (const char **str, int field) {
    char c;
#ifdef HAVE_WCHAR_H
    /* trichons gaiement, mais ce serait mieux en config, même si
     * l'optimisation de gcc analyse le code en direct */
    if (SLSMG_BUILD_CHAR(0,1)==0x100) { /* slang de base */
#endif
      c=**str; (*str)++;
      return SLSMG_BUILD_CHAR(c,field);
#ifdef HAVE_WCHAR_H
    }
    /* cas du patch slang */
    {
        wchar_t ch;
	mbstate_t ps;
	size_t d;
	/* beurk la retransformation en utf8 */
	memset(&ps,0,sizeof (mbstate_t));
	d=mbrtowc(&ch,*str,strlen(*str)+1,&ps);
	if (d==0) return (Colored_Char_Type)0;
	if (d==(size_t)(-1)) {
	    (*str)++; return SLSMG_BUILD_CHAR(L'?',field);
	}
	if (d==(size_t)(-2)) {
	    (*str)+=strlen(*str); return SLSMG_BUILD_CHAR(L'?',field);
	}
	(*str)+=d; return SLSMG_BUILD_CHAR(ch,field);
    }
#endif
}

size_t Transpose_Colored_String (Colored_Char_Type **cstr,
	char *guide, int field, size_t len) {
    Colored_Char_Type cch;
    const char *ptr=guide;
    size_t r=0;
    while ((*ptr) && (r<len) && (**cstr)) {
	cch=create_Colored_Char_Type(&ptr, (field==-1 ? 0 : field));
	if (cch==(Colored_Char_Type)0) break;
	if (field!=-1) **cstr=cch;
	(*cstr)++;
	r++;
    }
    return r;
}


