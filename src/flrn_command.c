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

#include "flrn.h"
#include "options.h"
#include "flrn_macros.h"
#include "flrn_command.h"
#include "flrn_comp.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_pager.h"
#include "flrn_menus.h"

static UNUSED char rcsid[]="$Id$";

/* I)  Bindings de commandes */
#define DENIED_CHARS "0123456789<>.,_*-"

/* le tableau touche -> commande */
int Flcmd_rev[NUMBER_OF_CONTEXTS][MAX_FL_KEY];
Flcmd_macro_t *Flcmd_macro=NULL;
int Flcmd_num_macros=0;
static int Flcmd_num_macros_max=0;

/* cree une macro
 * maintenant, on recherche une place libre avant d'ajouter a la fin...
 * donc plusieurs bind sur la meme touche ne doivent plus faire augmenter
 * la consommation memoire */
static int do_macro(int cmd, char *arg) {
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
  if (arg != NULL) 
    Flcmd_macro[num].arg = safe_strdup(arg);
  else
    Flcmd_macro[num].arg = NULL;
  Flcmd_macro[num].next_cmd = -1;
  return num;
}

static void del_macro(int num) {
  if (Flcmd_macro[num].arg)
    free(Flcmd_macro[num].arg);
  Flcmd_macro[num].cmd = -1;
  return;
}

/* renvoie -1 en cas de pb
 * si add!=0, on ajoute la commande... */
int Bind_command_new(int key, int command, char *arg, int context, int add ) {
  int parcours,*to_add=NULL;
  int mac_num,mac_prec=-1;
  if (add) {
    if (Flcmd_rev[context][key] < 0) {
      return -1;
    }
    parcours=Flcmd_rev[context][key];
    while ((parcours>-1) && (parcours & FLCMD_MACRO)) {
      mac_prec=parcours ^ FLCMD_MACRO;
      parcours=Flcmd_macro[parcours ^ FLCMD_MACRO].next_cmd;
    }
    if (parcours>-1) {
      mac_num = do_macro(parcours ,NULL);
      if (mac_prec==-1) Flcmd_rev[context][key] = mac_num | FLCMD_MACRO;
          else Flcmd_macro[mac_prec].next_cmd=mac_num | FLCMD_MACRO;
      to_add = & Flcmd_macro[mac_num].next_cmd;
    } else {
      to_add = & Flcmd_macro[mac_prec].next_cmd;
    }
  } else {
    to_add = &Flcmd_rev[context][key];
    parcours = Flcmd_rev[context][key];
    while ((parcours>-1) && (parcours & FLCMD_MACRO)) {
       /* pour liberer la macro */
       mac_prec=parcours ^ FLCMD_MACRO;
       parcours=Flcmd_macro[parcours ^ FLCMD_MACRO].next_cmd;
       del_macro(mac_prec);
    }
  }
  if (arg != NULL) {
    mac_num = do_macro(command,arg);
    *to_add = mac_num | FLCMD_MACRO;
    return 0;
  } else {
    *to_add = command;
    return 0;
  }
}

void free_Macros(void) {
   int i;
   for (i=0;i<Flcmd_num_macros;i++) {
     if (Flcmd_macro[i].arg);
       free(Flcmd_macro[i].arg);
   }
   Flcmd_num_macros=0;
   Flcmd_num_macros_max=0;
   free(Flcmd_macro);
}

static char *get_context_name (void *ptr, int num)
{
   return ((char **)ptr)[num];
}

int keybindings_comp(char *str, int len, Liste_Chaine *debut) {
   Liste_Chaine *courant;
   int i, res;
   int result[NUMBER_OF_CONTEXTS];

   for (i=0;i<NUMBER_OF_CONTEXTS;i++) result[i]=0;
   res = Comp_generic(debut, str,len,(void *)Noms_contextes,NUMBER_OF_CONTEXTS,
         get_context_name," ",result,1);
   if (res==-3) return 0;
   if (res>= 0) {
       if (str[0]) debut->complet=0;
       strcat(debut->une_chaine,str);
       return 0;
   }
   if (res==-1) {
      courant=debut->suivant;
      for (i=0;i<NUMBER_OF_CONTEXTS;i++) {
        if (result[i]==0) continue;
	strcat(courant->une_chaine,str);
	if (str[0]) courant->complet=0;
        courant=courant->suivant;
      }
      return -1;
   }  
   return -2;
}

