
#ifndef FLRN_MENUS_H
#define FLRN_MENUS_H

/* La structure utilis�e est pour l'instant celle-ci. Je ne sais pas si */
/* c'est la meilleure, mais bon... On verra bien. Peut-�tre rajouter un */
/* lien vers une fonction d'expression de l'objet... Ou non, le mieux   */
/* est de le passer en param�tre de la fonction de menu.	        */
typedef struct liste_menu_desc {
   char *nom;  /* En read-only, peut �tre un pointeur vers n'importe ou */
   void *lobjet; /* Ceci n'est pas lib�r� non plus !!! */
   struct liste_menu_desc *prec, *suiv;
} Liste_Menu;

#endif
