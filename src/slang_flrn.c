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
#include "flrn_inter.h"
#include "enc/enc_strings.h"

int flrn_SLang_inited=0;
char *retour_intrinsics=NULL;

/****************** I : Les types ************************/

/********** Groupe *************/

/**********************/
/* Définition du type */
/**********************/
/* groupe : Newsgroup_Type : pointeur vers un groupe */
static SLang_Class_Type *flrn_SLang_Newsgroup_Type;
#define NEWSGROUP_TYPE_NUMBER 128

/* louze : on ne peut pas pusher NULL comme pointeur de groupe à cause
 * d'une craderie de SLang sur la façon dont il passe les paramètres
 * aux fonctions intrinsèques. d'où la création d'un groupe dummy */
Newsgroup_List dummy_group = { NULL, NULL, 0, fl_static(""),  NULL, -1, -1, -1, 
                               -1, -1, NULL, 0, NULL, NULL, NULL, NULL };

/* Callback associés : -> string : donne le nom */
static char *Newsgroup_Type_string_callback(unsigned char type, VOID_STAR addr)
{
   Newsgroup_List *legroupe = *((Newsgroup_List **)addr);
   char buf[MAX_NEWSGROUP_LEN+1], *ptr=&(buf[0]);
   if ((legroupe==NULL) || (legroupe->name==NULL)) return SLmake_string("");
   conversion_to_file(legroupe->name,&ptr,MAX_NEWSGROUP_LEN,(size_t)(-1));
   return SLmake_string(ptr);
}
static void Newsgroup_Type_destroy_callback(unsigned char type, VOID_STAR addr)
{
   return; /* on ne détruit rien : la structure de groupe reste jusqu'au bout 
            * en fait, l'idéal aurait été de tout faire en MMT, mais bon ...  */
}
static int Newsgroup_Type_push_callback(unsigned char type, VOID_STAR addr)
{
   Newsgroup_List *legroupe = *((Newsgroup_List **)addr);
   return SLclass_push_ptr_obj(type, legroupe); 
}
static int Newsgroup_Type_pop_callback(unsigned char type, VOID_STAR addr)
{
   return SLclass_pop_ptr_obj(type, (VOID_STAR *)addr); 
}

int Push_newsgroup_on_stack (Newsgroup_List *groupe) {
   if (groupe)
     return SLclass_push_ptr_obj (NEWSGROUP_TYPE_NUMBER, (VOID_STAR) groupe);
   return SLclass_push_ptr_obj 
        (NEWSGROUP_TYPE_NUMBER, (VOID_STAR) (&dummy_group));
}

/* méthodes intrinsèques */
/* flags de la fonction */
int intrin_get_flags_group (Newsgroup_List *group) {
   /* group = (&dummy_group) ? */
   return (int)(group->flags);
}
/* description */
char *intrin_get_description_group (Newsgroup_List *group) {
   int rc;
   if (retour_intrinsics) free(retour_intrinsics);
   rc=conversion_to_file(group->description,&retour_intrinsics,
	   0,(size_t)(-1));
   if (rc!=0) retour_intrinsics=safe_strdup(retour_intrinsics);
   /* group = (&dummy_group) ? */
   return retour_intrinsics;
}

/* Liste des groupes. Pour l'instant on ignore l'argument entier. On
 * renvoie le nombre de groupes trouvés */
int intrin_get_groupes (char *nom, int *flags) {  
    int number=0;
    regex_t reg;
    void *bla=NULL;
    flrn_char *trad;
    int rc;

    int no_order(flrn_char *u1, void *u2) { return 0; }
    int igg_ajoute_elem (Newsgroup_List *grp, int flags2, int order,
	                  void **retour) {
	if (grp==NULL) return 0;
	Push_newsgroup_on_stack(grp);
	number++;
	return 0;
    }

    rc=conversion_from_file(nom,&trad,0,(size_t)(-1));
    cherche_newsgroups_in_list (trad, reg, 8, &igg_ajoute_elem,
	                                     &no_order, &bla);
    if (rc==0) free(trad);
    return number;
}

