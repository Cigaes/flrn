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

void Aff_ligne_comp_cmd (char *str, int len, int col) {
  Cursor_gotorc(Screen_Rows-1,col); Screen_erase_eol();
  Screen_write_nstring(str,len);
}

/* prefix : la chaine de départ sur laquelle on doit construire */
/* str : la chaine a étendre */
/* len : la taille totale, doit prendre en compte le prefixe */
/* truc, num, get_str : les possibilités d'extensions */
/* delim : les délimiteurs de l'extension */
/* result : si resultat=-1, tableau des resultats... */
/* resultat : >=0 grassouille, -1 plusieurs solutions, -2 aucune solution */
/* -3 : la chaine est vide...   */
/* on renvoie dans prefix ce qu'on a construit. */
int Comp_generic(Liste_Chaine *prefix, char *str, int len, void * truc,
  int num, char *get_str(void *, int ), char *delim, int *result)
{
  Liste_Chaine *courant=prefix;
  Liste_Chaine *nouveau;
  char *guess=NULL;
  char *cur=NULL;
  int prefix_len=0;
  int match=0;
  int i,j;
  int good=0;
  char *suite;

  suite=strtok(str,delim);
  if (suite==NULL) return -3;
  suite=strtok(NULL,"\0");
  if (suite) suite=safe_strdup(suite);

  for (i=0;i<num;i++)
    if (strncmp(str, cur=get_str(truc,i), strlen(str))==0) {
      if (strlen(cur)+strlen(prefix->une_chaine)+(suite ? strlen(suite) : 0)>=len) continue;
      if (match) {
        if (!prefix_len) prefix_len=strlen(guess);
        for (j=0;j<prefix_len;j++)
        if (guess[j] != cur[j]){
          prefix_len=j; break;
        }
      }
      guess=cur;
      good=i;
      match++;
      result[i]=1;
      nouveau=safe_calloc(1,sizeof(Liste_Chaine));
      nouveau->une_chaine=safe_malloc((len+1)*sizeof(char));
      strcpy(nouveau->une_chaine,prefix->une_chaine);
      strcat(nouveau->une_chaine,cur);
      strcat(nouveau->une_chaine," ");
      nouveau->complet=1;
      nouveau->suivant=courant->suivant;
      courant->suivant=nouveau;
      courant=nouveau;
    }
  if (match==1) {
    free(prefix->une_chaine);
    prefix->une_chaine=courant->une_chaine;
    prefix->suivant=courant->suivant;
    if (suite) strcpy(str,suite); else str[0]='\0';
    if (suite) free(suite);
    free(courant);
    return good;
  } else if (match>1) {
      prefix->complet=0;
      good=strlen(prefix->une_chaine);
      strncat(prefix->une_chaine,guess,prefix_len);
      prefix->une_chaine[good+prefix_len]='\0';
      if (suite) {
        strcat(prefix->une_chaine," ");
        strcpy(str,suite);
        strcat(prefix->une_chaine, suite);
        free(suite);
      } else str[0]='\0';
      return -1;
  }
  prefix->complet=-1;
  if (suite)
    free(suite);
  return -2;
}

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
       aff_ligne_comp(courant->une_chaine, strlen(courant->une_chaine)-(courant->complet), col);
       touche=Attend_touche();
       if (touche!='\011') break;
       courant=courant->suivant;
       if (courant==NULL) courant=debut;
     } while (1);
     courant->une_chaine[strlen(courant->une_chaine)-(courant->complet)]='\0';
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
