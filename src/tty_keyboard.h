/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      tty_keyboard.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_TTY_KEYBOARD_H
#define FLRN_TTY_KEYBOARD_H

#include "enc/enc_base.h"

/* entrée */
struct key_entry {
#define ENTRY_ERROR_KEY -1
#define ENTRY_SLANG_KEY 0
#define ENTRY_STATIC_STRING 1
#define ENTRY_ALLOCATED_STRING 2
    int entry;
    union { 
	int slang_key;
	flrn_char ststr[4]; /* j'en mets beaucoup trop si on compte en
			       wide char, mais tant pis */
	flrn_char *allocstr;
    } value;
};


/* les fonctions */

extern int Init_keyboard(int);
extern const flrn_char *get_name_key(struct key_entry *, int);
extern void Attend_touche(struct key_entry *);
extern size_t parse_key_string(flrn_char *, struct key_entry *);
extern void Free_key_entry (struct key_entry *);
extern int key_are_equal (struct key_entry *, struct key_entry *);
extern int flrn_getline(flrn_char *, size_t , char *, size_t, int, int );
extern int magic_flrn_getline(flrn_char *, size_t , char *, size_t,
	int , int , char * , int , struct key_entry *, struct key_entry *);

#endif