int intrin_menu_groups (int *flags,int *num) {
    char *fun_label_str, *str;
    flrn_char *flstr;
    int to_free,ret,numero,rc;
    Newsgroup_List *grp;
    SLang_Name_Type *fun_label=NULL;
    Liste_Menu *lemenu=NULL, *courant=NULL;

    if ((*flags) & 1) {
	ret = SLang_pop_string(&fun_label_str, &to_free);
        if (ret<0) return -1;
        fun_label=SLang_get_function(fun_label_str);
	if (to_free) SLfree(fun_label_str);
    }
    for (numero=0; numero<*num; numero++) {
        if (SLclass_pop_ptr_obj(NEWSGROUP_TYPE_NUMBER, (VOID_STAR *)&grp)<0)
	    break;
	if (fun_label) {
	    Push_newsgroup_on_stack(grp);
            if (SLexecute_function(fun_label)==-1) {
               SLang_restart (1);
               SLang_Error = 0;
	       str = safe_strdup(grp->name);
	    } else {
	       ret = SLpop_string(&str);
	       if (ret<0) flstr = safe_flstrdup(grp->name);
	       else {
		   rc=conversion_from_file(str,&flstr,0,(size_t)(-1));
		   if (rc==0) free(str); /* on libère *l'autre* chaine */
	       }
            }
	} else flstr = safe_flstrdup(grp->name); /* TODO : correct that */
	courant = ajoute_menu_ordre(&lemenu, &fmt_option_menu, 
		/*  FIXME : menu option ! */
		&str, (void *) grp,0,0);
	free(str);
	if (lemenu==NULL) lemenu=courant;
   }
   grp = Menu_simple (lemenu, lemenu, NULL, NULL, "Menu de groupes");
   Libere_menu(lemenu);
   lemenu = NULL;
   if (grp!=NULL) side_effect_of_slang_command(1,grp,0,0,0);
   return 0;
}


/* article : Article_Type : structure MMT : pointeur vers article et groupe */
typedef struct s_article_and_group {
   Newsgroup_List *groupe;
   Article_List *article;
} Article_and_Group;
static SLang_Class_Type *flrn_SLang_Article_Type;
#define ARTICLE_TYPE_NUMBER 129

static char *Article_Type_string_callback(unsigned char type, VOID_STAR addr)
{
   SLang_MMT_Type *bla = *((SLang_MMT_Type **)addr);
   Article_and_Group *larticle = (Article_and_Group *)SLang_object_from_mmt(bla);
   if ((larticle==NULL) || (larticle->article==NULL) ||
       (larticle->article->msgid==NULL)) return SLmake_string("");
   return SLmake_string(larticle->article->msgid);
}
static void Article_Type_destroy_callback(unsigned char type, VOID_STAR addr)
{
   SLang_MMT_Type *bla = *((SLang_MMT_Type **)addr);
   Article_and_Group *larticle = 
             (Article_and_Group *)SLang_object_from_mmt(bla);
   if (larticle) free(larticle);
   return; 
}

int Push_article_on_stack (Article_List *article, Newsgroup_List *groupe) {
   SLang_MMT_Type *bla;
   Article_and_Group *larticle;

   larticle=safe_malloc(sizeof(Article_and_Group));
   larticle->article=article;
   larticle->groupe=groupe;
   bla = SLang_create_mmt(ARTICLE_TYPE_NUMBER,(VOID_STAR) larticle);
   if (SLang_push_mmt(bla)<0)
   {
     SLang_free_mmt(bla);
     return -1;
   } else return 0;
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
    int rc, tofree;
    flrn_char *bla;
    if ((larticle==NULL) || (larticle->article==NULL)) return "";
    if (retour_intrinsics) free(retour_intrinsics);
    bla=get_one_header(larticle->article, larticle->groupe, name, &tofree);
    if (bla) {
      rc=conversion_to_file(bla,&retour_intrinsics,0,(size_t)(-1));
      if (rc!=0) retour_intrinsics=safe_strdup(retour_intrinsics);
      if (tofree) free(bla);
    } else return "";
    return retour_intrinsics;
}

