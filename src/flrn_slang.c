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
#include "flrn_color.h"

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
void Screen_reset() {  SLsmg_reset_smg(); }

void Get_terminfo() { SLtt_get_terminfo();
		      Screen_Rows=SLtt_Screen_Rows;
		      Screen_Cols=SLtt_Screen_Cols;
		    }
void Set_term_cannot_scroll (int a) { SLtt_Term_Cannot_Scroll=a; }
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
  unsigned int flags;
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
     
     line=Text_scroll;
     while (line != NULL) {
        next=line->next;
        if (line->data != NULL) free (line->data);
        if (line->data_save != NULL) free (line->data_save);
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
static File_Line_Type *create_color_line (unsigned short *buf, int len,
						int a_allouer) {
   File_Line_Type *line;
   int i;
     
   line = (File_Line_Type *) safe_calloc (1,sizeof (File_Line_Type));
   line->data = safe_malloc(sizeof(short) * a_allouer);
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
/* si a_allouer!=0, on veut allouer plus...	       */
File_Line_Type *Ajoute_color_Line(unsigned short *buf, int len, 
					int a_allouer) {
    File_Line_Type *line=Text_scroll;

    if (a_allouer==0) a_allouer=len;
    if (line==NULL) {
       Text_scroll=create_color_line(buf,len,a_allouer);
       if (Text_scroll) {
          Line_Window.num_lines++;
       }
       return Text_scroll;
    }
    while (line->next) line=line->next;
    line->next=create_color_line(buf,len,a_allouer);
    if (line->next) {
        Line_Window.num_lines++;
        line->next->prev=line;
    }
    return (line->next);
} 

/* Cette fonction rajoute un bout à la n-ième ligne du scrolling */
/* Elle suppose que suffisammenent de place a été alloué */
/* deb est la colonne du début du rajout (-1 si suite)	*/
/* si n=-1, on rajoute en fin...			*/
/* Elle renvoie NULL en cas d'échec.                   */
File_Line_Type *Rajoute_color_Line(unsigned short *buf, int n, 
					int len, int deb) {
    File_Line_Type *line=Text_scroll;
    int i=0;

    if (line==NULL) return NULL; /* pas de ligne */
    while ((i<n) || ((n==-1) && (line->next))) {
       line=line->next;
       if (line==NULL) return NULL; /* pas de ligne */
       i++;
    }
    if (deb==-1) deb=line->data_len;
    if (line->data_len<deb)   /* faut completer... je vais essayer :-( */
       for (i=line->data_len;i<deb;i++) line->data[i]=(buf[0] & 256)+32;
    for (i=0;i<len;i++) {
       line->data[deb+i]=buf[i];
    }
    line->data_len=deb+len;
    return line;
} 

/* Applique la regexp_scroll sur une ligne */
static void Apply_regexp_scroll (File_Line_Type *line) {
    unsigned short *retour=NULL;
    char *out;
   
    if (line->data_save) {
       if (line->data) free(line->data);
       line->data=line->data_save;
       line->data_save=NULL;
    }
    if (line->data==NULL) return;
    if (regexp_scroll) {
       out=safe_malloc(line->data_len+1);
       Read_Line(out,line);
       retour=Search_in_line (line->data,out,line->data_len,regexp_scroll);
       free(out);
    }
    if (retour) {
       line->data_save=line->data;
       line->data=retour;
    }
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
    if (line->data_save) {
       free(line->data_save);
       line->data_save=NULL;
    }
    line->data = safe_malloc(sizeof(short) * (strlen(buf)+1));
    for(i=0;(buf[i]) && (buf[i]!='\n');i++) {
      line->data[i]=(unsigned char)buf[i];
    }
    line->data[i]=(unsigned char)buf[i];
    line->data_len=i;
    if (regexp_scroll) Apply_regexp_scroll(line);
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
  out[line->data_len]='\0';
  return out;
}

/* Recherche de quelque chose... mais quoi ? */
/* retour : -1 si le pattern est faux , 0 s'il est bon */
int New_regexp_scroll (char *pattern) {
     File_Line_Type *line=Text_scroll;
    
     if (regexp_scroll) {
        regfree(regexp_scroll);
     } else regexp_scroll=safe_calloc(1,sizeof(regex_t));
     if (regcomp(regexp_scroll,pattern,REG_EXTENDED)) {
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
    
    while ((line) && (line->data_save==NULL)) {
       line=line->next;
    }
    if (line) return line;
    line=Text_scroll;
    while ((line) && (line!=start) && (line->data_save==NULL)) {
       line=line->next;
    }
    if ((line==NULL) || (line->data_save)) return line;
    return NULL;
}

/* Libere la derniere ligne de line */
void Retire_line(File_Line_Type *line) {
     
    if (line==NULL) return;
    while (line->next) line=line->next;
    if (line->data) 
      free(line->data);
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

  /* Parse Ca, C-a, F10 */
int parse_key_name(char *name) {
  int i;
  for (i=0;i<sizeof(fl_key_names)/sizeof(fl_key_names[0]);i++)
    if (strcmp(name,fl_key_names[i])==0) return i+FIRST_FL_KEY_OFFSET;
  if ((*name=='F') || (*name =='f'))
    return strtol(name,NULL,10) + FIRST_FN_OFFSET;
  if ((*name=='M') || (*name =='m')) {
    if ((name[1] == '-') && name[2]) return name[2]|128;
    return name[1]|128;
  }
  if ((*name=='C') || (*name =='c') || (*name == '^')) {
    if ((name[1] == '-') && name[2]) return toupper(name[2])-'A'+1;
    return toupper(name[1])-'A'+1;
  }
  return 0;
}

/* Taille maximum de la chaine rendue : 9 */
/* Tout est envoyé non alloué */
const char *get_name_char(int ch) {
  static char chaine[10];

  if ((ch<0) || (ch>=MAX_FL_KEY)) return NULL;
  if (ch<FIRST_FL_KEY_OFFSET) {
     if (ch==32) return "espace";
     if (ch=='\r') return "enter";
     if ((ch<32) || (ch==127)) {
        strcpy(chaine,"C-?");
	if (ch!=127) chaine[2]=(ch==127 ? '?' : (char)(ch+'@'));
        return chaine;
     }
     if (ch<127) {
        chaine[0]=(char) ch;
	chaine[1]='\0';
	return chaine;
     }
     if (ch==160) return "M-esp";
     if ((ch>160) && (ch<255)) {
        strcpy(chaine,"M- ");
	chaine[2]=(char)(ch-128);
	return chaine;
     }
  } else {
     if (ch-FIRST_FL_KEY_OFFSET<sizeof(fl_key_names)/sizeof(fl_key_names[0])) 
         return fl_key_names[ch-FIRST_FL_KEY_OFFSET];
     if ((ch>=FIRST_FN_OFFSET) && (ch<FIRST_FN_OFFSET+21)) {
        sprintf(chaine,"F%i",ch-FIRST_FN_OFFSET);
	return chaine;
     }
  }
  sprintf(chaine,"(%4i)",ch);
  return chaine;
}
