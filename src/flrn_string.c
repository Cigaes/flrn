/* flrn v 0.1                                                           */
/*              flrn_string.c           03/12/97                        */
/*                                                                      */
/*   Gestion des listes de chaines...                                   */
/*       et de scrollings.                                              */
/*                                                                      */



#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "flrn.h"
#include "flrn_string.h"

/* On fait ici toute la gestion des Lecture_List    */
/* Toutes les fonction renvoient 0 en cas de succes */
/* <0 en cas d'erreur.				    */

/* sauf alloue_chaine, qui fonctionne comme un malloc */

Lecture_List *alloue_chaine() {
   return (Lecture_List *)safe_calloc(1,sizeof(Lecture_List));
}

int free_chaine(Lecture_List *chaine) {
   Lecture_List *courant1, *courant2;

   courant1=chaine;
   while (courant1) {
      courant2=courant1->suivant;
      free(courant1);
      courant1=courant2;
   }
   return 0;
}


int ajoute_char(Lecture_List **chaine, int chr) {
    Lecture_List *courant;

    courant=*chaine;
    if (courant->size<MAX_READ_SIZE-1) {
       courant->lu[courant->size++]=chr;
    } else {
       courant->size=MAX_READ_SIZE-1;
       courant->lu[MAX_READ_SIZE-1]='\0';
       if (courant->suivant==NULL) {
          courant->suivant=alloue_chaine();
       } else courant->suivant->size=0;
       courant->suivant->prec=courant;
       courant=courant->suivant;
       courant->lu[courant->size++]=chr;
       *chaine=courant;
    }
    return 0;
}

int enleve_char(Lecture_List **chaine) {
    Lecture_List *courant;
    int res;

    courant=*chaine;
    if (courant->size!=0) {
       courant->size--;
       return 0;
    } else {
       if (courant->prec==NULL) return -1;
       if (courant->suivant) { free_chaine(courant->suivant); courant->suivant=NULL; }
       courant=courant->prec;
       res=enleve_char(&courant);
       *chaine=courant;
       return res;
    }
}

int str_cat (Lecture_List **chaine1, char *chaine) {
  char *ptr;
  for(ptr=chaine;*ptr;ptr++) ajoute_char(chaine1,*ptr);
  return 0;
}

#if 0
int str_cat (Lecture_List **chaine1, char *chaine) {
    int size_res, len_to_cop;
    Lecture_List *courant;
   
    courant=*chaine1;
    courant->lu[courant->size]='\0';
    len_to_cop=strlen(chaine);
    while (1) {
      size_res=(MAX_READ_SIZE-1)-courant->size;
      if (size_res>=len_to_cop) {
	 strcat(courant->lu, chaine);
	 courant->size+=len_to_cop;
	 *chaine1=courant;
	 return 0;
      } else {
	 strncat(courant->lu, chaine, size_res);
	 chaine+=size_res;
	 len_to_cop-=size_res;
	 courant->lu[MAX_READ_SIZE-1]='\0';
	 courant->size=MAX_READ_SIZE-1;
         if (courant->suivant==NULL) {
             courant->suivant=alloue_chaine();
         } else {
	    courant->suivant->lu[0]='\0';
	    courant->suivant->size=0;
	 }
         courant->suivant->prec=courant;
         courant=courant->suivant;
      }
   }
  return 0; /* pour les compilos naczes */
}
#endif


/* Cette fonction concatene chaine2 à partir de place dans chaine1 jusqu'au */
/* charactère char non inclus.						    */
int str_ch_cat(Lecture_List **chaine1, Lecture_List *chaine2, int place, char chr) {
    char *buf;
  
    while (chaine2->lu[place]!=chr) {
      buf=strchr(chaine2->lu+place,chr);
      if (buf!=NULL) {
	*buf='\0';
	str_cat(chaine1, chaine2->lu+place);
	*buf=chr;
	return 0;
      }
      else {
        str_cat(chaine1, chaine2->lu+place);
	chaine2=chaine2->suivant;
	if (chaine2==NULL) return -1;
	place=0;
      }
    }
    return 0;
}

    
/* Cette fonction renvoie le n-ieme caractere en partant de la fin */
char get_char(Lecture_List *chaine, int n) {
    
    if (chaine->size>=n) return chaine->lu[chaine->size-n];
      if (chaine->prec) return get_char(chaine->prec, n-chaine->size);
      else return 0;
}

/* recherche un caractère en partant de la fin et renvoie la place suivante */
/* (sert pour la recherche d'un debut de ligne...) */
void cherche_char(Lecture_List **chaine, int *place, char c) {
    Lecture_List *courant=*chaine;
    int s=courant->size-1;

    while (s>=0) {
      if (courant->lu[s]==c) {
         *chaine=courant;
	 *place=s+1;
         return;
      }
      s--;
      if ((s<0) && (courant->prec)) {
         courant=courant->prec;
	 s=courant->size-1;
      }
    }
    *chaine=courant;
    *place=0;
}
   
