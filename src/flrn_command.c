/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_command.c:: entrée des commandes
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

/*
 * Gestion des bindings de touches
 * Commun a tous les contextes */
/* On y ajoute aussi, commum a tous les contextes, l'entrées de commandes 
 * Ce qui fait de ce fichier un code extrêmement important qui reprend 
 * une partie du travail de flrn_inter.c . On y ajoute enfin la complétion
 * de commandes, de facon encore imparfaite. */
/* et  puis on ajoute aussi l'affichage d'un binding */

#include <stdlib.h>
#include <ctype.h>

#include "flrn.h"
#include "options.h"
#include "flrn_macros.h"
#include "flrn_command.h"
#include "flrn_comp.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_pager.h"
#include "flrn_menus.h"
#include "enc/enc_strings.h"
#include "flrn_format.h"
#ifdef USE_SLANG_LANGUAGE
#include "slang_flrn.h"
#endif

static UNUSED char rcsid[]="$Id$";

/* I)  Bindings de commandes */
#define DENIED_CHARS "0123456789<>.,_*-"

/* le tableau touche -> commande */
flrn_assoc_key_cmd *Flcmd_rev[MAX_ASSOC_TAB];
Flcmd_macro_t *Flcmd_macro=NULL;
int Flcmd_num_macros=0;
static int Flcmd_num_macros_max=0;

#define NUM_HIST_CMD 20
flrn_char *cmd_historique[NUM_HIST_CMD];
int cmd_hist_max=-1, cmd_hist_curr=-1;

/* Accès au tableau de commande */
void init_assoc_key_cmd() {
    memset (&Flcmd_rev[0],0,
	    MAX_ASSOC_TAB*sizeof(flrn_assoc_key_cmd *));
}
/* get_hash_key n'est utilisé que pour un key défini */
static unsigned char get_hash_key (struct key_entry *key) {
    if (key->entry==ENTRY_SLANG_KEY) 
	return ((unsigned char) key->value.slang_key & MAX_ASSOC_TAB);
    if (key->entry==ENTRY_STATIC_STRING) 
	return ((unsigned char) key->value.ststr[0] & MAX_ASSOC_TAB);
    return ((unsigned char) key->value.allocstr[0] & MAX_ASSOC_TAB);
}
int *get_adrcmd_from_key(int cxt, struct key_entry *key, int create) {
    flrn_assoc_key_cmd *pc;
    unsigned char hash;
    int i;
    if (key==NULL) return NULL;
    if (key->entry==ENTRY_ERROR_KEY) return NULL;
    hash=get_hash_key(key);
    pc=Flcmd_rev[hash];
    if (pc==NULL) {
        if (create) 
	    pc=Flcmd_rev[hash]=safe_calloc(1,sizeof(flrn_assoc_key_cmd));
	else return NULL;
    } else { 
	while (pc->next) {
            if (key_are_equal(&(pc->key),key)) break;
	    pc=pc->next;
	}
	if (key_are_equal(&(pc->key),key)) return &(pc->cmd[cxt]);
	if (create==0) return NULL;
	pc->next=safe_calloc(1,sizeof(flrn_assoc_key_cmd));
	pc=pc->next;
    }
    /* create = 1 */
    for (i=0;i<NUMBER_OF_CONTEXTS;i++) pc->cmd[i]=FLCMD_UNDEF;
    memcpy(&(pc->key),key,sizeof(struct key_entry));
    if (key->entry==ENTRY_ALLOCATED_STRING) 
	pc->key.value.allocstr=safe_flstrdup(key->value.allocstr);
    return &(pc->cmd[cxt]);
}
int add_cmd_for_slang_key (int cxt, int cmd, int slkey) {
    struct key_entry key;
    int *toadd;
    key.entry=ENTRY_SLANG_KEY;
    key.value.slang_key=slkey;
    toadd=get_adrcmd_from_key(cxt,&key,1);
    *toadd=cmd;
    return 0;
}
/* retourne la commande supprimée */
int del_cmd_for_key (int cxt,struct key_entry *key) {
    flrn_assoc_key_cmd *pc,**pere;
    unsigned char hash;
    int oldcmd,i;
    if (key==NULL) return -2;
    if (key->entry==ENTRY_ERROR_KEY) return -2;
    hash=get_hash_key(key);
    pc=Flcmd_rev[hash];
    if (pc==NULL) return FLCMD_UNDEF;
    if (key_are_equal(&(pc->key),key)) 
	pere=&(Flcmd_rev[hash]);
    else { 
	while (pc->next) {
	    if (key_are_equal(&(pc->next->key),key)) break;
	    pc=pc->next;
	}
	if (pc->next==NULL) return FLCMD_UNDEF;
	pere=&(pc->next);
	pc=pc->next;
    }
    oldcmd=pc->cmd[cxt];
    pc->cmd[cxt]=FLCMD_UNDEF;
    for (i=0;i<NUMBER_OF_CONTEXTS;i++) 
	if (pc->cmd[i]!=FLCMD_UNDEF) break;
    if (i==NUMBER_OF_CONTEXTS) {
        if (key->entry==ENTRY_ALLOCATED_STRING) 
	   free(pc->key.value.allocstr);
	(*pere)=pc->next;
        free(pc);
    }
    return oldcmd;
}

