/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_slang.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_SLANG_H

#define FLRN_SLANG_H

#include <slang.h>

typedef SLsmg_Char_Type Colored_Char_Type;

/* Gestion des scrolls */
typedef struct _Line_Elem_Type
{
    struct _Line_Elem_Type *next;
    int col;
    Colored_Char_Type *data;
    Colored_Char_Type *data_save;
    flrn_char *data_base;
    size_t data_len;
    size_t base_len;
    size_t data_mlen;
    size_t base_mlen;
} Line_Elem_Type;

typedef struct _File_Line_Type
{
      struct _File_Line_Type *next;
      struct _File_Line_Type *prev;
      Line_Elem_Type *line;
}
File_Line_Type;

extern File_Line_Type *Text_scroll;


/* Je recode les touches fl�ches */
#define FL_KEY_UP 	0x101
#define FL_KEY_DOWN 	0x102
#define FL_KEY_LEFT 	0x103
#define FL_KEY_RIGHT 	0x104
#define FL_KEY_BACKSPACE        0x110
#define FL_KEY_DELETE           0x113


/* Je recode les attributs */
#define FL_BOLD_MASK  0x01000000
#define FL_BLINK_MASK 0x02000000
#define FL_ULINE_MASK 0x04000000
#define FL_REV_MASK   0x08000000
#define FL_ALTC_MASK  0x10000000

/* Les fonctions */

#include "flrn_glob.h"
#include "enc/enc_base.h"

extern void Screen_suspend(void);
extern void Screen_resume(void);
extern void Screen_get_size(int hl);
extern int Screen_init_smg(void);
extern void Screen_reset(void);
extern void Get_terminfo(int hl);
extern void Set_term_cannot_scroll(int);
extern void Screen_write_char(char /*c*/);
extern void Screen_write_string(char * /*s*/);
extern void Screen_write_nstring(char * /*s*/, int /*n*/);
extern void Cursor_gotorc(int /*r*/, int /*c*/);
extern void Screen_erase_eol(void);
extern void Screen_erase_eos(void);
extern void Screen_refresh(void);
extern void Screen_touch_lines (int /*d*/, unsigned int /*n*/);
extern void Set_Use_Ansi_Colors(int /*n*/);
extern int  Get_Use_Ansi_Colors(void);
extern void Color_set(int /*n*/, char * /*s1*/, char * /*s2*/, char * /*s3*/);
extern void Color_add_attribute(int /*n*/, FL_Char_Type /*a*/);
extern void Mono_set (int /*n*/, char * /*s*/, FL_Char_Type /*a*/);
extern void Screen_set_color(int /*n*/);
extern int Put_tty_single_input (int /*a*/, int /*b*/, int /*c*/);
extern void React_suspend_char (int /*a*/);
extern int init_clavier(void);
extern void Set_Ignore_User_Abort (int /*n*/);
extern int Get_Ignore_User_Abort (void);
extern int Set_Abort_Signal(void (* /*a*/)(int));
extern int Keyboard_getkey(void);
extern void Reset_keyboard(void);
extern void free_text_scroll(void);
extern void Init_Scroll_window(int /*num*/, int /*beg*/, int /*nrw*/);
extern File_Line_Type *Ajoute_line(flrn_char *, int, int);
extern File_Line_Type *Ajoute_formated_line (flrn_char *, char *,
	size_t, size_t, int, int, int, int, File_Line_Type *);
extern File_Line_Type *Change_line(File_Line_Type *,
	int,flrn_char *,Colored_Char_Type *, size_t);
extern File_Line_Type *Ajoute_color_Line(Colored_Char_Type *, flrn_char *,
	           size_t,size_t,size_t,size_t,int);
extern File_Line_Type *Rajoute_color_Line(Colored_Char_Type *, flrn_char *,
	                        int, size_t, size_t, int);
/* extern char *Read_Line(char * , File_Line_Type * ); */
extern int New_regexp_scroll (char *);
extern File_Line_Type *Search_in_Lines (File_Line_Type *);
extern void Retire_line(File_Line_Type *);
extern void Swap_lines(File_Line_Type *, File_Line_Type *);
extern int Do_Scroll_Window(int /*n*/, int /*ob_update*/);
extern int Do_Search(int,int *,int);
extern int Number_current_line_scroll(void);
extern void Screen_write_color_chars(Colored_Char_Type * /*s*/,
	                                    unsigned int /*n*/);
extern int parse_key_name(flrn_char *);
extern const flrn_char *get_name_char(int,int);
extern void Screen_beep(void);
extern void Screen_set_screen_start(int * /*r*/, int * /*c*/);
extern int Parcours_du_menu(int);
extern void set_Display_Eight_Bit (int);

extern Colored_Char_Type create_Colored_Char_Type (const char **, int);
extern size_t Transpose_Colored_String (Colored_Char_Type **,
	char *, int, size_t);
static inline Colored_Char_Type build_Colored_Ascii (char c, int col) {
    return SLSMG_BUILD_CHAR(c,col);
}

#endif
