/* flrn v 0.4                                                           */
/*              flrn_comp.c          29/02/99                           */
/*                                                                      */
/*  Complétions dans flrn                                               */
/*                                                                      */
/*                                                                      */

#include <stdlib.h>
#include "flrn.h"
#include "flrn_slang.h"
#include "flrn_comp.h"
#include "tty_keyboard.h"

/* retour : 0 -> cool -1 : erreur (pas de omplétion) >0 : touche frappée */
int Comp_general_command (char *str, int len, int col,
          int comp_cmd(char *, int, Liste_Chaine *), 
	  void aff_ligne_comp(char *, int, int)) {
  Liste_Chaine *debut, *courant;
  int res, touche;
  char *sauf;

  sauf=safe_strdup(str);
  debut=safe_calloc(1,sizeof(Liste_Chaine));
  debut->une_chaine=safe_malloc(len);
  debut->une_chaine[0]='\0';
  res=comp_cmd(sauf, len, debut);
  free(sauf);
  if (res==0) {
     strcpy(str,debut->une_chaine);
     free(debut->une_chaine);
     free(debut);
     return 0;
  } else if (res==-1) {
     courant=debut;
     do {
       aff_ligne_comp(courant->une_chaine, strlen(courant->une_chaine)-1, col);
       touche=Attend_touche();
       if (touche!='\011') break;
       courant=courant->suivant;
       if (courant==NULL) courant=debut;
     } while (1);
     courant->une_chaine[strlen(courant->une_chaine)-1]='\0';
     strcpy(str,courant->une_chaine);
     courant=debut;
     while (courant) {
        free(courant->une_chaine);
	debut=courant->suivant;
	free(courant);
	courant=debut;
     }
     return touche;
  } else {
     free(debut->une_chaine);
     free(debut);
     Screen_beep();
     return -1;
  }
}