/* cree une macro
 * maintenant, on recherche une place libre avant d'ajouter a la fin...
 * donc plusieurs bind sur la meme touche ne doivent plus faire augmenter
 * la consommation memoire */
static int do_macro(int cmd, 
#ifdef USE_SLANG_LANGUAGE
    flrn_char *fun_slang,
#endif
    flrn_char *arg) {
  int num;
  for (num=0; num < Flcmd_num_macros; num++) {
    if (Flcmd_macro[num].cmd < 0)
      break;
  }
  if (num == Flcmd_num_macros_max) {
    Flcmd_macro=safe_realloc(Flcmd_macro,
	(Flcmd_num_macros_max+4)*sizeof(Flcmd_macro_t));
    Flcmd_num_macros_max += 4;
  }
  if (num == Flcmd_num_macros)
    Flcmd_num_macros++;
  Flcmd_macro[num].cmd = cmd;
#ifdef USE_SLANG_LANGUAGE
  if (fun_slang != NULL)
    Flcmd_macro[num].fun_slang = safe_flstrdup(fun_slang);
  else
    Flcmd_macro[num].fun_slang = NULL;
#endif
  if (arg != NULL) 
    Flcmd_macro[num].arg = safe_flstrdup(arg);
  else
    Flcmd_macro[num].arg = NULL;
  Flcmd_macro[num].next_cmd = -1;
  return num;
}

static void del_macro(int num) {
#ifdef USE_SLANG_LANGUAGE
  if (Flcmd_macro[num].fun_slang)
    free(Flcmd_macro[num].fun_slang);
#endif
  if (Flcmd_macro[num].arg)
    free(Flcmd_macro[num].arg);
  Flcmd_macro[num].cmd = -1;
  return;
}

/* renvoie -1 en cas de pb
 * si add!=0, on ajoute la commande... */
int Bind_command_new(struct key_entry *key, int command, flrn_char *arg,
#ifdef USE_SLANG_LANGUAGE
   flrn_char *fun_slang,
#endif
   int context, int add ) {
  int parcours,*toadd=NULL;
  int mac_num,mac_prec=-1;
  toadd=get_adrcmd_from_key(context,key,(add==0));
  if (add) {
    if ((toadd==NULL) || (*toadd==FLCMD_UNDEF)) return -1;
    parcours=*toadd;
    while ((parcours>-1) && (parcours & FLCMD_MACRO)) {
      mac_prec=parcours ^ FLCMD_MACRO;
      parcours=Flcmd_macro[parcours ^ FLCMD_MACRO].next_cmd;
    }
    if (parcours>-1) {
      mac_num = do_macro(parcours,
#ifdef USE_SLANG_LANGUAGE
      NULL,
#endif
      NULL);
      if (mac_prec==-1) *toadd=mac_num | FLCMD_MACRO;
          else Flcmd_macro[mac_prec].next_cmd=mac_num | FLCMD_MACRO;
      toadd = & Flcmd_macro[mac_num].next_cmd;
    } else {
      toadd = & Flcmd_macro[mac_prec].next_cmd;
    }
  } else {
    parcours = *toadd;
    while ((parcours>-1) && (parcours & FLCMD_MACRO)) {
       /* pour liberer la macro */
       mac_prec=parcours ^ FLCMD_MACRO;
       parcours=Flcmd_macro[parcours ^ FLCMD_MACRO].next_cmd;
       del_macro(mac_prec);
    }
  }
  if ((arg != NULL) 
#ifdef USE_SLANG_LANGUAGE
    || (fun_slang != NULL)
#endif
  ) {
    mac_num = do_macro(command,
#ifdef USE_SLANG_LANGUAGE
        fun_slang,
#endif
        arg);
    *toadd = mac_num | FLCMD_MACRO;
    return 0;
  } else {
    *toadd = command;
    return 0;
  }
}

void free_Macros(void) {
   int i;
   for (i=0;i<Flcmd_num_macros;i++) {
     if (Flcmd_macro[i].arg);
       free(Flcmd_macro[i].arg);
#ifdef USE_SLANG_LANGUAGE
     if (Flcmd_macro[i].fun_slang)
       free(Flcmd_macro[i].fun_slang);
#endif
   }
   Flcmd_num_macros=0;
   Flcmd_num_macros_max=0;
   if (Flcmd_macro) {
      free(Flcmd_macro);
      Flcmd_macro=NULL;
   }
}

static flrn_char *get_context_name (void *ptr, int num)
{
   return ((flrn_char **)ptr)[num];
}

int keybindings_comp(flrn_char *str, size_t len, Liste_Chaine *debut) {
   Liste_Chaine *courant;
   int i, res;
   int result[NUMBER_OF_CONTEXTS];

   for (i=0;i<NUMBER_OF_CONTEXTS;i++) result[i]=0;
   res = Comp_generic(debut, str,len,(void *)Noms_contextes,NUMBER_OF_CONTEXTS,
         get_context_name,fl_static(" "),result,1);
   if (res==-3) return 0;
   if (res>= 0) {
       if (str[0]) debut->complet=0;
       fl_strcat(debut->une_chaine,str);
       return 0;
   }
   if (res==-1) {
      courant=debut->suivant;
      for (i=0;i<NUMBER_OF_CONTEXTS;i++) {
        if (result[i]==0) continue;
	fl_strcat(courant->une_chaine,str);
	if (str[0]) courant->complet=0;
        courant=courant->suivant;
      }
      return -1;
   }  
   return -2;
}

