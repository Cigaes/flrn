/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_func.c : fonctions "de base" de flrn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/* Ces fonctions doivent être prévues pour, par la suite, servir de base
   au langage de script... Elles doivent avoir des noms compréhensible et
   respecter un format standard. En particulier, on rendra toujours un
   entier. On veillera à ne jamais utiliser Article_courant ni
   etat_loop. Par contre, on utilise Newsgroup_courant, toujours.
   Le retour est toujours : >=0 : bon
   			     <0 : erreur
   (pour les tests : 1 : oui, 0 : non)
*/

#include <stdio.h>
#include <stdlib.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "group.h"
#include "art_group.h"
#include "flrn_menus.h"
#include "flrn_pager.h"
#include "flrn_tags.h"
#include "flrn_regexp.h"
#include "flrn_func.h"

static UNUSED char rcsid[]="$Id$";

/* Gestion des tags */
/* ================ */

/* int is_tag_valid (Flrn_Tag *tag, Newsgroup_List **group)
   vérifie si un tag est valide (groupe existe, 
   				avec ou sans pointeur sur article)
   Retour : -2 : le tag n'existe pas
   	    -1 : le groupe n'existe pas
   	    0 : non valide (groupe connu, l'article n'existe pas)
   	    1 : valide pour le numéro, article non connu
	    2 : numéro et article valide
*/
int is_tag_valid (Flrn_Tag *tag, Newsgroup_List **group) {
   if (tag->article_deb_key==0) return -2;
   if (Newsgroup_courant && 
       (Newsgroup_courant->article_deb_key == tag->article_deb_key)) {
       *group=Newsgroup_courant;
       return 2;
   }
   *group = cherche_newsgroup(tag->newsgroup_name,1,0);
   if ((*group)==NULL) return -1;
   if (((*group)==Newsgroup_courant) && (tag->article_deb_key==-1)) {
       if (Recherche_article(tag->numero, &(tag->article), 0)==0) {
          tag->article_deb_key=Newsgroup_courant->article_deb_key;
          return 2;
       } else return 0;
   }
   if ((*group)->article_deb_key == tag->article_deb_key)
      return 2;
   else return 1;
}
       

/* int goto_tag (Flrn_Tag *tag, union element *article, Newsgroup_List **group)
   cette fonction prend un tag, et renvoie l'article
   considéré, éventuellement avec un nouveau groupe.
   Retour : 0 : ok, sans changer de groupe (article.article)
   	    1 : changement de groupe avec article (article et group modifié)
	    2 : changement de groupe avec numéro  (article et group modifié)
	    -1 : l'article n'existe pas
	    -2 : le groupe n'existe pas
	    -3 : tag innexistant
	    -4 : numéro de tag incorrect
*/
int goto_tag (Flrn_Tag *tag,
	union element *article, Newsgroup_List **group) {
   int ret;

   if (tag==NULL) return -4;
   ret = is_tag_valid(tag,group);
   if (ret==-2) return -3;
   if (ret==-1) return -2;
   if (ret==0) return -1;
   if (ret==1) {
      (*article).number=tag->numero;
      return 2;
   }
   (*article).article=tag->article;
   return ((*group==Newsgroup_courant) ? 0 : 1);
}

/* int put_tag (Article_List *article, Flrn_Tag *tag)
   Cette fonction met l'article article dans le tag tag...
   Retour : 0 : ok
   	   -1 : numéro de tag invalide
   ATTENTION : le nom du groupe n'est pas dupliqué
*/
int put_tag (Article_List *article, Flrn_Tag *tag) {
   if (tag==NULL) return -1;
   tag->article_deb_key = Newsgroup_courant->article_deb_key;
   tag->article = article;
   tag->numero = article -> numero;
   tag->newsgroup_name=Newsgroup_courant->name;
   return 0;
}


/* Gestion de groupes */
/* ================== */

/* int get_group (Newsgroup_List **group, int flags, char *name)
   cette fonction demande un groupe contenant la chaine (ou regexp) name
   flags & 1 : abonné et non abonné
   flags & 2 : Options.prefixe_groupe
   flags & 4 : regexp
   flags & 8 : avec menu
   Retour : 0 : Ok
   	   -1 : pas de choix au finish
	   -2 : pas de groupe possible
	   -3 : chaine vide
	   -4 : regexp invalide
*/
static struct format_elem_menu fmt_getgrp_menu_e [] =
    { { 0, 0, -1, 2, 0, 1 } };
