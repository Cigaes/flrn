#ifndef FLRN_FLRN_XOVER_H
#define FLRN_FLRN_XOVER_H

#include "art_group.h"

extern int va_dans_groupe(void);
extern int get_overview_fmt(void);
extern int cree_liste_xover(int /*n1*/, int /*n2*/, Article_List **);
extern int cree_liste_noxover(int /*n1*/, int /*n2*/,
    Article_List * /*article*/);
extern int cree_liste(int, int * );
extern int cree_liste_end(void);

#endif
