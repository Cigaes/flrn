/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      slang_flrn.c : usage du langage de script de SLang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include "flrn.h"

static UNUSED char rcsid[]="$Id$";

#ifdef USE_SLANG_LANGUAGE

#include <stdio.h>
#include <stdlib.h>
#include <slang.h>

/* on vérifie tout de suite si la version de SLang est suffisante */
#if SLANG_VERSION < 10400
#error slang doit être de version 1.4 ou plus pour que flrn compile \
avec le langage de script.
#endif

#include "art_group.h"
#include "group.h"
#include "tty_display.h"
#include "tty_keyboard.h"

int flrn_SLang_inited=0;

/****************** Les types ************************/

/* groupe : Newsgroup_Type : pointeur vers un groupe */
static SLang_Class_Type *flrn_SLang_Newsgroup_Type;
#define NEWSGROUP_TYPE_NUMBER 128

/* article : Article_Type : structure MMT : pointeur vers article et groupe */
typedef struct s_article_and_group {
   Newsgroup_List *groupe;
   Article_List *article;
} Article_and_Group;
static SLang_Class_Type *flrn_SLang_Article_Type;
#define ARTICLE_TYPE_NUMBER 129

static char *Newsgroup_Type_string_callback(unsigned char type, VOID_STAR addr)
{
   Newsgroup_List *legroupe = (Newsgroup_List *)addr;
   if ((legroupe==NULL) || (legroupe->name==NULL)) return SLmake_string("");
   return SLmake_string(legroupe->name);
}
static void Newsgroup_Type_destroy_callback(unsigned char type, VOID_STAR addr)
{
   return; /* on ne détruit rien : la structure de groupe reste jusqu'au bout 
            * en fait, l'idéal aurait été de tout faire en MMT, mais bon ...  */
}
static int Newsgroup_Type_push_callback(unsigned char type, VOID_STAR addr)
{
   return SLclass_push_ptr_obj(type, addr);
}
static int Newsgroup_Type_pop_callback(unsigned char type, VOID_STAR addr)
{
   return SLclass_pop_ptr_obj(type, addr);
}

static char *Article_Type_string_callback(unsigned char type, VOID_STAR addr)
{
   Article_and_Group *larticle = (Article_and_Group *)addr;
   if ((larticle==NULL) || (larticle->article==NULL) ||
       (larticle->article->msgid==NULL)) return SLmake_string("");
   return SLmake_string(larticle->article->msgid);
}
static void Article_Type_destroy_callback(unsigned char type, VOID_STAR addr)
{
   Article_and_Group *larticle = (Article_and_Group *)addr;
   if (larticle) free(larticle);
   return; 
}

int Push_article_on_stack (Article_List *article, Newsgroup_List *groupe) {
   SLang_MMT_Type *bla;
   Article_and_Group *larticle;

   larticle=safe_malloc(sizeof(Article_and_Group));
   larticle->article=article;
   larticle->groupe=groupe;
   bla = SLang_create_mmt(ARTICLE_TYPE_NUMBER,VOID_STAR larticle);
   if (SLang_push_mmt(bla)) return 0;
   else {
     SLang_free_mmt(bla);
     return -1;
   }
}

/***************** Les variables *****************************/

static SLang_MMT_Type *flrn_SLang_article_courant_mmt;
static Article_and_Group flrn_SLang_article_courant;
static Newsgroup_List *flrn_SLang_newsgroup_courant;

static SLang_Intrin_Var_Type flrn_Intrin_Vars [] =
{
   MAKE_VARIABLE("Article_Courant", &flrn_SLang_article_courant_mmt, 
                           ARTICLE_TYPE_NUMBER, 1),
   MAKE_VARIABLE("Newsgroup_Courant", &flrn_SLang_newsgroup_courant,
   			   NEWSGROUP_TYPE_NUMBER, 1),
   SLANG_END_TABLE
};


/***************** Les fonctions intrinsèques ****************/

char *intrin_get_header(Article_and_Group *larticle, char *name) {
    if ((larticle==NULL) || (larticle->article==NULL)) return "";
    return get_one_header(larticle->article, larticle->groupe, name);
}

SLang_Intrin_Fun_Type flrn_Intrin_Fun [] =
{
   MAKE_INTRINSIC_2("get_header", intrin_get_header, SLANG_STRING_TYPE,
   				ARTICLE_TYPE_NUMBER, SLANG_STRING_TYPE),
   SLANG_END_TABLE
};

/***************** Les "hooks" : error_SLang_hook ***************/
/***************** et vmessage_SLang_hook ***********************/
/* TODO : à faire mieux...					*/

/* cette fonction est *temporaire* et ne devrait pas être utilisée */
void vmessage_SLang_Hook (char *fmt, va_list ap)
{
   char buf[200];
   /* on "triche", en prenant des risques... avec vsprintf... */
   vsprintf(buf, fmt, ap);
   Aff_error(buf);
   Aff_fin("Appuyez sur une touche...");
   Attend_touche();
}

/* cette fonction est aussi plus ou moins temporaire, mais elle */
/* fait plus ou moins ce que je pense être le mieux...          */
void error_SLang_Hook (char *str)
{
   Aff_error(str);
   Aff_fin("Appuyez sur une touche...");
   Attend_touche();
}
    

/***************** Les fonctions de bases ***************/