/* deux formats possibles */
static struct format_elem_menu fmt_keybind_menu1_e [] =
       { { 0, 0, 11, 2, 0, 3 }, { 0, 0, -1, 12, 0, 3 } };
struct format_menu fmt_keybind_menu1 = { 2, &(fmt_keybind_menu1_e[0]) };

static struct format_elem_menu fmt_keybind_menu2_e [] =
       { { 0, 0, 11, 2, 0, 3 }, { 0, 0, 0, 12, 0, 3 }, { 0, 0, 0, 12, 0, 3 },
	 { 0, 0, -1, 12, 0, 3 } };
struct format_menu fmt_keybind_menu2 = { 4, &(fmt_keybind_menu2_e[0]) };

/* 0 : un truc, -1 : rien -2 : trop court, -3 : spécial */
int aff_ligne_binding(struct key_entry *key, int contexte, 
	flrn_char ***ligne, struct format_menu **fmt_menu) {
   int i=0, taille, j;
   int ch;
   int *cmd=NULL;
   const flrn_char *special=NULL;

   if (Screen_Cols<20) return -2;
   if (*fmt_menu==NULL) {
       if (contexte==-1) {
	   *fmt_menu=&fmt_keybind_menu2;
	   taille=(Screen_Cols-12)/NUMBER_OF_CONTEXTS;
	   fmt_keybind_menu2_e[1].maxwidth=taille-1;
	   fmt_keybind_menu2_e[2].maxwidth=taille-1;
	   fmt_keybind_menu2_e[3].maxwidth=taille-1;
	   fmt_keybind_menu2_e[2].coldeb=taille+12;
	   fmt_keybind_menu2_e[3].coldeb=2*taille+12;
       } else {
	   *fmt_menu=&fmt_keybind_menu1;
       }
   } else 
   if (contexte==-1) *fmt_menu=&fmt_keybind_menu2;
   else *fmt_menu=&fmt_keybind_menu1;
   if (key->entry==ENTRY_SLANG_KEY) {
       /* cas des spécials */
       ch=key->value.slang_key;
       switch (ch) {
	   case '\\' : special=
			   fl_static("touche de commande explicite");
			  /* FIXME : francais */
		       break;
	   case '@' : special=
		         fl_static("touche de commande nocbreak");
			  /* FIXME : francais */
		      break;
           default : if ((ch<256) && (strchr(DENIED_CHARS,ch)) && (ch!='-'))
			 special=fl_static("touche de prefixe de commande");
			  /* FIXME : francais */
       }
   }
   if (special==NULL) {
       i=(contexte==-1 ? 0 : contexte);
       cmd=get_adrcmd_from_key(i, key,  0);
       if (cmd==NULL) return -1;
       if ((contexte!=-1) && ((*cmd)==FLCMD_UNDEF)) return -1;
   } else *fmt_menu=&fmt_keybind_menu1;
   *ligne=safe_calloc((*fmt_menu)->nbelem,sizeof(flrn_char *));
   (*ligne)[0]=safe_flstrdup(get_name_key(key,0));
   if (special) {
       (*ligne)[1]=safe_flstrdup(special);
       return -3;
   }
   j=1;
   do {
      if (*cmd==-1) (*ligne)[j]=safe_flstrdup(fl_static("undef")); else {
        if ((*cmd) & FLCMD_MACRO) {
	   if (contexte!=-1) {
	       size_t len;
	       int cmd2;
               if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].next_cmd!=-1)
		   len=17;
	       else len=3;
	       cmd2=Flcmd_macro[(*cmd) ^ FLCMD_MACRO].cmd;
#ifdef USE_SLANG_LANGUAGE
              if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].fun_slang==NULL) {
#endif
	      switch (i) {
	        case CONTEXT_COMMAND : special=Flcmds[cmd2].nom;
				break;
                case CONTEXT_PAGER : special=Flcmds_pager[cmd2];
				break;
                default : special=Flcmds_menu[cmd2];
				break;
	      }
	      len+=strlen(special);
#ifdef USE_SLANG_LANGUAGE
              } else 
	        len+=fl_strlen(Flcmd_macro[(*cmd) ^ FLCMD_MACRO].fun_slang)+3;
#endif	      
	      if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].arg) 
		  len+=strlen(Flcmd_macro[(*cmd) ^ FLCMD_MACRO].arg)+1;
	      (*ligne)[j]=safe_malloc(len*sizeof(flrn_char));
	      if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].next_cmd!=-1)
		  fl_strcpy((*ligne)[j],fl_static("(mult.)"));
	      else (*ligne)[j][0]=fl_static('\0');
