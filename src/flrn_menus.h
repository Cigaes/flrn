
#ifndef FLRN_MENUS_H
#define FLRN_MENUS_H

/* La structure utilisée est pour l'instant celle-ci. Je ne sais pas si */
/* c'est la meilleure, mais bon... On verra bien. Peut-être rajouter un */
/* lien vers une fonction d'expression de l'objet... Ou non, le mieux   */
/* est de le passer en paramètre de la fonction de menu.	        */
typedef struct liste_menu_desc {
   char *nom;  /* En read-only, peut être un pointeur vers n'importe ou */
   void *lobjet; /* Ceci n'est pas libéré non plus !!! */
   struct liste_menu_desc *prec, *suiv;
} Liste_Menu;

#endif
