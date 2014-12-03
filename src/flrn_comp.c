/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_comp.c : complétion automatique dans flrn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdlib.h>
#include "flrn.h"
#include "flrn_slang.h"
#include "flrn_comp.h"
#include "tty_keyboard.h"
#include "enc_strings.h"

static UNUSED char rcsid[]="$Id$";

void Aff_ligne_comp_cmd (flrn_char *str, size_t len, int col) {
  int rc;
  char *trad=NULL;
  rc=conversion_to_terminal(str,&trad,0,len);
  Cursor_gotorc(Screen_Rows2-1,col); Screen_erase_eol();
  Screen_write_string(trad);
  if (rc==0) free(trad);
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
/* case_sen=1 : on est case_sensitive */
/* Attention : get_str peut renvoyer une chaîne statique, ou un pointeur
 * vers un paramètre statique de get_str. Donc on ne doit pas réutiliser
 * un vieux get_str */
int Comp_generic(Liste_Chaine *prefix, flrn_char *str, size_t len, void * truc,
  int num, flrn_char *get_str(void *, int ), flrn_char *delim, int *result,
  int case_sen)
{
  Liste_Chaine *courant=prefix;
  Liste_Chaine *nouveau;
  flrn_char *guess=NULL;
  flrn_char *cur=NULL;
  int prefix_len=0;
  int match=0;
  int i,j;
  int good=0;
  flrn_char *suite, *dummy;

  suite=fl_strtok_r(str,delim,&dummy);
  if (suite==NULL) return -3;
  suite=fl_strtok_r(NULL,fl_static("\0"),&dummy);
  if (suite) suite=safe_flstrdup(suite);

  for (i=0;i<num;i++)
    if (((case_sen) && (fl_strncmp(str, cur=get_str(truc,i),
			 fl_strlen(str))==0))
        || ((!case_sen) && 
	      (fl_strncasecmp(str, cur=get_str(truc,i), fl_strlen(str))==0)))
    {
      if (fl_strlen(cur)+fl_strlen(prefix->une_chaine)+
	      (suite ? fl_strlen(suite) : 0)>=len) continue;
      if (match) {
        if (!prefix_len) prefix_len=fl_strlen(guess)-1;
        for (j=0;j<prefix_len;j++)
        if (guess[j] != cur[j]){
          prefix_len=j; break;
        }
      }
      good=i;
      match++;
      result[i]=1;
      nouveau=safe_calloc(1,sizeof(Liste_Chaine));
      nouveau->une_chaine=safe_malloc((len+1)*sizeof(flrn_char));
      fl_strcpy(nouveau->une_chaine,prefix->une_chaine);
      guess=nouveau->une_chaine+fl_strlen(nouveau->une_chaine);
      fl_strcpy(guess,cur);
      fl_strcat(guess,fl_static(" "));
      nouveau->complet=1;
      nouveau->suivant=courant->suivant;
      courant->suivant=nouveau;
      courant=nouveau;
    }
  if (match==1) {
    free(prefix->une_chaine);
    prefix->une_chaine=courant->une_chaine;
    prefix->suivant=courant->suivant;
    if (suite) fl_strcpy(str,suite); else str[0]=fl_static('\0');
    if (suite) free(suite);
    free(courant);
    return good;
  } else if (match>1) {
      prefix->complet=0;
      good=fl_strlen(prefix->une_chaine);
      fl_strncat(prefix->une_chaine,guess,prefix_len);
      prefix->une_chaine[good+prefix_len]=fl_static('\0');
      if (suite) {
        fl_strcat(prefix->une_chaine,fl_static(" "));
        fl_strcpy(str,suite);
        fl_strcat(prefix->une_chaine, suite);
        free(suite);
      } else str[0]=fl_static('\0');
      return -1;
  }
  prefix->complet=-1;
  if (suite)
    free(suite);
  return -2;
}

/* retour : 0 -> cool -1 : erreur (pas de omplétion) >0 : touche frappée */
int Comp_general_command (flrn_char *str, int len, int col,
          int comp_cmd(flrn_char *, size_t, Liste_Chaine *), 
	  void aff_ligne_comp(flrn_char *, size_t, int),
	  struct key_entry *key) {
  Liste_Chaine *debut, *courant;
  int res;
  flrn_char *sauf;

  sauf=safe_flstrdup(str);
  debut=safe_calloc(1,sizeof(Liste_Chaine));
  debut->une_chaine=safe_malloc(len*sizeof(flrn_char));
  debut->une_chaine[0]=fl_static('\0');
  res=comp_cmd(sauf, len, debut);
  free(sauf);
  if (res==0) {
     fl_strcpy(str,debut->une_chaine);
     free(debut->une_chaine);
     free(debut);
     return 0;
  } else if (res==-1) {
     courant=debut;
     do {
       aff_ligne_comp(courant->une_chaine, 
	       fl_strlen(courant->une_chaine)-(courant->complet), col);
       Attend_touche(key);
       if ((key->entry!=ENTRY_SLANG_KEY) || 
	       (key->value.slang_key!=(int)'\011'))
	   break;
       courant=courant->suivant;
       if (courant==NULL) courant=debut;
     } while (1);
     courant->une_chaine[fl_strlen(courant->une_chaine)-
	 (courant->complet)]=fl_static('\0');
     fl_strcpy(str,courant->une_chaine);
     courant=debut;
     while (courant) {
        free(courant->une_chaine);
	debut=courant->suivant;
	free(courant);
	courant=debut;
     }
     return 1;
  } else {
     free(debut->une_chaine);
     free(debut);
     Screen_beep();
     return -1;
  }
}
