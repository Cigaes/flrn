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

/* les fonctions */

extern int Init_keyboard(int);
extern int Attend_touche(void);
extern int getline(char * /*buf*/, int /*buffsize*/, int /*row*/, int /*col*/);
extern int magic_getline(char * /*buf*/, int /*buffsize*/, int /*row*/,
    int /*col*/, char * /*magic*/, int /*flag*/, int /* key */);

#endif
