#ifndef FLRN_COMP_H
#define FLRN_COMP_H

typedef struct liste_de_chaine {
  char *une_chaine;
  int complet;
  struct liste_de_chaine *suivant;
} Liste_Chaine;


extern void Aff_ligne_comp_cmd (char *, int , int );

extern int Comp_generic(Liste_Chaine *, char *, int , void *,
	  int , char *(void *, int ), char *, int *);


extern int Comp_general_command (char *, int , int,
      int (char *, int, Liste_Chaine *), 
      void (char *, int, int));

#endif