#ifdef USE_SLANG_LANGUAGE
              if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].fun_slang==NULL) {
#endif
		  fl_strcat((*ligne)[j],fl_static_tran(special));
#ifdef USE_SLANG_LANGUAGE
              } else {
		  fl_strcat((*ligne)[j],fl_static("["));
		  fl_strcat((*ligne)[j],
			  Flcmd_macro[(*cmd) ^ FLCMD_MACRO].fun_slang);
		  fl_strcat((*ligne)[j],fl_static("]"));
	        
              }
#endif	      
	      fl_strcat((*ligne)[j],fl_static(" "));
	      if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].arg)
		  fl_strcat((*ligne)[j],Flcmd_macro[(*cmd) ^ FLCMD_MACRO].arg);
	      if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].next_cmd!=-1) 
		  fl_strcat((*ligne)[j],fl_static(" ..."));
	   } else
	     if (Flcmd_macro[(*cmd) ^ FLCMD_MACRO].next_cmd!=-1) 
	        (*ligne)[j]=safe_flstrdup(fl_static("(mult.)"));
	     else (*ligne)[j]=safe_flstrdup(fl_static("(macro)"));
	} else {
          switch (i) {
	    case CONTEXT_COMMAND : 
		(*ligne)[j]=safe_flstrdup(fl_static_tran(Flcmds[*cmd].nom));
		break;
	    case CONTEXT_PAGER : 
		(*ligne)[j]=safe_flstrdup(fl_static_tran(Flcmds_pager[*cmd]));
		break;
	    case CONTEXT_MENU :
		(*ligne)[j]=safe_flstrdup(fl_static_tran(Flcmds_menu[*cmd]));
		break;
	  }
        }
      }
      i++;
      j++;
      if ((contexte!=-1) || (i==NUMBER_OF_CONTEXTS)) return 0;
      cmd++; /* pas super propre, mais on va faire avec */
   } while (1);
}

/* II) : complétion d'une commande quelconque */


/* TODO : généraliser ça mieux */
static flrn_char * get_command_name_tout(void * rien, int num) {
    static flrn_char flbla[25];
    char *bla;
    int n;
    if ((n=num-NB_FLCMD)<0) bla=Flcmds[num].nom; else
    if ((num=n-NB_FLCMD_PAGER)<0) bla=Flcmds_pager[n]; else
	bla=Flcmds_menu[num];
    strcpy(flbla,fl_static_tran(bla));
    return flbla;
}

/* Comp_cmd_explicite ne s'occupe pas de connaitre le contexte. C'est */
/* ennuyeux. Il faudrait améliorer ça un jour...		      */
int Comp_cmd_explicite(flrn_char *str, size_t len, Liste_Chaine *debut)
{
  int res,res2=-2,i,nbflcmd, bon;
  char *suite;
  Liste_Chaine *courant, *pere, *suivant;
  int result[NB_FLCMD+NB_FLCMD_PAGER+NB_FLCMD_MENU];

  if (*str==fl_static('\\')) {
    str++; fl_strcat(debut->une_chaine,fl_static("\\"));
  }
  /* TODO : améliorer ça */
  nbflcmd=NB_FLCMD+NB_FLCMD_PAGER+NB_FLCMD_MENU;
  for (i=0;i<nbflcmd;i++) result[i]=0;
  res = Comp_generic(debut,str,len,NULL,nbflcmd, get_command_name_tout,
	      fl_static(" "),result,1);
  if (res==-3) return 0;
  if (res>=0) {
    if ((res<NB_FLCMD) && (Flcmds[res].comp)) {
       return (*Flcmds[res].comp)(str,len,debut);
    } else {
      fl_strcat(debut->une_chaine,str);
      if (str[0]) debut->complet=0;
      return 0;
    }
  }
  if (res==-1) {
    bon=0;
    pere=debut;
    courant=debut->suivant;
    suivant=courant->suivant;
    /* TODO : améliorer ça */
    for (i=0;i<NB_FLCMD;i++) {
       if (result[i]==0) continue;
       res2=0;
       if (Flcmds[i].comp) {
         suite=safe_flstrdup(str);
         res2=(*Flcmds[i].comp)(suite,len,courant);
         free(suite);
         if (res2<-1) {
            free(courant->une_chaine);
            pere->suivant=courant->suivant;
            free(courant);
            courant=pere->suivant;
	    if (courant) suivant=courant->suivant;
            continue;
         }
       } else {
         fl_strcat(courant->une_chaine,str);
         if (str[0]) courant->complet=0;
       }
       if (res2==-1) bon+=2; else bon++;
       pere=courant;
       while (pere->suivant!=suivant) pere=pere->suivant;
       courant=suivant;
       if (courant) suivant=courant->suivant;
    }
    for (i=NB_FLCMD;i<NB_FLCMD+NB_FLCMD_PAGER+NB_FLCMD_MENU;i++) {
       if (result[i]==0) continue;
       res2=0;
       fl_strcat(courant->une_chaine,str);
       if (str[0]) courant->complet=0;
       bon++;
       pere=courant;
       while (pere->suivant!=suivant) pere=pere->suivant;
       courant=suivant;
       if (courant) suivant=courant->suivant;
    }
    if (bon>1) return -1; else if (bon) {
       free(debut->une_chaine);
       courant=debut->suivant;
       debut->suivant=courant->suivant;
       debut->une_chaine=courant->une_chaine;
       free(courant);
       return 0;
    }
  }
  return -2;
}