SLang_Intrin_Fun_Type flrn_Intrin_Fun [] =
{
/*** Articles ***/
/* 1) obtenir l'entête d'un article */
   MAKE_INTRINSIC_2("get_header", intrin_get_header, SLANG_STRING_TYPE,
   				ARTICLE_TYPE_NUMBER, SLANG_STRING_TYPE),
/*** Groupes  ***/
/* 1) obtenir l'état actuel d'un groupe */
   MAKE_INTRINSIC_1("get_flags_group", intrin_get_flags_group, SLANG_INT_TYPE,
	                        NEWSGROUP_TYPE_NUMBER),
/* 2) obtenir la description d'un groupe */
   MAKE_INTRINSIC_1("get_description_group", intrin_get_description_group,
	        SLANG_STRING_TYPE, NEWSGROUP_TYPE_NUMBER),
/* 3) recherche de groupes */
   MAKE_INTRINSIC_2("get_groupes", intrin_get_groupes,
	        SLANG_INT_TYPE, SLANG_STRING_TYPE, SLANG_INT_TYPE), 
/* 4) menu sur des groupes */
   MAKE_INTRINSIC_2("menu_groups", intrin_menu_groups, SLANG_INT_TYPE,
	                        SLANG_INT_TYPE,SLANG_INT_TYPE),
/*** Divers ***/
#if 0
/* 1) obtention d'une option (-> renvoie le nombre de résultats) */
   MAKE_INTRINSIC_1("get_option", intrin_get_option, SLANG_DATATYPE_TYPE,
	                          SLANG_STRING_TYPE),
/* 2) nom du programme */
   MAKE_INTRINSIC_0("get_program_name", intrin_get_program_name,
	             SLANG_STRING_TYPE),
/* 3) set option */
   MAKE_INTRINSIC_2("set_option", intrin_set_option, SLANG_INT_TYPE,
	             SLANG_STRING_TYPE, SLANG_INT_TYPE),
#endif
   SLANG_END_TABLE
};

/***************** Les "hooks" : error_SLang_hook ***************/
/***************** et vmessage_SLang_hook ***********************/
/* TODO : à faire mieux...					*/

/* cette fonction est *temporaire* et ne devrait pas être utilisée */
void vmessage_SLang_Hook (char *fmt, va_list ap)
{
   char buf[200]; 
   int rc; flrn_char *trad;
   /* on "triche", en prenant des risques... avec vsprintf... */
   /* FIXME: une version correcte utilierait vsnprintf */
   vsprintf(buf, fmt, ap);
   rc=conversion_from_file(buf,&trad,0,(size_t)(-1));
   Aff_error(trad);
   if (rc==0) free(trad);
   /* FIXME : français */
   Aff_fin(fl_static("Appuyez sur une touche..."));
   Attend_touche(NULL);
}

void vmessage_SLang_Hook2 (char *fmt, va_list ap)
{
   char buf[200];
   int rc; flrn_char *trad;
   /* on "triche", en prenant des risques... avec vsprintf... */
   /* FIXME: une version correcte utilierait vsnprintf */
   vsprintf(buf, fmt, ap);
   rc=conversion_from_file(buf,&trad,0,(size_t)(-1));
   Aff_error_fin(trad,0,0);
   if (rc==0) free(trad);
   Attend_touche(NULL);
}

/* cette fonction est aussi plus ou moins temporaire, mais elle */
/* fait plus ou moins ce que je pense être le mieux...          */
void error_SLang_Hook (char *str)
{
   flrn_char *trad;
   int rc;
   rc=conversion_from_file(str,&trad,0,(size_t)(-1));
   Aff_error(trad);
   if (rc==0) free(trad);
   /* FIXME : français */
   Aff_fin(fl_static("Appuyez sur une touche..."));
   Attend_touche(NULL);
}

