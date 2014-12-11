/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Mass� et Jo�l-Yann Fourr�
 *
 *      options.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#ifndef FLRN_OPTIONS_H
#define FLRN_OPTIONS_H

#include <stdio.h>
#include "compatibility.h"
#include "config.h"
#include "flrn_config.h"
#include "flrn_slang.h"
#include "art_group.h"
#include "flrn_color.h"
#include "flrn_comp.h"

#ifdef IN_OPTION_C
#  define OPT_COMMENT_CHAR '#'

#  define WORD_NAME_PROG "name"

/* fonctions pour parser une option */
int opt_do_header(flrn_char *, int);
int opt_do_my_hdr(flrn_char *, int);
int opt_do_include(flrn_char *, int);
int opt_do_color(flrn_char *, int);
int opt_do_mono(flrn_char *, int);
int opt_do_regcolor(flrn_char *, int);
int opt_do_set(flrn_char *, int);
int opt_do_bind(flrn_char *, int);
int opt_do_my_flags(flrn_char *, int);
int opt_do_autocmd(flrn_char *, int);
#ifdef USE_SLANG_LANGUAGE
int opt_do_slang_parse(flrn_char *, int);
#endif

/* pour la comptetion automatique */
int var_comp(flrn_char *, size_t, Liste_Chaine *);
int bind_comp(flrn_char *, size_t, Liste_Chaine *);

struct _Optcmd {
  char *name;
  int (*parse)(flrn_char *, int);
  int (*comp)(char *, size_t, Liste_Chaine *);
} Optcmd_liste[] = {
  {"header", &opt_do_header, NULL},
  {"my_hdr", &opt_do_my_hdr, NULL},
  {"include", &opt_do_include, NULL},
  {"my_flags", &opt_do_my_flags, NULL},
  {"color", &opt_do_color, NULL},
  {"mono", &opt_do_mono, NULL},
  {"regcolor", &opt_do_regcolor, NULL},
  {"set", &opt_do_set, &var_comp},
  {"bind", &opt_do_bind, &bind_comp},
  {"autocmd", &opt_do_autocmd, NULL},
#ifdef USE_SLANG_LANGUAGE
  {"slang_parse", &opt_do_slang_parse, NULL},
#endif
};

#define NUMBER_OF_OPT_CMD (sizeof(Optcmd_liste)/sizeof(Optcmd_liste[0]))

#endif

extern int size_header_list;

extern Known_Headers *unknown_Headers;
	/* on va les reperer en n�gatif de -2 � -size_header_list-1 */
extern const flrn_char *Help_Lines[17];

/* pour les my_hdr */
typedef struct string_list {
  flrn_char *str;
  struct string_list *next;
} string_list_type;

typedef struct autocmd_list {
  int autocmd_flags;
#define AUTOCMD_ENTER 0x01
#define AUTOCMD_LEAVE 0x02
  regex_t match;
  int cmd;
  flrn_char *arg;
  struct autocmd_list *next;
} autocmd_list_type;

