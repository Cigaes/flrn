/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *        rfc2045.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_RFC2045_H
#define FLRN_RFC2045_H

extern void init_charsets ();
extern int Parse_charset_line (char *);
extern int Parse_ContentType_header (char *);
extern int Decode_ligne_message (char *, char **);

#endif
