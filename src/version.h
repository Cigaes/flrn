/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      version.h : headers de version.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_VERSION_H
#define FLRN_VERSION_H

extern char version_string[];
extern char short_version_string[];
extern int version_number;
extern void print_version_info(FILE * /* out */, char * /*program_name*/);
extern int version_has(char * /* request */);

#endif
