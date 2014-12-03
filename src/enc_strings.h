/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-2003  Damien Massé et Joël-Yann Fourré
 *
 *        enc_strings.h : gestion des multi-byte strings, locales, etc...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_ENC_STRING_H
#define FLRN_ENC_STRING_H

#include <wchar.h>

#include "flrn.h"

/* gestion des charsets */
extern int read_terminal_charset ();
extern int init_charsets();

extern void width_termchar(char *,int *, size_t *);

extern size_t previous_flch(flrn_char *, size_t, size_t );
extern size_t next_flch(flrn_char *, size_t );

/* shift state pour la conversion */
typedef mbstate_t flrn_mbstate;

extern char *set_localeUTF8 ();

extern int init_conv_base ();

extern size_t flrn_wcstofls (flrn_char **, wflrn_char **, size_t);
extern size_t flrn_mbstofls (flrn_char **, const nflrn_char **,
	                        size_t, flrn_mbstate *);
extern int fl_stdconv_open (const char *);
extern int fl_stdconv_close ();
extern size_t fl_conv_to_flstring (char **, size_t *,
	                           flrn_char **, flrn_char **, size_t *);
extern int fl_revconv_open (const char *);
extern int fl_revconv_close ();
extern size_t fl_conv_from_flstring (flrn_char **, size_t *,
	                             char **, char **, size_t *);
extern int open_approximate_conversion ();
extern int close_approximate_conversion ();
extern size_t fl_approximate_conv (flrn_char **, size_t *,
	                           char **, char **, size_t *);
extern int isolate_non_critical_element (flrn_char *, flrn_char*,
	                                 flrn_char *, flrn_char **,
					 flrn_char **);

extern int find_best_conversion (flrn_char *, size_t, const char **, const char *); 

/* conversions rapides */
extern int conversion_to_editor(const flrn_char *, char **, size_t, size_t);
extern int conversion_from_editor(const char *, flrn_char **, size_t, size_t);
extern int conversion_to_file(const flrn_char *, char **, size_t, size_t);
extern int conversion_from_file(const char *, flrn_char **, size_t, size_t);
extern int conversion_to_terminal(const flrn_char *, char **, size_t, size_t);
extern int conversion_from_terminal(const char *, flrn_char **, size_t, size_t);
extern int conversion_to_message(const flrn_char *, char **, size_t, size_t);
extern int conversion_from_message(const char *, flrn_char **, size_t, size_t);
extern int conversion_to_utf8(const flrn_char *, char **, size_t, size_t);
extern int conversion_from_utf8(const char *, flrn_char **, size_t, size_t);


extern int convert_termchar(const char *, size_t, flrn_char **, size_t);

/* parsing de Content-Type */
extern const char *parse_ContentType_header(flrn_char *);

/* parsing charsets */
int Parse_termcharset_line(flrn_char *);
int Parse_filescharset_line(flrn_char *);
int Parse_postcharsets_line(flrn_char *);
int Parse_editorcharset_line(flrn_char *);

/* change for message */
int Change_message_conversion(char *);

/* QP */
#ifdef USE_CONTENT_ENCODING
extern void change_QP_mode(int);
#endif

#endif