/* initialisation des fonctions de SLang. retour : -1 si erreur */
int flrn_init_SLang(void) {
   if (-1 == SLang_init_all ()) {
     fprintf(stderr,"Erreur d'initialisation des fonctions SLang de base.\n");
     return -1;
   }
   /* les types */
   flrn_SLang_Newsgroup_Type=SLclass_allocate_class("Newsgroup_Type");
   flrn_SLang_Article_Type=SLclass_allocate_class("Article_Type");
   if ((-1 == SLclass_set_string_function(flrn_SLang_Newsgroup_Type,
                       Newsgroup_Type_string_callback)) ||
       (-1 == SLclass_set_push_function(flrn_SLang_Newsgroup_Type,
                       Newsgroup_Type_push_callback)) ||
       (-1 == SLclass_set_pop_function(flrn_SLang_Newsgroup_Type,
                       Newsgroup_Type_pop_callback)) ||
       (-1 == SLclass_set_destroy_function(flrn_SLang_Newsgroup_Type,
                       Newsgroup_Type_destroy_callback)) ||
       (-1 == SLclass_register_class(flrn_SLang_Newsgroup_Type,
              NEWSGROUP_TYPE_NUMBER, sizeof(void *), SLANG_CLASS_TYPE_PTR)))
   {
     fprintf(stderr,"SLang : erreur d'initialisation du type %s.\n",
                    "Newsgroup");
     return -1;
   }
   if ((-1 == SLclass_set_string_function(flrn_SLang_Article_Type,
                       Article_Type_string_callback)) ||
       (-1 == SLclass_set_destroy_function(flrn_SLang_Article_Type,
                       Article_Type_destroy_callback)) ||
       (-1 == SLclass_register_class(flrn_SLang_Article_Type,
              ARTICLE_TYPE_NUMBER, sizeof(void *), SLANG_CLASS_TYPE_MMT)))
   {
     fprintf(stderr,"SLang : erreur d'initialisation du type %s.\n",
                    "Article");
     return -1;
   }
   /* les variables */
   flrn_SLang_article_courant.article=NULL;
   flrn_SLang_article_courant.groupe=NULL;
   flrn_SLang_newsgroup_courant=NULL;
   if (NULL == (flrn_SLang_article_courant_mmt = SLang_create_mmt
            (ARTICLE_TYPE_NUMBER, (VOID_STAR) &flrn_SLang_article_courant)))
   {
     fprintf(stderr, "SLang : erreur dans la création d'un MMT.");
     return -1;
   }
   SLang_inc_mmt(flrn_SLang_article_courant_mmt);
   if (-1 == SLadd_intrin_var_table (flrn_Intrin_Vars, NULL))
   {
     fprintf(stderr,"SLang : erreur dans le chargement des %s.\n","variables");
     return -1;
   }
   if (-1 == SLadd_intrin_fun_table (flrn_Intrin_Fun, NULL))
   {
     fprintf(stderr,"SLang : erreur dans le chargement des %s.\n","fonctions");
     return -1;
   }
   flrn_SLang_inited=1;
   return 0;
}

/* lecture d'une chaîne, utilisée dans certains cas. */
/* retour de -1 si erreur */
/* ATTENTION : la chaîne *result ne doit pas être modifiée et doit être 
 * libérée avec un appel à SLang_free_slstring */
int source_SLang_string(char *str, char **result)
{
   flrn_SLang_article_courant.article=Article_courant;
   flrn_SLang_article_courant.groupe=Newsgroup_courant;
   flrn_SLang_newsgroup_courant=Newsgroup_courant;
   *result=NULL;
   if (flrn_SLang_inited==0) return -1;
   if ((-1 == SLang_load_string(str)) ||  (-1 == SLang_pop_slstring(result))) {
      SLang_restart (1);
      SLang_Error = 0;
      return -1;
   }
   return 0;
}

/* lecture d'un fichier, utilisation de base */
/* retour de -1 si erreur */
int source_SLang_file (char *str) 
{
/* comme il s'agit d'une lecture d'options, en aucun cas cela ne doit faire
 * une véritable commande... */
   flrn_SLang_article_courant.article=NULL;
   flrn_SLang_article_courant.groupe=NULL;
   flrn_SLang_newsgroup_courant=NULL;
   if (-1 == SLang_load_file(str)) {
      SLang_restart(1);
      SLang_Error = 0;
      return -1;
   }
   return 0;
}

/* change SLang_Error_Hook selon ce qu'on cherche : si paramètre=1, on passe
 * en mode slang et le message d'erreur est proche d'un message d'erreur
 * de flrn. Si paramètre=0, on quitte le mode graphique, et SLang_Error_Hook
 * devient NULL, ce qui correspond à une écriture classique sur stderr.
 * Ce programme change aussi SLang_VMessage_Hook */
void change_SLang_Error_Hook (int param)
{
   if (param) {
      SLang_Error_Hook = &error_SLang_Hook;
      SLang_VMessage_Hook = &vmessage_SLang_Hook;
   } else {
      SLang_Error_Hook = NULL;
      SLang_VMessage_Hook = NULL;
   }
}

int Parse_type_fun_slang(char *str_int) {
    return strtol(str_int, NULL, 10);
}

extern SLang_Name_Type *Parse_fun_slang (char *str, int *num) {
    char *comma;
    SLang_Name_Type *result;
    comma=strchr(str,',');
    if (comma) {
       *comma='\0';
       *num=Parse_type_fun_slang(comma+1);
    } else *num=0;
    result=SLang_get_function(str);
    if (comma) *comma=',';
    return result;
}

#endif
