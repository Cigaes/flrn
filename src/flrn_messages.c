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

#include "flrn.h"
#include "flrn_messages.h"
#include "config.h"
#include "compatibility.h"

static UNUSED char rcsid[]="$Id$";

const char *Messages[NB_MESSAGES] = {
    N_("Commande inconnue ou hors contexte !"),     /* 0 */
    N_("Rien de nouveau."),                         /* 1 */
    N_("Fin du newsgroup."),                        /* 2 */
    N_("Message(s) inexistant(s)."),                /* 3 */
    N_("Les messages sont marqués non lus."),       /* 4 */
    N_("Post envoyé."),                             /* 5 */
    N_("Post annulé."),                             /* 6 */
    N_("Article(s) sauvé(s)."),                     /* 7 */
    N_("Abonnement effectué."),                     /* 8 */
    N_("Désabonnement effectué."),			/* 9 */
    N_("Pas de thread sélectionné."),               /* 10 */
    N_("Groupe zappé."),                            /* 11 */
    N_("Pas d'autres newsgroups."),                 /* 12 */
    N_("Xref non trouvé !"),                        /* 13 */
    N_("(continue)"),                               /* 14 */
    N_("Tag mis"),                                  /* 15 */
    N_("Cancel annulé"),                            /* 16 */
    N_("Article(s) cancelé(s)"),                    /* 17 */
    N_("Opération effectuée"),                      /* 18 */
    N_("Mail envoyé"),                              /* 19 */
    N_("Mail envoyé, article posté"),               /* 20 */
    N_("Article(s) lu(s) temporairement."),         /* 21 */
    N_("Vous n'êtes abonné à aucun groupe"),        /* 22 */
    N_("Newsgroup vide"),                           /* 23 */
    N_("Vous n'êtes dans aucun groupe"),            /* 24 */
    N_("Pas d'article négatif"),                    /* 25 */
    N_("Echec de la sauvegarde."),                  /* 26 */
    N_("Newsgroup inconnu, supprimé !"),            /* 27 */
    N_("Pas de newsgroup trouvé"),                  /* 28 */
    N_("Regexp invalide !"),                        /* 29 */
    N_("Echec du pipe..."),                         /* 30 */
    N_("L'article n'est plus dans le groupe"),      /* 31 */
    N_("Tag invalide"),                             /* 32 */
    N_("Cancel refusé"),                            /* 33 */
    N_("Historique vide."),                         /* 34 */
    N_("Pas de header !"),                          /* 35 */
    N_("Header refusé !"),                          /* 36 */
    N_("Erreur fatale !!!"),                        /* 37 */
    N_("Pas de recherche."),			/* 38 */ /* pager, menus */
    N_("Rien trouvé."),				/* 39 */ /* pager, menus */
    N_("Flag invalide."),			/* 40 */
    N_("Flag appliqué."),			/* 41 */ /* pager, menus */
    N_("Macro interdite."),			/* 42 */
    N_("Message-ID inconnu."),			/* 43 */
    N_("Passage en mode thread."),		/* 44 */
    N_("Passage en mode normal.")		/* 45 */
};