struct Option_struct {
  flrn_char *serveur_name;     /* option serveur   */
  int  port;				 /* Option port	     */
  flrn_char *flnews_ext;		 /* extension du .flnewsrc */
  flrn_char *post_name;			 /* options post_name */
  int  *header_list;     /* ordre des headers  */
  int  *weak_header_list; /* headers "faibles" */
  int  *hidden_header_list; /* headers a cacher */
  string_list_type *user_header;
  string_list_type *user_flags;
  autocmd_list_type *user_autocmd;
  int  headers_scroll;
  int  skip_line;                /* nombre de lignes avant le header */
  int  color;
  int  cbreak;
#ifdef CHECK_MAIL
  int  check_mail;
#endif
  int  forum_mode;
  int  space_is_return;
  int  edit_all_headers;
  int  include_in_edit;
  int  date_in_summary;
  int  duplicate_subject;
  int  use_mailbox;
  int  ordered_summary;
  int  threaded_next;
  int  use_regexp;
  int  use_menus;
  int  with_cousins;
  int  quit_if_nothing;
  int  parse_from;
  flrn_char *auto_subscribe;
  flrn_char *auto_ignore;
  int  subscribe_first;
  int  default_subscribe;
  flrn_char *index_string;
  int smart_quote;
  int zap_change_group;
  int scroll_after_end;
  flrn_char *attribution;
  int small_tree;
  flrn_char *kill_file_name;
  int auto_kill;
  flrn_char *savepath;
  flrn_char *prefixe_groupe;
  flrn_char *flags_group;
  flrn_char *hist_file_name;
  int  warn_if_new;
  flrn_char *default_flnewsfile;
  int short_errors;
  int help_line;
  flrn_char *help_lines_file;
  flrn_char *auth_user;
  flrn_char *auth_pass;
  flrn_char *auth_cmd;
  flrn_char *sig_file;
  int quote_all;
  int quote_sig;
  flrn_char *terminal_charset;
  flrn_char *editor_charset;
  flrn_char *post_charsets;
  flrn_char *files_charset;
#ifndef DOMAIN
  flrn_char *default_domain;
#endif
  int max_group_size;
  int list_all_groups;
  flrn_char *alternate;
};

extern struct Option_struct Options;

#ifdef IN_OPTION_C

/* le nombre d'options */
#define NUM_OPTIONS (sizeof(All_options)/sizeof(All_options[0]))
/* les types d'otions */
#define OPT_TYPE_INTEGER 1
#define OPT_TYPE_BOOLEAN 2
#define OPT_TYPE_STRING 3

/* les macros pour ajouter les options */
#define MAKE_OPT(a,b) {#a,b,OPT_TYPE_BOOLEAN,{0,0,0,0,0},{&Options.a}}
#define MAKE_OPT_NAME(a,b,c) {#a,c,OPT_TYPE_BOOLEAN,{0,0,0,0,0},{&Options.b}}
#define MAKE_OPT_REVNAME(a,b,c) {#a,c,OPT_TYPE_BOOLEAN,{0,1,0,0,0},{&Options.b}}
#define MAKE_OPT_L(a,b) {#a,b,OPT_TYPE_BOOLEAN,{1,0,0,0,0},{&Options.a}}
#define MAKE_INTEGER_OPT(a,b) {#a,b,OPT_TYPE_INTEGER,{0,0,0,0,0},{&Options.a}}
#define MAKE_INTEGER_OPT_NAME(a,b,c) {#a,c,OPT_TYPE_INTEGER,{0,0,0,0,0},{&Options.b}}
#define MAKE_INTEGER_OPT_REVNAME(a,b,c) {#a,c,OPT_TYPE_INTEGER,{0,1,0,0,0},{&Options.b}}
#define MAKE_INTEGER_OPT_L(a,b) {#a,b,OPT_TYPE_INTEGER,{1,0,0,0,0},{&Options.a}}

#define MAKE_OPT_O(a,b) {#a,b,OPT_TYPE_BOOLEAN,{0,0,0,0,1},{&Options.a}}
#define MAKE_OPT_NULL_O(a,b) {#a,b,OPT_TYPE_BOOLEAN,{0,0,0,0,1},{NULL}}
#define MAKE_OPT_NAME_O(a,b,c) {#a,c,OPT_TYPE_BOOLEAN,{0,0,0,0,1},{&Options.b}}
#define MAKE_OPT_REVNAME_O(a,b,c) {#a,c,OPT_TYPE_BOOLEAN,{0,1,0,0,1},{&Options.b}}
#define MAKE_OPT_L_O(a,b) {#a,b,OPT_TYPE_BOOLEAN,{1,0,0,0,1},{&Options.a}}
#define MAKE_INTEGER_OPT_O(a,b) {#a,b,OPT_TYPE_INTEGER,{0,0,0,0,1},{&Options.a}}
#define MAKE_INTEGER_OPT_NAME_O(a,b,c) {#a,c,OPT_TYPE_INTEGER,{0,0,0,0,1},{&Options.b}}
#define MAKE_INTEGER_OPT_REVNAME_O(a,b,c) {#a,c,OPT_TYPE_INTEGER,{0,1,0,0,1},{&Options.b}}
#define MAKE_INTEGER_OPT_L_O(a,b) {#a,b,OPT_TYPE_INTEGER,{1,0,0,0,1},{&Options.a}}

