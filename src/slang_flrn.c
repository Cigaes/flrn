/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      slang_flrn.c : usage du langage de script de SLang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#include "flrn.h"

#ifdef USE_SLANG_LANGUAGE

#include <slang.h>

int flrn_SLang_inited=0;

char *intrin_get_header(int *num, char *name) {
   return "Un truc pipo.";
}

SLang_Intrin_Fun_Type flrn_Intrinsics [] =
{
   MAKE_INTRINSIC_2("get_header", intrin_get_header, SLANG_STRING_TYPE,
   				SLANG_INT_TYPE, SLANG_STRING_TYPE),
   SLANG_END_TABLE
};


/* initialisation des fonctions de SLang. retour : -1 si erreur */
int flrn_init_SLang(void) {
   if (-1 == SLang_init_all ()) 
     return -1;
   if (-1 == SLadd_intrin_fun_table (flrn_Intrinsics, NULL))
     return -1;
   flrn_SLang_inited=1;
   return 0;
}

/* lecture d'une chaîne, utilisée dans certains cas. */
/* retour de -1 si erreur */
/* ATTENTION : la chaîne *result ne doit pas être modifiée et doit être 
 * libérée avec un appel à SLang_free_slstring */
int source_SLang_string(char *str, char **result) {

   *result=NULL;
   if (flrn_SLang_inited==0) return -1;
   if ((-1 == SLang_load_string(str)) ||  (-1 == SLang_pop_slstring(result))) {
      SLang_restart (1);
      SLang_Error = 0;
      return -1;
   }
   return 0;
}

#endif
