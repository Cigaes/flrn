/* flrn v 3.0								   */
/*									   */ 
/*		flrn_slang.c						   */
/*									   */
/*  Liens avec les fonctions slang					   */
/*  Ce fichier ne devrait pas être indispensable... si on passe à ncurses  */

#include <stdio.h>
#include <stdlib.h>
#include <slang.h>
#include <ctype.h>
#include "flrn.h"
#include "flrn_slang.h"

int KeyBoard_Quit;
int Screen_Rows, Screen_Cols;
int Screen_Tab_Width;

void Screen_suspend() { SLsmg_suspend_smg(); }
void Screen_resume() { SLsmg_resume_smg(); }
void Screen_get_size() { SLtt_get_screen_size(); 
			 Screen_Rows=SLtt_Screen_Rows;
			 Screen_Cols=SLtt_Screen_Cols;
		       }
int Screen_init_smg() { int a; a=SLsmg_init_smg(); Screen_Tab_Width=SLsmg_Tab_Width; return a; }
void Reset_screen() { SLsmg_reset_smg(); }
void Get_terminfo() { SLtt_get_terminfo();
		      Screen_Rows=SLtt_Screen_Rows;
		      Screen_Cols=SLtt_Screen_Cols;
		    }
void Screen_write_char(char c) { SLsmg_write_char(c); }
void Screen_write_string(char *s) { SLsmg_write_string(s); }
void Screen_write_nstring(char *s, int n) { SLsmg_write_nstring(s,n); }
void Screen_write_color_chars(unsigned short *s, unsigned int n) 
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



/**************************************************************************/
/**************************************************************************/
/*   Gestion des scrolls ... cf pager.c dans les demos de slang.         **/
/**************************************************************************/

/* Fenetre de scroll */
File_Line_Type *Text_scroll=NULL;
/* on s'inspire de slang, amis on change les types et on simplifie */
typedef struct
{
  unsigned int flags;
  File_Line_Type *top_window_line; /* list element at top of window */
  File_Line_Type *lines;           /* first list element */
  unsigned int nrows;             /* number of rows in window */
  unsigned int line_num;          /* current line number (visible) */
  unsigned int num_lines;         /* total number of lines (visible) */
  unsigned int window_row;        /* row of current_line in window */
}
Scroll_Window_Type;

static Scroll_Window_Type Line_Window; /* On va essayer de limiter */
                                /* l'usage de cette variable a ce fichier */

void free_text_scroll() {
     File_Line_Type *line, *next;
     
     line=Text_scroll;
     while (line != NULL) {
        next=line->next;
        if (line->data != NULL) free (line->data);
        free(line);
        line=next;
     }
     Text_scroll=NULL;
}

/* fonction de compatibilité... */
static File_Line_Type *create_line (char *buf) {
   File_Line_Type *line;
   int i;
     
   line = (File_Line_Type *) safe_calloc (1,sizeof (File_Line_Type));
/*   line->data = SLmake_string (buf); */  /* use a slang routine */
   line->data = safe_malloc(sizeof(short) * (strlen(buf)+1));
   for(i=0;buf[i];i++) {
     line->data[i]=(unsigned char)buf[i];
   }
   line->data[i]=buf[i];
   line->data_len=i;
   return line;
}

/* on refait un malloc... */
static File_Line_Type *create_color_line (unsigned short *buf, int len) {
   File_Line_Type *line;
   int i;
     
   line = (File_Line_Type *) safe_calloc (1,sizeof (File_Line_Type));
   line->data = safe_malloc(sizeof(short) * len);
   for(i=0;i<len;i++) {
     line->data[i]=buf[i];
   }
   line->data_len=len;
   return line;
}

/* Cette fonction ajoute une ligne au texte a scroller */
/* Elle renvoie NULL en cas d'échec.                   */
File_Line_Type *Ajoute_line(char *buf) {
    File_Line_Type *line=Text_scroll;

    if (line==NULL) {
       Text_scroll=create_line(buf);
       if (Text_scroll) {
          Line_Window.num_lines++;
       }
       return Text_scroll;
    }
    while (line->next) line=line->next;
    line->next=create_line(buf);
    if (line->next) {
        Line_Window.num_lines++;
        line->next->prev=line;
    }
    return (line->next);
} 

/* Cette fonction ajoute une ligne au texte a scroller */
/* Elle renvoie NULL en cas d'échec.                   */
File_Line_Type *Ajoute_color_Line(unsigned short *buf, int len) {
    File_Line_Type *line=Text_scroll;

    if (line==NULL) {
       Text_scroll=create_color_line(buf,len);
       if (Text_scroll) {
          Line_Window.num_lines++;
       }
       return Text_scroll;
    }
    while (line->next) line=line->next;
    line->next=create_color_line(buf,len);
    if (line->next) {
        Line_Window.num_lines++;
        line->next->prev=line;
    }
    return (line->next);
} 

File_Line_Type *Change_line(int n,char *buf) {
    File_Line_Type *line=Text_scroll;
    int i;
    for(i=0;i<n;i++) {
      if(line==NULL) return NULL;
      line=line->next;
    }
    if (line==NULL) {
       return NULL;
    }
    free(line->data);
    line->data = safe_malloc(sizeof(short) * (strlen(buf)+1));
    for(i=0;buf[i];i++) {
      line->data[i]=(unsigned char)buf[i];
    }
    return (line);
} 

File_Line_Type *Ajoute_form_Ligne(char *buf, int field) {
  File_Line_Type *line=Ajoute_line(buf);
  int i;
  for(i=0;i<line->data_len;i++)
    line->data[i] += field<<8;
  return line;
}

char * Read_Line(char * out, File_Line_Type *line) {
  int i;
  for(i=0;i<line->data_len;i++)
    out[i]=line->data[i];
  return out;
}

/* Libere la derniere ligne de line */
void Retire_line(File_Line_Type *line) {
     
    if (line==NULL) return;
    while (line->next) line=line->next;
    if (line->data) 
      free(line->data);
    if (line->prev) line->prev->next=NULL;
      Line_Window.num_lines--;
      free(line);
    if (line==Text_scroll) Text_scroll=NULL;
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
	     
   /* On va bloquer les signaux temporairement */
   SLsig_block_signals ();
   row=Line_Window.window_row;
   line= Line_Window.top_window_line;
   while (row < Line_Window.nrows+Line_Window.window_row) 
   {
       SLsmg_gotorc(row, 0);
       if (line != NULL)
       {
	  SLsmg_write_color_chars(line->data, line->data_len);
          line=line->next;
       }
       SLsmg_erase_eol();
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
  "c3","redo","undo"};
#define FIRST_FN_OFFSET 0x200

int parse_key_name(char *name) {
  int i;
  for (i=0;i<sizeof(fl_key_names)/sizeof(fl_key_names[0]);i++)
    if (strcmp(name,fl_key_names[i])==0) return i+FIRST_FL_KEY_OFFSET;
  if ((*name=='F') || (*name =='f'))
    return strtol(name,NULL,10) + FIRST_FN_OFFSET;
  if ((*name=='M') || (*name =='m'))
    return name[1]|128;
  if ((*name=='C') || (*name =='c') || (*name == '^'))
    return toupper(name[1])-'A'+1;
  return 0;
}