#ifdef __GNUC__
  /* une extension de gcc que j'aime bien... */
#  define MAKE_STRING_OPT(a,b) {#a,b,OPT_TYPE_STRING,{0,0,0,0,0},{string:&Options.a}}
#  define MAKE_STRING_OPT_L(a,b) {#a,b,OPT_TYPE_STRING,{1,0,0,0,0},{string:&Options.a}}
#  define MAKE_STRING_OPT_NAME(a,b,c) {#a,c,OPT_TYPE_STRING,{0,0,0,0,0},{string:&Options.b}}
#  define MAKE_STRING_OPT_NAME_L(a,b,c) {#a,c,OPT_TYPE_STRING,{1,0,0,0,0},{string:&Options.b}}
#  define MAKE_STRING_OPT_O(a,b) {#a,b,OPT_TYPE_STRING,{0,0,0,0,1},{string:&Options.a}}
#  define MAKE_STRING_OPT_L_O(a,b) {#a,b,OPT_TYPE_STRING,{1,0,0,0,1},{string:&Options.a}}
#  define MAKE_STRING_OPT_NAME_O(a,b,c) {#a,c,OPT_TYPE_STRING,{0,0,0,0,1},{string:&Options.b}}
#  define MAKE_STRING_OPT_NAME_L_O(a,b,c) {#a,c,OPT_TYPE_STRING,{1,0,0,0,1},{string:&Options.b}}
#else /* __GNUC__ */
#  define MAKE_STRING_OPT(a,b) {#a,b,OPT_TYPE_STRING,{0,0,0,0,0},{(void *)&Options.a}}
#  define MAKE_STRING_OPT_L(a,b) {#a,b,OPT_TYPE_STRING,{1,0,0,0,0},{(void *)&Options.a}}
#  define MAKE_STRING_OPT_NAME(a,b,c) {#a,c,OPT_TYPE_STRING,{0,0,0,0,0},{(void *)&Options.b}}
#  define MAKE_STRING_OPT_NAME_L(a,b,c) {#a,c,OPT_TYPE_STRING,{1,0,0,0,0},{(void *)&Options.b}}

#  define MAKE_STRING_OPT_O(a,b) {#a,b,OPT_TYPE_STRING,{0,0,0,0,1},{(void *)&Options.a}}
#  define MAKE_STRING_OPT_L_O(a,b) {#a,b,OPT_TYPE_STRING,{1,0,0,0,1},{(void *)&Options.a}}
#  define MAKE_STRING_OPT_NAME_O(a,b,c) {#a,c,OPT_TYPE_STRING,{0,0,0,0,1},{(void *)&Options.b}}
#  define MAKE_STRING_OPT_NAME_L_O(a,b,c) {#a,c,OPT_TYPE_STRING,{1,0,0,0,1},{(void *)&Options.b}}
#endif
/* la liste des options 
 * en ordre alphab�tique... Peut-�tre y a-t-il mieux ?
 * en tout cas c'est mieux qu'avant */

