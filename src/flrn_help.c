/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_help.c : aide
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdlib.h> 
#include <stdio.h>      
#include <strings.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "flrn_files.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_help.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* TODO : à revoir pour cause des entrées */
static char hl_chars[]="0123456789mq";
static char hl_mesgs[]=N_("Aide de flrn, Ã  vous [0-9qm] :");

void Aide () {
   FILE *file;
   int key=(int) 'm';
   struct key_entry ke;
   int rc;
   flrn_char *trad;

   rc=conversion_from_utf8(_(hl_mesgs),&trad,0,(size_t)(-1));

   ke.entry=ENTRY_ERROR_KEY;
   while (key!=(int)'q') {
      file=open_flhelpfile(key);
      key=0;
      if (file==NULL) { 
         num_help_line=16;
         Aff_error_utf8(
	   _("Erreur : ne trouve pas le fichier d'aide cherchÃ©.")); 
	 if (key==(int)'m') break;
      } else {
         Free_key_entry(&ke);
         Aff_file(file,hl_chars,trad,&ke);
	 if (ke.entry!=ENTRY_SLANG_KEY) key=-1;
	    else key=ke.value.slang_key;
         fclose(file);
         num_help_line=16;
         Aff_help_line(Screen_Rows-1);
      }
      if ((key==0) || (!strchr(hl_chars,key))) Aff_fin(trad);
      while ((key==0) || (!strchr(hl_chars,key))) {
         Attend_touche(&ke);
	 if (ke.entry!=ENTRY_SLANG_KEY) key=-1;
	      else key=ke.value.slang_key;
	 if (KeyBoard_Quit) key=(int)'q';
	 if (key==(int)'h') key=(int)'m';
      } 
   }
   if (rc==0) free(trad);
   Free_key_entry(&ke);
}
