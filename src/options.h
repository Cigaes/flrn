/* flrn v 0.1                                                           */
/*              options.h             22/11/97                          */
/*                                                                      */
/* Fichier de headers pour options.c : contient la syntaxe des options  */
/* de flrn.                                                             */
/*                                                                      */

#ifndef FLRN_OPTIONS_H
#define FLRN_OPTIONS_H

#include "flrn_slang.h"
#include "art_group.h"
#include "flrn_color.h"

#define OPT_COMMENT_CHAR '#'

#define OPT_HEADER "header"
#define OPT_HEADER_LEN 6

#define OPT_SET_COLOR "color"
#define OPT_SET_COLOR_LEN 5

#define OPT_SET_MONO "mono"
#define OPT_SET_MONO_LEN 4

#define OPT_SET_NEWCOLOR "regcolor"
#define OPT_SET_NEWCOLOR_LEN 8

#define OPT_SET "set"
#define OPT_SET_LEN 3

#define OPT_BIND "bind"
#define OPT_BIND_LEN 4

#define MAX_HEADER_LIST 20
extern Known_Headers unknown_Headers[MAX_HEADER_LIST];
	/* on va les reperer en négatif de -2 à -MAX_HEADER_LIST-1 */


struct Option_struct {
  char *serveur_name;     /* option serveur   */
  int  port;				 /* Option port	     */
  char *post_name;			 /* options post_name */
  int  header_list[MAX_HEADER_LIST];     /* ordre des headers  */
  int  weak_header_list[MAX_HEADER_LIST]; /* headers "faibles" */
  int  hidden_header_list[MAX_HEADER_LIST]; /* headers a cacher */
  int  skip_line;                /* nombre de lignes avant le header */
  int  color;
  int  cbreak;
  int  new_mode;
  int  space_is_return;
  int  inexistant_arrow;
  int  edit_all_headers;
  int  simple_post;
  int  include_in_edit;
  int  date_in_summary;
  int  duplicate_subject;
  int  use_mailbox;
  int  ordered_summary;
  int  threaded_space;
  int  auto_edit;
  int  use_regexp;
  int  use_menus;
  int  quit_if_nothing;
  char *auto_subscribe;
  char *auto_ignore;
  int  subscribe_first;
  int  default_subscribe;
  char *index_string;
  int smart_quote;
  int zap_change_group;
  int scroll_after_end;
  char *attribution;
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
#define MAKE_OPT(a) {#a,OPT_TYPE_BOOLEAN,{0,0,0},{&Options.a}}
#define MAKE_OPT_NAME(a,b) {#a,OPT_TYPE_BOOLEAN,{0,0,0},{&Options.b}}
#define MAKE_OPT_REVNAME(a,b) {#a,OPT_TYPE_BOOLEAN,{0,1,0},{&Options.b}}
#define MAKE_OPT_L(a) {#a,OPT_TYPE_BOOLEAN,{1,0,0},{&Options.a}}
#define MAKE_INTEGER_OPT(a) {#a,OPT_TYPE_INTEGER,{0,0,0},{&Options.a}}
#define MAKE_INTEGER_OPT_NAME(a,b) {#a,OPT_TYPE_INTEGER,{0,0,0},{&Options.b}}
#define MAKE_INTEGER_OPT_REVNAME(a,b) {#a,OPT_TYPE_INTEGER,{0,1,0},{&Options.b}}
#define MAKE_INTEGER_OPT_L(a) {#a,OPT_TYPE_INTEGER,{1,0,0},{&Options.a}}

#ifdef __GNUC__
  /* une extension de gcc que j'aime bien... */
#  define MAKE_STRING_OPT(a) {#a,OPT_TYPE_STRING,{0,0,0},{string:&Options.a}}
#  define MAKE_STRING_OPT_NAME(a,b) {#a,OPT_TYPE_STRING,{0,0,0},{string:&Options.b}}
#  define MAKE_STRING_OPT_NAME_L(a,b) {#a,OPT_TYPE_STRING,{1,0,0},{string:&Options.b}}
#else /* __GNUC__ */
#  define MAKE_STRING_OPT(a) {#a,OPT_TYPE_STRING,{0,0,0},{(void *)&Options.a}}
#  define MAKE_STRING_OPT_NAME(a,b) {#a,OPT_TYPE_STRING,{0,0,0},{(void *)&Options.b}}
#  define MAKE_STRING_OPT_NAME_L(a,b) {#a,OPT_TYPE_STRING,{1,0,0},{(void *)&Options.b}}
#endif
/* la liste des options 
 * en ordre alphabétique... Peut-être y a-t-il mieux ?
 * en tout cas c'est mieux qu'avant */

static struct {
  char *name;
  short int type;
  struct {
    unsigned int lock :1;	/* la valeur ne peut être changée qu-au début */
    unsigned int reverse:1;	/* il faut afficher !value.integer */
    unsigned int allocated:1;	/* la mémoire doit être libérée */
  } flags;
  union { int *integer; char **string; } value;
} All_options[] = {
  MAKE_STRING_OPT(attribution),
  MAKE_OPT(auto_edit),
  MAKE_STRING_OPT(auto_ignore),
  MAKE_STRING_OPT(auto_subscribe),
  MAKE_OPT(cbreak),
  MAKE_OPT(color),
  MAKE_OPT_REVNAME(cool_arrows,inexistant_arrow),
  MAKE_OPT(date_in_summary),
  MAKE_OPT(default_subscribe),
  MAKE_OPT(duplicate_subject),
  MAKE_OPT(edit_all_headers),
  MAKE_OPT(include_in_edit),
  MAKE_STRING_OPT(index_string),
  MAKE_OPT(new_mode),
  MAKE_OPT(ordered_summary),
  MAKE_INTEGER_OPT_L(port),
  MAKE_STRING_OPT(post_name),
  MAKE_OPT(quit_if_nothing),
  MAKE_OPT(scroll_after_end),
  MAKE_STRING_OPT_NAME_L(server,serveur_name),
  MAKE_OPT(simple_post),
  MAKE_INTEGER_OPT(skip_line),
  MAKE_OPT(space_is_return),
  MAKE_OPT(smart_quote),
  MAKE_OPT(subscribe_first),
  MAKE_OPT(threaded_space),
  MAKE_OPT(use_mailbox),
  MAKE_OPT(use_menus),
  MAKE_OPT(use_regexp),
  MAKE_OPT(zap_change_group),
};


#endif /* IN_OPTION_C */

#endif
