/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      flrn_tcp.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_FLRN_TCP_H
#define FLRN_FLRN_TCP_H

/* On note ici ce qu'on connait du serveur */
/* Ce fichier devrait �tre export� dans flrn_tcp.c et flrn_xover.c (� la */
/* limite pour ce dernier...) */

/* Nombres de commandes : d�fini dans flrn_config.h */
#define CMD_FLAG_TESTED 1
#define CMD_FLAG_KNOWN 3
#define CMD_FLAG_MAXIMAL 7
extern int server_command_status[NB_TCP_CMD];

/* On note ici les divers comportements minimaux (et accepte-t-on inconnu ?) :

nom		|inconnu|	minimal	
QUIT		|   ? 	|	   X
MODE READER	|   O	|	   X
NEWGROUPS	|   O  	|	   X   (r�sultat toujours vide)
NEWNEWS		|   O	|	   X   (r�sultat toujours vide)
GROUP		|   N	|	   X   (je vois pas ce que �a pourrait �tre)
STAT		|   O	|	   peut ne pas renvoyer num ou mesgid
HEAD		|   O	|	   peut ne pas renvoyer num ou mesgid
BODY		|   O	|	   peut ne pas renvoyer num ou mesgid
XHDR		|   O(1)|	   X   (on n'appelle que pour ID et References)
XOVER		|   O(1)|	   X
POST		|   O(2)|	   X
LIST		|   N	|	   aucun argument possible
DATE		|   O	|	   X   (mais elle peut ne pas exister)
ARTICLE		|   N	|	   peut ne pas renvoyer num ou mesgid
(1) XHDR et XOVER ne peuvent pas ne �tre refus�s en m�me temps
(2) A priori, on se basera sur le code de retour � l'entr�e

*/

/* Les fonctions */

extern int connect_server (char * /*host*/, int /*port*/);
extern void quit_server (void);
extern int read_server (char * /*ligne*/, int /*deb*/, int /*max*/);
extern int read_server_with_reconnect
                        (char * /*ligne*/, int /*deb*/, int /*max*/);
  /* appel pour post */
extern int raw_write_server (char * /*buf*/, unsigned int /*len*/);
extern int write_command (int /*num_com*/, int /*num_para*/, char ** /*param*/);
extern int reconnect_after_timeout(int /*refait_commande*/);
extern int discard_server(void);
extern int return_code (void);
extern int adjust_time(void);
extern int read_server_for_list (char*, int, int);

#endif