/* III) Entrées de commandes */

/* Prend une commande... Renvoie, selon les contextes, le code de la     */
/* commande frappée, ainsi que les paramètres à lire...			 */
/* Prend en paramètre : key_depart : la touche de départ si elle existe  */
/*			princip, second : les contextes possibles	 */
/* 			la_commande : pointeur sur le retour...		 */
/*			chaine : la chaîne à afficher			 */
/* Renvoie  0 contexte principal					 */
/*          1 contexte secondaire					 */
/*	   -1 si commande non défini                                     */
/*         -2 si rien                                                    */
/*         -3 si l'état est déjà défini...                               */

flrn_char *Noms_contextes[NUMBER_OF_CONTEXTS] = {
   fl_static("command"), fl_static("pager"), fl_static("menu") };
#define MAX_CHAR_STRING 100
/* on ne met pas le '-', hélas, pour des raisons classiques */

int get_command_nocbreak(struct key_entry *, int, int, int, Cmd_return *);
int get_command_explicite(flrn_char *, int, int, int, Cmd_return *);
int get_command_newmode(struct key_entry *, int, int, int, Cmd_return *);
int Lit_cmd_key(struct key_entry *, int, int, Cmd_return *);
int Lit_cmd_explicite(flrn_char *, int, int, Cmd_return *);

static int aff_context(int princip, int second) {
   flrn_char chaine[18];

   chaine[0]=fl_static('(');
   fl_strcpy(&(chaine[1]),Noms_contextes[princip]);
   if (second>=0) {
      fl_strcat(chaine,fl_static("/"));
      fl_strcat(chaine,Noms_contextes[second]);
   }
   fl_strcat(chaine,fl_static(") : "));
   return Aff_fin(chaine);
}

static void add_str_to_description (const flrn_char *str,
	Cmd_return *la_commande) {
   if (str==NULL) return;
   if (la_commande->len_desc>0) {
      if (fl_strlen(str)>=la_commande->len_desc) {
	  free(la_commande->description);
	  la_commande->description=NULL;
	  la_commande->len_desc=0;
      } else {
          fl_strncat(la_commande->description, str, la_commande->len_desc);
          la_commande->len_desc-=strlen(str);
      }
   }
}

int save_command (Cmd_return *la_commande) {
    int i=-1,len;
    flrn_char *created,*tmp;
    
    if (la_commande->len_desc==0) return -1;
    len = (la_commande->after ? fl_strlen(la_commande->after) : 0) +
	  (la_commande->before ? fl_strlen(la_commande->before) : 0) +
	  fl_strlen(la_commande->description) + 3;
    created=safe_malloc(len*sizeof(flrn_char));
    if (la_commande->before) fl_strcpy(created,la_commande->before);
       else created[0]=fl_static('\0');
    fl_strcat(created, la_commande->description);
    if (la_commande->after) {
	 if (la_commande->after[0]!=fl_static(' ')) 
	    fl_strcat(created,fl_static(" "));
	 fl_strcat(created,la_commande->after);
    }
    for (i=0; i<=cmd_hist_max; i++) 
	if (fl_strcmp(created, cmd_historique[i])==0) break;
    if (i<=cmd_hist_max) {
	/* on l'a trouvé */
	free(created);
	tmp = cmd_historique[i];
	if (i>cmd_hist_curr) {
	    while (i<cmd_hist_max) {
		cmd_historique[i] = cmd_historique[i+1];
		i++;
            }
	    cmd_historique[cmd_hist_max]=cmd_historique[0];
	    i=0;
	}
	if (i<cmd_hist_curr) {
            while (i<cmd_hist_curr) {
		cmd_historique[i] = cmd_historique[i+1];
		i++;
            }
	} 
	cmd_historique[cmd_hist_curr]=tmp;
	la_commande->len_desc=0;
	free (la_commande->description);
	return 1;
    }
    if (cmd_hist_max<NUM_HIST_CMD-1) {
         cmd_historique[++cmd_hist_max]=created;
         cmd_hist_curr=cmd_hist_max;
    } else {
         cmd_hist_curr++;
         if (cmd_hist_curr==NUM_HIST_CMD) cmd_hist_curr=0;
         free(cmd_historique[cmd_hist_curr]);
         cmd_historique[cmd_hist_curr]=created;
   }
   la_commande->len_desc=0;
   free (la_commande->description);
   return 0;
}
   
