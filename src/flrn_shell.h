/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_shell.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_FLRN_SHELL_H
#define FLRN_FLRN_SHELL_H

/* Les fonctions */

extern int Launch_Editor (int /*flags*/);
extern int Pipe_Msg_Start(int /*flagin*/, int /*flagout*/, char * /*cmd*/);
extern int Pipe_Msg_Stop(int /*fd*/);

#endif
