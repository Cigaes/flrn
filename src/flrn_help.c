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

static UNUSED char rcsid[]="$Id$";

/* TODO : à revoir pour cause des entrées */
static char hl_chars[]="0123456789mq";
static flrn_char hl_mesgs[]=fl_static("Aide de flrn, à vous [0-9qm] :");

void Aide () {
   FILE *file;
   int key=(int) 'm';
   struct key_entry ke;

   ke.entry=ENTRY_ERROR_KEY;
   while (key!=(int)'q') {
      file=open_flhelpfile(key);
      key=0;
      if (file==NULL) { 
         num_help_line=16;
	 /* TODO : français */
         Aff_error(
	   fl_static("Erreur : ne trouve pas le fichier d'aide cherchÃ©.")); 
	 if (key==(int)'m') { Free_key_entry(&ke); return; }
      } else {
         Aff_file(file,hl_chars,hl_mesgs,&ke);
	 if (ke.entry!=ENTRY_SLANG_KEY) key=-1;
	    else key=ke.value.slang_key;
         fclose(file);
         num_help_line=16;
         Aff_help_line(Screen_Rows-1);
      }
      if (key==0) Aff_fin(hl_mesgs);
      while ((key==0) || (!strchr(hl_chars,key))) {
         Attend_touche(&ke);
	 if (ke.entry!=ENTRY_SLANG_KEY) key=-1;
	      else key=ke.value.slang_key;
	 if (KeyBoard_Quit) key=(int)'q';
	 if (key==(int)'h') key=(int)'m';
      } 
   }
   Free_key_entry(&ke);
}