int get_command(struct key_entry *key_depart, int princip, int second, 
		Cmd_return *la_commande) {
   int col,i, res, slk=-1;
   struct key_entry key;

   key.entry=ENTRY_ERROR_KEY;
   la_commande->description=safe_malloc(MAX_CHAR_STRING*sizeof(flrn_char));
   la_commande->len_desc=MAX_CHAR_STRING;
   la_commande->before=la_commande->after=NULL;
   la_commande->flags=0;
   la_commande->description[0]=fl_static('\0');
   for (i=0; i<NUMBER_OF_CONTEXTS; i++) 
      la_commande->cmd[i]=FLCMD_UNDEF;
#ifdef USE_SLANG_LANGUAGE
   la_commande->fun_slang=NULL;
#endif
   if (!Options.cbreak) {
      col=aff_context(princip, second);
      res=get_command_nocbreak(key_depart,col,princip,second,
                                        la_commande);
   } else {
     if (key_depart) memcpy(&key,key_depart,sizeof(struct key_entry));
     else {
	 Attend_touche(&key);
     }
     if (key.entry==ENTRY_SLANG_KEY) slk=key.value.slang_key;
     if (slk==fl_key_nocbreak) {
        col=aff_context(princip, second);
        res= get_command_nocbreak(&key,col,princip,second, la_commande);
     } else
     if (slk==fl_key_explicite) {
        col=aff_context(princip, second);
        res=get_command_explicite(NULL,col,princip,second,la_commande);
     } else {
        /* Beurk pour '-' */
        if ((slk==-1) || (slk==(int)'-') || 
		(strchr(DENIED_CHARS,slk)==NULL)) {
                   res=Lit_cmd_key(&key,princip,second,la_commande);
		   if ((res>=0) && (slk!='\r')) la_commande->flags|=1;
        } else {
           col=aff_context(princip, second);
           res=get_command_newmode(&key,col,princip,second,la_commande);
        }
     }
   }
   Free_key_entry(&key);
   return res;
}

/* Renvoie le nom d'une commande par touche raccourcie */
/* on ne peut pas obtenir une fonction slang, par exemple, autrement */
/* que par une commande de macro, alors non indiquée */
int Lit_cmd_key(struct key_entry *key, int princip, int second,
	Cmd_return *la_commande) {
   int res=-1, j, context;
   int *cmd;
   if (key==NULL) return -1;
   for (j=0; j<2; j++) {
      context=(j==0 ? princip : second);
      if (context==-1) break;
      /* CAS PARTICULIER : la touche entrée */
      if ((key->entry==ENTRY_SLANG_KEY) && (key->value.slang_key=='\r')
	      && (la_commande->before) && (context==CONTEXT_COMMAND))
      {
	  res=(res==-1 ? j : res);
	  la_commande->cmd[CONTEXT_COMMAND]=FLCMD_VIEW;
      } else {
	  cmd=get_adrcmd_from_key(context,key,0);
	  if (cmd==NULL) 
	      la_commande->cmd[context]=FLCMD_UNDEF;
	  else {
	      la_commande->cmd[context]=*cmd;
	      if (*cmd!=FLCMD_UNDEF) res=(res==-1 ? j : res);
	  }
      }
   }
   add_str_to_description(get_name_key(key,1),la_commande);
   return res;
}


/* Lit le nom explicite d'une commande */
int Lit_cmd_explicite(flrn_char *str, int princip, int second,
	Cmd_return *la_commande) {
   int i, j, res=-1, ini, final;
   flrn_char *flncmd;
     
   la_commande->flags|=2;
   /* Hack pour les touches fleches en no-cbreak */
   if (fl_strncmp(str, "key-", 4)==0) {
       flrn_char *buf;
       int key;
       struct key_entry ke;
       buf=fl_strchr(str,fl_static(' '));
       if (buf) *buf=fl_static('\0');
       key = parse_key_name(str+4);
       if (buf) *buf=fl_static(' ');
       ke.entry=ENTRY_SLANG_KEY;
       ke.value.slang_key=key;
       if (key) return Lit_cmd_key(&ke,princip,second,la_commande);
   }
   /* cas "normal" */
   
#ifdef USE_SLANG_LANGUAGE
   if (str[0]==fl_static('[')) {
      flrn_char *end_str, *comma;
      int ctxt=-1, flg=0;
      str++;
      if ((end_str=fl_strchr(str,fl_static(']')))==NULL) return -1;
      *end_str=fl_static('\0');
      add_str_to_description(str,la_commande);
      add_str_to_description(fl_static("]"),la_commande);
      comma=strchr(end_str,fl_static(','));
      if (comma) {
	 *comma=fl_static('\0');     
         for (i=0;i<NUMBER_OF_CONTEXTS;i++) {
           if (fl_strcmp(str, Noms_contextes[i])==0) {
 	      ctxt=i;
	      break;
           }
	 }
	 if (ctxt!=-1) {
	    str=comma+1;
	    *comma=fl_static(',');
	    if ((ctxt!=princip) && (ctxt!=second)) {
	       *end_str=fl_static(']');
	       return -1;
	    }
	 }
      }
      la_commande->fun_slang=safe_flstrdup(str);
      comma=fl_strchr(str,fl_static(','));
      if (comma) flg = Parse_type_fun_slang(comma+1);
      *end_str=fl_static(']');
      la_commande->cmd[CONTEXT_COMMAND]=NB_FLCMD+flg;
      la_commande->cmd[CONTEXT_PAGER]=NB_FLCMD_PAGER+flg;
      la_commande->cmd[CONTEXT_MENU]=NB_FLCMD_MENU+flg;
      return (((ctxt==princip) || (ctxt==-1)) ? 0 : 1);
   }
#endif      
   for (j=0;j<2;j++) {
      ini=0; final=NB_FLCMD;
      switch (j==0 ? princip : second) {
	  case CONTEXT_MENU  : ini=NB_FLCMD_PAGER;
			       final+=NB_FLCMD_MENU;  /* fall */
	  case CONTEXT_PAGER : ini+=NB_FLCMD;
			       final+=NB_FLCMD_PAGER;
      }
      for (i=ini;i<final;i++) {
	  flncmd=get_command_name_tout(NULL,i);
	  if ((fl_strncmp(str,flncmd,fl_strlen(flncmd))==0) 
		  && ((str[fl_strlen(flncmd)]==fl_static('\0')) ||
		      (fl_isblank(str[fl_strlen(flncmd)])))) {
	      add_str_to_description(flncmd, la_commande);
	      la_commande->cmd[j==0 ? princip : second]=i-ini;
	      res=(res==-1 ? j : res);
	      break;
	  }
      }
   }
   return res;
}

