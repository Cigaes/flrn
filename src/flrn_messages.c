/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_messages.c : messages d'erreur
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* FLRN : les messages */

#include "flrn_messages.h"
#include "config.h"
#include "compatibility.h"

static UNUSED char rcsid[]="$Id$";

const char *Messages[NB_MESSAGES] = {
    "Commande inconnue ou hors contexte !",     /* 0 */
    "Rien de nouveau.",                         /* 1 */
    "Fin du newsgroup.",                        /* 2 */
    "Message(s) inexistant(s).",                /* 3 */
    "Les messages sont marqu�s non lus.",       /* 4 */
    "Post envoy�.",                             /* 5 */
    "Post annul�.",                             /* 6 */
    "Article(s) sauv�(s).",                     /* 7 */
    "Abonnement effectu�.",                     /* 8 */
    "D�sabonnement effectu�.",			/* 9 */
    "Pas de thread s�lectionn�.",               /* 10 */
    "Groupe zapp�.",                            /* 11 */
    "Pas d'autres newsgroups.",                 /* 12 */
    "Xref non trouv� !",                        /* 13 */
    "(continue)",                               /* 14 */
    "Tag mis",                                  /* 15 */
    "Cancel annul�",                            /* 16 */
    "Article(s) cancel�(s)",                    /* 17 */
    "Op�ration effectu�e",                      /* 18 */
    "Mail envoy�",                              /* 19 */
    "Mail envoy�, article post�",               /* 20 */
    "Article(s) lu(s) temporairement.",         /* 21 */
    "Vous n'�tes abonn� � aucun groupe",        /* 22 */
    "Newsgroup vide",                           /* 23 */
    "Vous n'�tes dans aucun groupe",            /* 24 */
    "Pas d'article n�gatif",                    /* 25 */
    "Echec de la sauvegarde.",                  /* 26 */
    "Newsgroup inconnu, supprim� !",            /* 27 */
    "Pas de newsgroup trouv�",                  /* 28 */
    "Regexp invalide !",                        /* 29 */
    "Echec du pipe...",                         /* 30 */
    "L'article n'est plus dans le groupe",      /* 31 */
    "Tag invalide",                             /* 32 */
    "Cancel refus�",                            /* 33 */
    "Historique vide.",                         /* 34 */
    "Pas de header !",                          /* 35 */
    "Header refus� !",                          /* 36 */
    "Erreur fatale !!!",                        /* 37 */
    "Pas de recherche.",			/* 38 */ /* pager, menus */
    "Rien trouv�.",				/* 39 */ /* pager, menus */
    "Flag invalide.",				/* 40 */
    "Flag appliqu�.",				/* 41 */ /* pager, menus */
    "Macro interdite.",				/* 42 */
    "Message-ID inconnu.",			/* 43 */
    "Passage en mode thread.",			/* 44 */
    "Passage en mode normal."			/* 45 */
};

