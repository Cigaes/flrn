#ifndef FLRN_COMP_H
#define FLRN_COMP_H

typedef struct liste_de_chaine {
  char *une_chaine;
  int complet;
  struct liste_de_chaine *suivant;
} Liste_Chaine;


extern int Comp_general_command (char *, int , int,
      int (char *, int, Liste_Chaine *), 
      void (char *, int, int));

#endif
