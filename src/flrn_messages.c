/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
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
    "Les messages sont marqués non lus.",       /* 4 */
    "Post envoyé.",                             /* 5 */
    "Post annulé.",                             /* 6 */
    "Article(s) sauvé(s).",                     /* 7 */
    "Abonnement effectué.",                     /* 8 */
    "Désabonnement effectué.",			/* 9 */
    "Pas de thread sélectionné.",               /* 10 */
    "Groupe zappé.",                            /* 11 */
    "Pas d'autres newsgroups.",                 /* 12 */
    "Xref non trouvé !",                        /* 13 */
    "(continue)",                               /* 14 */
    "Tag mis",                                  /* 15 */
    "Cancel annulé",                            /* 16 */
    "Article(s) cancelé(s)",                    /* 17 */
    "Opération effectuée",                      /* 18 */
    "Mail envoyé",                              /* 19 */
    "Mail envoyé, article posté",               /* 20 */
    "Article(s) lu(s) temporairement.",         /* 21 */
    "Vous n'êtes abonné à aucun groupe",        /* 22 */
    "Newsgroup vide",                           /* 23 */
    "Vous n'êtes dans aucun groupe",            /* 24 */
    "Pas d'article négatif",                    /* 25 */
    "Echec de la sauvegarde.",                  /* 26 */
    "Newsgroup inconnu, supprimé !",            /* 27 */
    "Pas de newsgroup trouvé",                  /* 28 */
    "Regexp invalide !",                        /* 29 */
    "Echec du pipe...",                         /* 30 */
    "L'article n'est plus dans le groupe",      /* 31 */
    "Tag invalide",                             /* 32 */
    "Cancel refusé",                            /* 33 */
    "Historique vide.",                         /* 34 */
    "Pas de header !",                          /* 35 */
    "Header refusé !",                          /* 36 */
    "Erreur fatale !!!",                        /* 37 */
    "Pas de recherche.",			/* 38 */ /* pager, menus */
    "Rien trouvé.",				/* 39 */ /* pager, menus */
    "Flag invalide.",				/* 40 */
    "Flag appliqué.",				/* 41 */ /* pager, menus */
    "Macro interdite.",				/* 42 */
    "Message-ID inconnu.",			/* 43 */
    "Passage en mode thread.",			/* 44 */
    "Passage en mode normal."			/* 45 */
};

