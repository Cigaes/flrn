/* flrn v 0.1                                                           */
/*              fl_help.c         10/12/97                              */
/*                                                                      */
/*  Gestion hyper-torchante de l'aide.				        */
/*                                                                      */
/*                                                                      */

#include <stdlib.h> 
#include <stdio.h>      
#include <strings.h>

#include "flrn.h"

static char hl_chars[]="0123456789mq";
static char hl_mesgs[]="Aide de flrn, à vous [0-9qm] :";

void Aide () {
   FILE *file;
   char key='m';

   while (key!='q') {
      file=open_flhelpfile(key);
      key='\0';
      if (file==NULL) { 
         Aff_error("Erreur : ne trouve pas le fichier d'aide cherché."); 
	 if (key=='m') return; 
      } else {
         key=Aff_file(file,hl_chars,hl_mesgs);
         fclose(file);
      }
      if (key==0) Aff_fin(hl_mesgs);
      while ((key==0) || (!strchr(hl_chars,key))) {
         key=(char)Attend_touche();
	 if (KeyBoard_Quit) key='q';
	 if (key=='h') key='m';
      } 
   }
}
