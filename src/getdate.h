/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      getdate.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/*   getdate.h  */

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/times.h>

extern time_t get_date (const char *, const time_t *);
