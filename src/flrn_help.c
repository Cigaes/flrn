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

static char hl_chars[]="0123456789mq";
static char hl_mesgs[]="Aide de flrn, à vous [0-9qm] :";

void Aide () {
   FILE *file;
   char key='m';

   while (key!='q') {
      file=open_flhelpfile(key);
      key='\0';
      if (file==NULL) { 
         num_help_line=16;
         Aff_error("Erreur : ne trouve pas le fichier d'aide cherché."); 
	 if (key=='m') return; 
      } else {
         key=Aff_file(file,hl_chars,hl_mesgs);
         fclose(file);
         num_help_line=16;
         Aff_help_line(Screen_Rows-1);
      }
      if (key==0) Aff_fin(hl_mesgs);
      while ((key==0) || (!strchr(hl_chars,key))) {
         key=(char)Attend_touche();
	 if (KeyBoard_Quit) key='q';
	 if (key=='h') key='m';
      } 
   }
}