/* Prend une commande explicite */
/* retourne -2 si rien */
int get_command_explicite(flrn_char *start, int col, int princip, int second, Cmd_return *la_commande) {
   int res=0, ret=0, ecart=0;
   struct key_entry key;
   flrn_char flcmd_line[MAX_CHAR_STRING], *flstr=flcmd_line;
   char cmd_line[MAX_CHAR_STRING], *str=cmd_line;
   size_t prefix_len=0, prefix_afflen=0;

   key.entry=ENTRY_ERROR_KEY;
   la_commande->flags |= 2;
   flcmd_line[0]=fl_static('\0');
   if (start) {
     prefix_len = fl_strlen(start);
     fl_strncpy (flcmd_line, start, MAX_CHAR_STRING-2);
   }
   fl_strcat(flcmd_line,fl_static("\\"));
   prefix_len++;
   conversion_to_terminal(flstr,&str,MAX_CHAR_STRING-1,prefix_len);
   prefix_afflen=strlen(str);
   ecart=str_estime_width(str,col,prefix_afflen);
   str+=prefix_afflen;
   do {
     Aff_ligne_comp_cmd(flstr,(size_t)(-1),col);
     conversion_to_terminal(flstr+prefix_len,&str,
	     MAX_CHAR_STRING-prefix_afflen,(size_t)(-1));
     if ((res=magic_flrn_getline(flstr+prefix_len,MAX_CHAR_STRING-prefix_len,
		     str,MAX_CHAR_STRING-prefix_afflen,
         Screen_Rows2-1,col+ecart,"\011",0,(ret==1 ? &key : NULL),NULL))<0) {
	 Free_key_entry(&key);
         return -2;
     }
     if (res>0) ret=Comp_general_command(flstr+prefix_len,
	     MAX_CHAR_STRING-prefix_len,col+ecart,Comp_cmd_explicite,
	     Aff_ligne_comp_cmd,&key);
     if (ret<0) ret=0;
   } while (res!=0);
   res=Lit_cmd_explicite(flstr+prefix_len, princip, second, la_commande);
   flstr=fl_strchr(flstr,fl_static(' '));
   if (flstr) {
       la_commande->after=safe_strdup(flstr+1);
   }
   Free_key_entry(&key);
   return res;
}