void error_SLang_Hook2 (char *str)
{
   flrn_char *trad;
   int rc;
   rc=conversion_from_file(str,&trad,0,(size_t)(-1));
   Aff_error_fin(trad,0,0);
   if (rc==0) free(trad);
   Attend_touche(NULL);
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
   flrn_SLang_newsgroup_courant=&dummy_group;
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
int source_SLang_string(flrn_char *str, flrn_char **result)
{
   int rc;
   char *trad;
   char **rst=NULL;
   flrn_SLang_article_courant.article=Article_courant;
   flrn_SLang_article_courant.groupe=Newsgroup_courant;
   flrn_SLang_newsgroup_courant=(Newsgroup_courant ?
	                             Newsgroup_courant : &dummy_group);
   *result=NULL;
   if (flrn_SLang_inited==0) return -1;
   rc=conversion_to_file(str,&trad,0,(size_t)(-1));
   if ((-1 == SLang_load_string(trad)) ||
	   (-1 == SLang_pop_slstring(rst))) {
       if (retour_intrinsics) {
	   free(retour_intrinsics);
	   retour_intrinsics=NULL;
       }
       if (rc==0) free(trad);
      SLang_restart (1);
      SLang_Error = 0;
      return -1;
   }
   if (retour_intrinsics) {
      free(retour_intrinsics);
      retour_intrinsics=NULL;
   }
   if (rc==0) free(trad);
   rc=conversion_to_file(*rst,result,0,(size_t)(-1));
   if (rc!=0) *result=safe_flstrdup(*result);
   SLang_free_slstring(*rst);
   return 0;
}

/* lecture d'un fichier, utilisation de base */
/* retour de -1 si erreur */
int source_SLang_file (char *str) 
{
    int ret;
/* comme il s'agit d'une lecture d'options, en aucun cas cela ne doit faire
 * une véritable commande... */
   flrn_SLang_article_courant.article=NULL;
   flrn_SLang_article_courant.groupe=NULL;
   flrn_SLang_newsgroup_courant=&dummy_group;
   ret=SLang_load_file(str);
   if (retour_intrinsics) {
      free(retour_intrinsics);
      retour_intrinsics=NULL;
   }
   if (-1 == ret) {
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
 * Ce programme change aussi SLang_VMessage_Hook.
 * dernière option : param = 2 -> l'erreur est affichée en bas 
 * retourne l'ancienne valeur */
int change_SLang_Error_Hook (int param)
{
   static int current_value;
   int temp;
   switch (param) {
       case 1 : SLang_Error_Hook = &error_SLang_Hook;
		SLang_VMessage_Hook = &vmessage_SLang_Hook;
		break;
       case 2 : SLang_Error_Hook = &error_SLang_Hook2;
		SLang_VMessage_Hook = &vmessage_SLang_Hook2;
		break;
       default : SLang_Error_Hook = NULL;
	  	 SLang_VMessage_Hook = NULL;
	 	 break;
   }
   temp = current_value;
   current_value = param;
   return temp;
}

int Parse_type_fun_slang(flrn_char *str_int) {
    return fl_strtol(str_int, NULL, 10);
}

SLang_Name_Type *Parse_fun_slang (flrn_char *str, int *num) {
    flrn_char *comma;
    int rc;
    char *trad;
    SLang_Name_Type *result;
    comma=fl_strchr(str,fl_static(','));
    if (comma) {
       *comma=fl_static('\0');
       *num=Parse_type_fun_slang(comma+1);
    } else *num=0;
    rc=conversion_to_file(str,&trad,0,(size_t)(-1));
    result=SLang_get_function(trad);
    if (rc==0) free(trad);
    if (comma) *comma=fl_static(',');
    return result;
}

/**** Gestions des hooks ****/
/* retour : 0 : non.  -1 : ok */

/* classés en fonction du type */

/*  Newsgroup -> string */
extern int try_hook_newsgroup_string (char *name, Newsgroup_List *groupe,
	                              flrn_char **res) {
    SLang_Name_Type *fun;
    int value_hook, ret=0,rt,rc;
    char *ires=NULL;

    if ((fun=SLang_get_function(name))==NULL) return ret;
    SLang_start_arg_list ();
    Push_newsgroup_on_stack(Newsgroup_courant);
    SLang_end_arg_list ();
    flrn_SLang_article_courant.article=Article_courant;
    flrn_SLang_article_courant.groupe=Newsgroup_courant;
    flrn_SLang_newsgroup_courant=(Newsgroup_courant ?
	                             Newsgroup_courant : &dummy_group);
    value_hook = change_SLang_Error_Hook(2);
    rt=SLexecute_function(fun);
    if (retour_intrinsics) {
       free(retour_intrinsics);
       retour_intrinsics=NULL;
    }
    if (rt==-1) {
         SLang_restart (1);
         SLang_Error = 0;
    } else 
	 if (SLpop_string(&ires)>=0) ret=1;
    if (ret) {
	rc=conversion_from_file(ires,res,0,(size_t)(-1));
	if (rc==0) free(ires);
    }
    (void) change_SLang_Error_Hook(value_hook);
    return ret;
}

#endif
