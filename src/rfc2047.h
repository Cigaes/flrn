/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      rfc2047.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_RFC_2047_H
#define FLRN_RFC_2047_H

#include <stdio.h>

#define SHORT_STRING 128
#define ENCQUOTEDPRINTABLE 1
#define ENCBASE64 2

/* les fonctions */

extern void rfc2047_encode_string (char *, unsigned char *, size_t);
extern void rfc2047_decode (char *, const char *, size_t);

#endif
