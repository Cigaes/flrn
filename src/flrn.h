/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_H
#define FLRN_FLRN_H

#include "config.h"	    /* Defines de configure */
#include "flrn_config.h"    /* Defines autres */
#include "compatibility.h"  /* La compatibilité */
#include "enc/enc_base.h"   /* fonctions chaînes de base */
#include "flrn_glob.h"      /* Variables externes */

/* #define I18N_GETTEXT 1 */
#undef I18N_GETTEXT
#ifdef I18N_GETTEXT
#include <libintl.h>

#define _(String) gettext(String)
#define N_(String) (String)
#else
#define _(String) (String)
#define N_(String) (String)
#define textdomain(Domain)
#define bindtextdomain(Package,Directory)
#endif


#endif