/* Prend une commande en nocbreak */
int get_command_nocbreak(struct key_entry *asked,int col, int princip, int second, Cmd_return *la_commande) {
   flrn_char flcmd_line[MAX_CHAR_STRING], *flstr=flcmd_line;
   char cmd_line[MAX_CHAR_STRING], *str=cmd_line;
   flrn_char *buf, *save_str=NULL, first;
   int res, hist_chosen=-1, correct=0, chang;
   int key_nocb=0;
   struct key_entry key;

   key.entry=ENTRY_ERROR_KEY;
   memset(flstr,0,8*sizeof(flrn_char));
   /* Dans le cas où asked='\r', et vient donc directement d'une interruption */
   /* on ne prend pas de ligne de commande : on l'a déjà...                   */
   if (asked) {
       if (asked->entry==ENTRY_SLANG_KEY) {
	   if (asked->value.slang_key==(int)'\r')
	       return Lit_cmd_key(asked,princip,second,la_commande);
	   if (asked->value.slang_key==fl_key_nocbreak) {
	       key_nocb=1;
	       Screen_write_char(fl_key_nocbreak);
               Screen_erase_eol();
	   } else *flstr=(char)asked->value.slang_key;
	      /* special characters should not be used in nocbreak mode */
       } else if (asked->entry==ENTRY_STATIC_STRING) 
	   fl_strncpy(flstr,asked->value.ststr,4);
       else if (asked->entry==ENTRY_ALLOCATED_STRING)
	   fl_strcpy(flstr,asked->value.allocstr);
   }
   if (*flstr) {
       conversion_to_terminal(flstr,&str,MAX_CHAR_STRING,(size_t)(-1));
       Screen_write_string(str);
       Screen_erase_eol();
   } else *str='\0';
   res=0;
   while (!correct) {
     chang=0;
     if ((res=magic_flrn_getline(flstr,MAX_CHAR_STRING,
		     str,MAX_CHAR_STRING, Screen_Rows2-1,
                      col+key_nocb,"",2,NULL,&key))<0) {
       if (save_str) free(save_str);
       return -2;
     }
     chang=0;
     if (res>1) { 
	 if (key.entry==ENTRY_SLANG_KEY) res=key.value.slang_key;
	   /* en théorie c'est obligatoire */
         Free_key_entry(&key);
     }
     if (res==FL_KEY_UP) {
	 if (cmd_hist_max>=0) {
	     if (hist_chosen==-1) {
		 hist_chosen=cmd_hist_curr;
		 save_str=safe_flstrdup(str);
		 fl_strcpy(flstr,cmd_historique[hist_chosen]);
             } else { 
		 hist_chosen--;
		 if (hist_chosen==-1) hist_chosen=cmd_hist_max;
		 fl_strcpy(flstr,cmd_historique[hist_chosen]);
	     }
	     chang=1;
	 }
     } else if (res==FL_KEY_DOWN) {
	 if ((cmd_hist_max>=0) && (hist_chosen!=-1)) {
	     if (hist_chosen==cmd_hist_curr) {
		 fl_strcpy(str,save_str);
		 hist_chosen=-1;
		 free(save_str);
		 save_str=NULL;
             } else {
		 hist_chosen++;
		 if (hist_chosen>cmd_hist_max) hist_chosen=0;
		 fl_strcpy(str,cmd_historique[hist_chosen]);
             }
	     chang=1;
	 }
     } else correct=(res==0);
     if (chang) {
	 Cursor_gotorc(Screen_Rows2-1,col+key_nocb);
         conversion_to_terminal(flstr,&str,MAX_CHAR_STRING,(size_t)(-1));
	 Screen_write_string(str);
	 Screen_erase_eol();
     }
   }
   while(*flstr==(flrn_char)fl_key_nocbreak) str++;
   if (flstr[0]==fl_static('\0')) {
       if (save_str) free(save_str);
       key.entry=ENTRY_SLANG_KEY;
       key.value.slang_key=(int)'\r';
       return Lit_cmd_key(&key,princip,second,la_commande);
   }
   /* Beurk pour le '-' */
   if ((flstr[0]==fl_static('-')) && (flstr[1]==fl_static('\0'))) {
       if (save_str) free(save_str);
       key.entry=ENTRY_SLANG_KEY;
       key.value.slang_key=(int)'-';
       return Lit_cmd_key(&key,princip,second,la_commande);
   }
   buf=flstr+fl_strspn(str,fl_static(DENIED_CHARS));
   first=(*buf ? *buf : fl_static('\r'));
   *buf=fl_static('\0');
   if (*flstr) {
       la_commande->before=safe_strdup(flstr);
   }
   *buf=first;
   if (first==(flrn_char)fl_key_explicite) {
       flrn_char bla[2];
       bla[0]=(flrn_char)fl_key_explicite;
       bla[1]=fl_static('\0');
       add_str_to_description(bla,la_commande);
       buf++;
       res=Lit_cmd_explicite(buf, princip, second, la_commande);
       buf=fl_strchr(buf,fl_static(' ')); if (buf) buf++;
   } else {
       buf+=parse_key_string(buf,&key);
       res=Lit_cmd_key(&key,princip,second,la_commande);
   }
   /* Ne pas oublier !!!!! */
   /* dans Lit_cmd_key le cas très particulier de \r qui devient 'v' pour un */
   /* contexte particulier...						     */
   if ((buf) && (*buf)) {
       la_commande->after=safe_strdup(buf);
   }
   if (save_str) free(save_str);
   Free_key_entry(&key);
   return res;
}

/* Prend une commande dans le nouveau mode */
int get_command_newmode(struct key_entry *key_ini,int col, int princip, int second, Cmd_return *la_commande) {
   int res;
   flrn_char flcmd_line[MAX_CHAR_STRING];
   char cmd_line[MAX_CHAR_STRING];
   flrn_char *flstr=flcmd_line;
   char *str=cmd_line;
   struct key_entry key;

   key.entry=ENTRY_ERROR_KEY;
   cmd_line[0]='\0';
   flcmd_line[0]='\0';
   /* On appelle magic_flrn_getline avec flag=1 */
   if ((res=magic_flrn_getline(flstr,MAX_CHAR_STRING,str,MAX_CHAR_STRING,
		   Screen_Rows2-1,col,DENIED_CHARS,1,key_ini,&key))<0)
      return -2;
   if (*flstr) { 
       la_commande->before=safe_strdup(flstr);
       la_commande->flags|=2;
   }
   if ((res==1) && (key.entry==ENTRY_SLANG_KEY) && 
	   (key.value.slang_key==fl_key_explicite)) {
       flrn_char bla[2];
       bla[0]=(flrn_char)fl_key_explicite;
       bla[1]=fl_static('\0');
       add_str_to_description(bla,la_commande);
       return get_command_explicite(flcmd_line,col,princip,second,
	       la_commande); 
   }
   else 
       res=Lit_cmd_key(&key,princip,second,la_commande);
   if ((res>=0) && ((key.entry!=ENTRY_SLANG_KEY) || 
	             (key.value.slang_key!=(int)'\r')))
       la_commande->flags |=1;
   Free_key_entry(&key);
   return res;
}
