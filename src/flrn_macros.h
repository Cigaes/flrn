/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_macros.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_MACROS_H
#define FLRN_MACROS_H

/* on partage les macros entre tous les contextes */
#define MAX_FL_MACRO 256

typedef struct {
  int cmd;
  char *arg;
  int next_cmd;
} Flcmd_macro_t;

#endif