static struct {
  char *name;
  char *desc; /* en UTF8 */
  short int type;
  struct {
    unsigned int lock :1;	/* la valeur ne peut �tre chang�e qu-au d�but */
    unsigned int reverse:1;	/* il faut afficher !value.integer */
    unsigned int allocated:1;	/* la m�moire doit �tre lib�r�e */
    unsigned int modified:1;	/* modifi� par rapport au site_config.h */
    unsigned int obsolete:1;
  } flags;
  union { int *integer; flrn_char **string; } value;
} All_options[] = {
  MAKE_OPT_NAME_O(alpha_tree,small_tree,
	        N_("Obsolete (utiliser small_tree à la place).")),
  MAKE_STRING_OPT(alternate, N_("Adresses alternatives.")),
  MAKE_STRING_OPT(attribution,N_("Chaîne précédent les citations.")),
  MAKE_STRING_OPT_L(auth_user,N_("Nom de l'utilisateur pour l'authentification par le serveur")),
  MAKE_STRING_OPT_L(auth_pass,N_("Mot de passe pour l'authentification par le serveur")),
  MAKE_STRING_OPT_L(auth_cmd,N_("Commande pour l'authentification par le serveur")),
  MAKE_OPT_NULL_O(auto_edit,N_("Obsolète (l'éditeur interne est supprimé).")),
  MAKE_STRING_OPT(auto_ignore,N_("Expression régulières de groupes à ignorer.")),
  MAKE_OPT(auto_kill,N_("Met le groupe dans le kill-file au moment de l'abonnement.")),
  MAKE_STRING_OPT(auto_subscribe,N_("Expression régulière de newsgroups à accepter.")),
  MAKE_STRING_OPT_NAME_O(character_set,terminal_charset,
	    N_("Obsolete (utiliser terminal_charset)")),
  MAKE_OPT(cbreak,N_("Mode d'entrée de commande par d�faut.")),
#ifdef CHECK_MAIL
  MAKE_OPT(check_mail,N_("Vous informe si vous avez du nouveau mail.")),
#endif
  MAKE_OPT(color,N_("Utilisation des couleurs (-1 = auto).")),
  MAKE_OPT_NULL_O(cool_arrows,N_("Obsolete (toujours activée).")),
  MAKE_OPT(date_in_summary,N_("Ajoute la date dans les résumés.")),
#ifndef DOMAIN
  MAKE_STRING_OPT(default_domain,N_("Fixe le domaine de la machine.")),
#endif
  MAKE_STRING_OPT_L(default_flnewsfile,N_("Si le .flnewsrc n'existe pas, où en chercher un.")),
  MAKE_OPT(default_subscribe,N_("Abonnement par défaut.")),
  MAKE_OPT(duplicate_subject,N_("Les sujets sont réaffichés à chaque ligne dans les résumés.")),
  MAKE_OPT(edit_all_headers,N_("Permet d'éditer tous les headers dans un post.")),
  MAKE_STRING_OPT(editor_charset,N_("Jeu de caractères de l'éditeur.")),
  MAKE_STRING_OPT(files_charset,N_("Jeu de caractères des fichiers (excepté l'éditeur).")),
  MAKE_STRING_OPT(flags_group,N_("Caractères précédents un groupe selon ses attributs.")),
  MAKE_STRING_OPT_L(flnews_ext,N_("Extension sur le nom du .flnewsrc.")),
  MAKE_OPT(forum_mode,N_("Mode de commande forum-like.")),
  MAKE_OPT(headers_scroll,N_("Les headers scrollent avec le reste des messages.")),
  MAKE_OPT(help_line,N_("Ajoute une ligne d'aide en fin d'écran.")),
  MAKE_STRING_OPT_L(help_lines_file,N_("Fichier o� trouver les lignes d'aides.")),
  MAKE_STRING_OPT(hist_file_name,N_("Nom de fichier pour sauvegarder l'historique.")),
  MAKE_OPT(include_in_edit,N_("Lorsque auto_edit est défini, inclut automatiquement le message d'origine.")),
  MAKE_STRING_OPT(index_string,N_("Caractères précédents un quote.")),
  MAKE_STRING_OPT(kill_file_name,N_("Nom du fichier de kill utilisé.")),
  MAKE_OPT(list_all_groups,N_("Liste tous les groupes d'un coup.")),
  MAKE_INTEGER_OPT(max_group_size,N_("Taille maximum de gestion d'un groupe.")),
  MAKE_OPT(ordered_summary,N_("Ordonne les résumés suivant leur num�ro.")),
  MAKE_OPT(parse_from,N_("Affiche le \"Auteur:\" et le \"Réponse à:\" plus forum-like.")),
  MAKE_INTEGER_OPT_L(port,N_("Port d'accès au serveur (119 en général).")),
  MAKE_STRING_OPT(post_name,N_("Nom de posts des messages.")),
  MAKE_STRING_OPT(post_charsets,N_("Jeux de caractères des posts (dans l'ordre de préférence).")),
  MAKE_STRING_OPT(prefixe_groupe,N_("Préfixe par défaut de noms de groupes.")),
  MAKE_OPT(quit_if_nothing,N_("Quitte si il n'y a rien de nouveau au lancement.")),
  MAKE_OPT(quote_all,N_("Inclut l'ensemble du message, en-têtes comprises.")),
  MAKE_OPT(quote_sig,N_("Inclut la signature dans la partie quotée.")),
  MAKE_STRING_OPT(savepath,N_("Répertoire où sauver les articles.")),
  MAKE_OPT(scroll_after_end,N_("Modifie le comportement final du scrolling.")),
  MAKE_STRING_OPT_NAME_L(server,serveur_name,N_("Nom du serveur de news.")),
  MAKE_OPT(short_errors,N_("flrn n'affiche les erreurs en bas qu'une seconde.")),
  MAKE_STRING_OPT(sig_file,N_("Nom du fichier de signature inclu dans l'éditeur.")),
  MAKE_INTEGER_OPT(skip_line,N_("Nombre de lignes blanches après la barre de statut.")),
  MAKE_OPT(space_is_return,N_("La commande \\next-unread ne permet pas de changer de groupe.")),
  MAKE_OPT(small_tree,N_("Affichage du petit arbre de la discussion.")),
  MAKE_OPT(smart_quote,N_("Permet de faire un quote \"intelligent\".")),
  MAKE_OPT(subscribe_first,N_("Applique auto_subscribe avant auto_ignore.")),
  MAKE_STRING_OPT(terminal_charset,N_("Jeu de caractères du terminal.")),
  MAKE_OPT(threaded_next,N_("La commande \\next-unread agit en fonction du thread, et non des numéros.")),
  MAKE_OPT_NAME_O(threaded_space,threaded_next,N_("Obsolete (utiliser threaded_next).")),
  MAKE_OPT(use_mailbox,N_("Sauve les messages au format d'une mailbox.")),
  MAKE_OPT(use_menus,N_("Utilisation des menus.")),
  MAKE_OPT(use_regexp,N_("Utilise les expresions r�gulières.")),
  MAKE_OPT(warn_if_new,N_("Avertit de l'arrivée de nouveaux groupes.")),
  MAKE_OPT(with_cousins,N_("Permet de se déplacer entre cousins.")),
  MAKE_OPT(zap_change_group,N_("La commande \\zap change automatiquement de groupe courant.")),
};


#endif /* IN_OPTION_C */

/* les fonctions */

extern void init_options(void);
extern void parse_options_line(flrn_char * /*ligne*/, int /*flag*/,
	                      int *, int *);
extern void dump_variables(FILE * /*file*/);
extern void dump_flrnrc(FILE * /*file*/);
extern int  options_comp(flrn_char * , size_t , Liste_Chaine *);
extern void free_options(void);
extern void menu_config_variables(void);
extern void load_help_line_file(void);
extern int  get_and_push_option(flrn_char *, 
	   void push_string(flrn_char *), void push_int(int));

#endif