/* 0 : un truc, -1 : rien -2 : trop court, -3 : spécial */
int aff_ligne_binding(int ch, int contexte, char *ligne, int len) {
   int i, taille, ret=-1;
   int compte,cmd;
   char *buf;

   ligne[len-1]='\0';
   if (len<20) return -2;
   taille=len-13;
   if (contexte==-1) taille=taille/NUMBER_OF_CONTEXTS;
   strcpy(ligne,get_name_char(ch));
   compte=strlen(ligne);
   strncat(ligne,"            ",12-compte);
   buf=ligne+12;
   /* cas des spécials */
   switch (ch) {
      case '\\' : strncpy(buf,  _("touche de commande explicite"), len-13);
      		  return -3;
      case '@' : strncpy(buf, _("touche de commande nocbreak"), len-13);
      		 return -3;
   }
   if ((ch<256) && (strchr(DENIED_CHARS,ch)) && (ch!='-')) {
       strncpy(buf, _("touche de préfixe de commande"), len-13);
       return -3;
   }
   i=(contexte==-1 ? 0 : contexte);
   do {
      if ((cmd=Flcmd_rev[i][ch])==-1) strncpy(buf,"undef",taille-1); else {
        ret=0;
        if (cmd & FLCMD_MACRO) {
	   if (Flcmd_macro[cmd ^ FLCMD_MACRO].next_cmd!=-1) 
	     strncpy(buf,"(mult.) ",taille-1);
	   else if (contexte==-1) 
	     strncpy(buf,"(macro)", taille-1); else *buf='\0';
	   if (contexte!=-1) {
	      int cmd2;
	      cmd2=Flcmd_macro[cmd ^ FLCMD_MACRO].cmd;
	      compte=strlen(buf);
	      switch (i) {
	        case CONTEXT_COMMAND : strncat(buf,Flcmds[cmd2].nom,taille-compte-1); 
				break;
                case CONTEXT_PAGER : strncat(buf,Flcmds_pager[cmd2],taille-compte-1); 
				break;
                case CONTEXT_MENU : strncat(buf,Flcmds_menu[cmd2],taille-compte-1); 
				break;
	      }
	      compte=strlen(buf);
	      if (compte<taille-2) {
	         strcat(buf," ");
		 if (Flcmd_macro[cmd ^ FLCMD_MACRO].arg) 
		    strncat(buf,Flcmd_macro[cmd ^ FLCMD_MACRO].arg,
		                taille-compte-1);
	         else strncat(buf,"...",taille-compte-1);
              }
	   }
	} else {
          switch (i) {
	    case CONTEXT_COMMAND : strncpy(buf,Flcmds[cmd].nom,taille-1); break;
	    case CONTEXT_PAGER : strncpy(buf,Flcmds_pager[cmd],taille-1); break;
	    case CONTEXT_MENU : strncpy(buf,Flcmds_menu[cmd],taille-1); break;
	  }
        }
      }
      compte=strlen(buf);
      while (compte<taille) {
         buf[compte++]=' ';
      }
      buf=buf+taille;
      i++;
   } while ((contexte==-1) && (i<NUMBER_OF_CONTEXTS));
   *buf='\0';
   return ret;
}

/* II) : complétion d'une commande quelconque */


/* TODO : généraliser ça mieux */
static char * get_command_name_tout(void * rien, int num) {
    int n;
    if ((n=num-NB_FLCMD)<0) return Flcmds[num].nom; else
    if ((num=n-NB_FLCMD_PAGER)<0) return Flcmds_pager[n]; else
	return Flcmds_menu[num];
}

