#ifndef FLRN_TTY_KEYBOARD_H
#define FLRN_TTY_KEYBOARD_H

/* les fonctions */

extern int Init_keyboard(void);
extern int Attend_touche(void);
extern int getline(char * /*buf*/, int /*buffsize*/, int /*row*/, int /*col*/);
extern int magic_getline(char * /*buf*/, int /*buffsize*/, int /*row*/,
    int /*col*/, char * /*magic*/, int /*flag*/, int /* key */);

#endif