struct format_menu fmt_getgrp_menu = { 1, &(fmt_getgrp_menu_e[0]) };
int get_group (Newsgroup_List **group, int flags, flrn_char *name) {
   flrn_char *gpe=name;
   Newsgroup_List *my_group=Newsgroup_courant, *tmpmygroup=NULL;
   regex_t reg;
   int correct, order=-1, best_order=-1;
   Liste_Menu *lemenu=NULL, *courant=NULL;
   flrn_char *tmp_name=NULL;

   while (*gpe==fl_static(' ')) gpe++;
   if (*gpe==fl_static('\0')) return -3;
   if (flags & 4) if (fl_regcomp(&reg,gpe,REG_EXTENDED)) return -4;
   if ((flags & 9)==9) {
      if (flags & 4) {
         flrn_char *mustmatch;
	 mustmatch=reg_string(gpe,1);
	 lemenu=menu_newsgroup_re(mustmatch, reg,1+(flags & 2));
	 if (mustmatch) free(mustmatch);
      } else lemenu=menu_newsgroup_re(gpe,reg,(flags & 2));
   } else  {
      if (my_group)
      do {
         my_group=my_group->next;
	 if (my_group==NULL) break;
	 if (flags & 2) tmp_name=truncate_group(my_group->name,0); else
	   tmp_name=my_group->name;
	 correct=(((flags & 4) &&
	 	     ((order=calcul_order_re(tmp_name,&reg))!=-1))
	    || (!(flags & 4) &&
	             ((order=calcul_order(tmp_name,gpe))!=-1))) &&
		 ((flags & 1) || !(my_group->flags & GROUP_UNSUBSCRIBED));
         if (correct) {
	    if (flags & 8)
	       courant=ajoute_menu_ordre(&lemenu,&fmt_getgrp_menu,
		       &tmp_name,my_group,-1,order);
	    else
	       if ((best_order==-1) || (order<best_order)) {
	          best_order=order;
		  tmpmygroup=my_group;
	       }
	 }
      } while (1);

      /* my_group=NULL */
      my_group=Newsgroup_deb;
      /* deuxième passe */
      while (my_group!=Newsgroup_courant) {
         if (flags & 2) tmp_name=truncate_group(my_group->name,0); else
	    tmp_name=my_group->name;
	 correct=(((flags & 4) &&
	           ((order=calcul_order_re(tmp_name,&reg))!=-1))
	       || (!(flags & 4) &&
		   ((order=calcul_order(tmp_name,gpe))!=-1))) &&
		((flags & 1) || !(my_group->flags & GROUP_UNSUBSCRIBED));
         if (correct) {
	    if (flags & 8)
	       courant=ajoute_menu_ordre(&lemenu,&fmt_getgrp_menu,
		       &tmp_name,my_group,-1,order);
	    else
	       if ((best_order==-1) || (order<best_order)) {
	          best_order=order;
		  tmpmygroup=my_group;
	       }
	  }
	  my_group=my_group->next;
      }
      /* my_group=Newsgroup_courant */
      my_group=NULL;
      if (!(flags & 8)) {
         if (tmpmygroup) my_group=tmpmygroup;
	 else
	 if (flags & 1) {
	    /* on recupere la chaine minimale de la regexp */
	    if (flags & 4) {
	       char *mustmatch;
	       mustmatch=reg_string(gpe,1);
	       if (mustmatch==NULL) mustmatch=safe_strdup("*");
	       if ((my_group=cherche_newsgroup_re
	                 (mustmatch,reg,(flags & 2)?1:0))==NULL) {
	          free(mustmatch);
		  regfree(&reg);
		  return -2;
	       } else {
	          free(mustmatch);
	          *group=my_group;
		  regfree(&reg);
		  return 0;
	       }
	    } else {
	      if ((my_group=cherche_newsgroup(gpe,0,(flags & 2)?1:0))==NULL) 
	         return -2;
	      else {
	         *group=my_group;
		 return 0;
	      }
	    }
	 } else {
	    if (flags & 4) regfree(&reg);
	    return -2;
	 }
     }
  }
  if (lemenu) {
     if (lemenu->suiv==NULL) my_group=lemenu->lobjet;
     else {
        num_help_line=10;
	my_group=Menu_simple(lemenu,NULL,Ligne_carac_du_groupe,NULL,
		/* FIXME : français */
	                      fl_static("Quel groupe ?"));
     }
     Libere_menu(lemenu); /* on est bien d'accord qu'on ne libere PAS les noms*/
                          /* CAR action_select vaut NULL */
  } else if (flags & 8) {
    if (flags & 4) regfree(&reg);
    return -2;
  }
  *group=my_group;
  if (debug && *group)
	fprintf(stderr, "get-group : %s\n", (*group)->name);
  if (flags & 4) regfree(&reg);
  return (my_group ? 0 : -1);
}