/* Comp_cmd_explicite ne s'occupe pas de connaitre le contexte. C'est */
/* ennuyeux. Il faudrait améliorer ça un jour...		      */
int Comp_cmd_explicite(char *str, int len, Liste_Chaine *debut)
{
  int res,res2=-2,i,nbflcmd, bon;
  char *suite;
  Liste_Chaine *courant, *pere, *suivant;
  int result[NB_FLCMD+NB_FLCMD_PAGER+NB_FLCMD_MENU];

  if (*str=='\\') {
    str++; strcat(debut->une_chaine,"\\");
  }
  /* TODO : améliorer ça */
  nbflcmd=NB_FLCMD+NB_FLCMD_PAGER+NB_FLCMD_MENU;
  for (i=0;i<nbflcmd;i++) result[i]=0;
  res = Comp_generic(debut,str,len,NULL,nbflcmd, get_command_name_tout,
	      " ",result,1);
  if (res==-3) return 0;
  if (res>=0) {
    if ((res<NB_FLCMD) && (Flcmds[res].comp)) {
       return (*Flcmds[res].comp)(str,len,debut);
    } else {
      strcat(debut->une_chaine,str);
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
         suite=safe_strdup(str);
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
         strcat(courant->une_chaine,str);
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
       strcat(courant->une_chaine,str);
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

char *Noms_contextes[NUMBER_OF_CONTEXTS] = {
   "command", "pager", "menu" };
#define MAX_CHAR_STRING 100
/* on ne met pas le '-', hélas, pour des raisons classiques */

int get_command_nocbreak(int, int, int, int, Cmd_return *);
int get_command_explicite(char *, int, int, int, Cmd_return *);
int get_command_newmode(int, int, int, int, Cmd_return *);
int Lit_cmd_key(int, int, int, Cmd_return *);
int Lit_cmd_explicite(char *, int, int, Cmd_return *);

static int aff_context(int princip, int second) {
   char chaine[18];

   sprintf(chaine,"(%s",Noms_contextes[princip]);
   if (second>=0) {
      strcat(chaine,"/");
      strcat(chaine,Noms_contextes[second]);
   }
   strcat(chaine,") : ");
   return Aff_fin(chaine);
}
   
int get_command(int key_depart, int princip, int second, 
		Cmd_return *la_commande) {
   int key, col,i, res;

   la_commande->before=la_commande->after=NULL;
   la_commande->maybe_after=0;
   for (i=0; i<NUMBER_OF_CONTEXTS; i++) la_commande->cmd[i]=FLCMD_UNDEF;
   if (!Options.cbreak) {
      col=aff_context(princip, second);
      return get_command_nocbreak(key_depart,col,princip,second,
                                        la_commande);
   }
   if (key_depart) key=key_depart; else key=Attend_touche();
   if (key==fl_key_nocbreak) {
      col=aff_context(princip, second);
      return get_command_nocbreak(fl_key_nocbreak,col,princip,second,
					la_commande);
   }
   if (key==fl_key_explicite) {
      col=aff_context(princip, second);
      return get_command_explicite(NULL,col,princip,second,la_commande);
   } else {
      /* Beurk pour '-' */
      if ((key=='-') || (strchr(DENIED_CHARS,key)==NULL)) {
                   res=Lit_cmd_key(key,princip,second,la_commande);
		   if ((res>=0) && (key!='\r')) la_commande->maybe_after=1;
      } else {
         col=aff_context(princip, second);
         res=get_command_newmode(key,col,princip,second,la_commande);
      }
   }
   return res;
}

/* Renvoie le nom d'une commande par touche raccourcie */
int Lit_cmd_key(int key, int princip, int second, Cmd_return *la_commande) {
   int res=-1, j, context;
   if ((key <0) || (key >= MAX_FL_KEY)) return -1;
   for (j=0; j<2; j++) {
      context=(j==0 ? princip : second);
      if (context==-1) break;
      /* CAS PARTICULIER : la touche entrée */
      if ((key=='\r') && (la_commande->before) && (context==CONTEXT_COMMAND))
      {
	  res=(res==-1 ? j : res);
	  la_commande->cmd[CONTEXT_COMMAND]=FLCMD_VIEW;
      } else
      if ((la_commande->cmd[context]=Flcmd_rev[context][key])!=FLCMD_UNDEF)
          res=(res==-1 ? j : res);
   }
   return res;
}


/* Lit le nom explicite d'une commande */
int Lit_cmd_explicite(char *str, int princip, int second, Cmd_return *la_commande) {
   int i, j, res=-1;
     
   /* Hack pour les touches fleches en no-cbreak */
   if (strncmp(str, "key-up ",7)==0) 
        return Lit_cmd_key(FL_KEY_UP, princip, second, la_commande);
   if (strncmp(str, "key-down ",9)==0) 
        return Lit_cmd_key(FL_KEY_DOWN, princip, second, la_commande);
   if (strncmp(str, "key-left ",9)==0) 
        return Lit_cmd_key(FL_KEY_LEFT, princip, second, la_commande);
   if (strncmp(str, "key-right ",10)==0) 
        return Lit_cmd_key(FL_KEY_RIGHT, princip, second, la_commande);
   /* cas "normal" */
   
   for (j=0;j<2;j++) {
      switch (j==0 ? princip : second) {
      /* TODO : unifier un peu mieux ça. C'est vrai quoâ ! */
         case CONTEXT_COMMAND : for (i=0;i<NB_FLCMD;i++) 
                if ((strncmp(str, Flcmds[i].nom, strlen(Flcmds[i].nom))==0)
                    && ((str[strlen(Flcmds[i].nom)]=='\0') ||
                    (isblank(str[strlen(Flcmds[i].nom)])))) {
		  la_commande->cmd[CONTEXT_COMMAND]=i;
		  res=(res==-1 ? j : res);
		  break;
		}
		break;
         case CONTEXT_MENU : for (i=0;i<NB_FLCMD_MENU;i++) 
                if ((strncmp(str, Flcmds_menu[i], strlen(Flcmds_menu[i]))==0)
                    && ((str[strlen(Flcmds_menu[i])]=='\0') ||
                    (isblank(str[strlen(Flcmds_menu[i])])))) {
		  la_commande->cmd[CONTEXT_MENU]=i;
		  res=(res==-1 ? j : res);
		  break;
		}
		break;
         case CONTEXT_PAGER : for (i=0;i<NB_FLCMD_PAGER;i++) 
                if ((strncmp(str, Flcmds_pager[i], strlen(Flcmds_pager[i]))==0)
                    && ((str[strlen(Flcmds_pager[i])]=='\0') ||
                    (isblank(str[strlen(Flcmds_pager[i])])))) {
		  la_commande->cmd[CONTEXT_PAGER]=i;
		  res=(res==-1 ? j : res);
		  break;
		}
		break;
      }
   }
   return res;
}

/* Prend une commande explicite */
/* retourne -2 si rien */
int get_command_explicite(char *start, int col, int princip, int second, Cmd_return *la_commande) {
   int res=0, ret=0;
   char cmd_line[MAX_CHAR_STRING], *str=cmd_line;
   int prefix_len=0;
   cmd_line[0]='\0';
   if (start) {
     prefix_len = strlen(start);
     strncpy (cmd_line, start, MAX_CHAR_STRING-2);
   }
   strcat(cmd_line,"\\");
   prefix_len++;
   do {
     Aff_ligne_comp_cmd(str,strlen(str),col);
     if ((res=magic_flrn_getline(str+prefix_len,MAX_CHAR_STRING-prefix_len,
         Screen_Rows2-1,col+prefix_len,"\011",0,ret))<0)
       return -2;
     ret=0;
     if (res>0) ret=Comp_general_command(str+prefix_len,MAX_CHAR_STRING-prefix_len,col+prefix_len,Comp_cmd_explicite, Aff_ligne_comp_cmd);
     if (ret<0) ret=0;
   } while (res!=0);
   res=Lit_cmd_explicite(str+prefix_len, princip, second, la_commande);
   str=strchr(str,' ');
   if (str) la_commande->after=safe_strdup(str+1);
   return res;
}

/* Prend une commande en nocbreak */
int get_command_nocbreak(int asked,int col, int princip, int second, Cmd_return *la_commande) {
   char cmd_line[MAX_CHAR_STRING];
   char *str=cmd_line,*buf;
   int res, key;

   /* Dans le cas où asked='\r', et vient donc directement d'une interruption */
   /* on ne prend pas de ligne de commande : on l'a déjà...                   */
   if (asked=='\r') return Lit_cmd_key('\r',princip,second,la_commande);
   if (asked) Screen_write_char(asked);
   if (asked && (asked!=fl_key_nocbreak))  *(str++)=asked;
   *str='\0';
   str=cmd_line;
   if ((res=flrn_getline(str,MAX_CHAR_STRING,Screen_Rows2-1,
                      col+(asked && (asked==fl_key_nocbreak))))<0)
     return -2;
   while(*str==fl_key_nocbreak) str++;
   if (str[0]=='\0') return Lit_cmd_key('\r',princip,second,la_commande);
   /* Beurk pour le '-' */
   if ((str[0]=='-') && (str[1]=='\0'))
       return Lit_cmd_key('-',princip,second,la_commande);
   buf=str+strspn(str,DENIED_CHARS);
   key=(*buf ? *buf : '\r');
   if (*buf) *(buf++)='\0';
   if (*str) la_commande->before=safe_strdup(str);
   if (key==fl_key_explicite) {
       res=Lit_cmd_explicite(buf, princip, second, la_commande);
       buf=strchr(buf,' '); if (buf) buf++;
   } else res=Lit_cmd_key(key,princip,second,la_commande);
   /* Ne pas oublier !!!!! */
   /* dans Lit_cmd_key le cas très particulier de \r qui devient 'v' pour un */
   /* contexte particulier...						     */
   if (*buf) la_commande->after=safe_strdup(buf);
   return res;
}

/* Prend une commande dans le nouveau mode */
int get_command_newmode(int key,int col, int princip, int second, Cmd_return *la_commande) {
   int res;
   char cmd_line[MAX_CHAR_STRING];
   char *str=cmd_line;

   cmd_line[0]=key; cmd_line[1]='\0';
   Screen_write_char(key);
   /* On appelle magic_flrn_getline avec flag=1 */
   if ((key=magic_flrn_getline(str,MAX_CHAR_STRING,Screen_Rows2-1,col,DENIED_CHARS,1,0))<0)
      return -2;
   if (*str) la_commande->before=safe_strdup(str);
   if (key==fl_key_explicite) 
       return get_command_explicite(cmd_line,col,princip,second,la_commande); 
   else 
       res=Lit_cmd_key(key,princip,second,la_commande);
   if ((res>=0) && (key!='\r')) la_commande->maybe_after=1;
   return res;
}

