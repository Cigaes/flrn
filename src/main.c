/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      main.c : parsing de la ligne de commande
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "flrn.h"
#include "flrn_tags.h"
#include "options.h"
#include "group.h"
#include "version.h"
#include "flrn_command.h"
#include "flrn_tcp.h"
#include "flrn_files.h" 
#include "flrn_xover.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "flrn_inter.h"
#include "flrn_menus.h"
#include "flrn_pager.h"
#include "flrn_filter.h"
#include "flrn_color.h"
#ifdef WITH_CHARACTER_SETS
#include "rfc2045.h"
#endif
/* les langages. Les #iddef sont directement dans les fichiers inclus */
#include "slang_flrn.h"

static UNUSED char rcsid[]="$Id$";

extern int with_direc;
int debug;
int stupid_terminal=0;
struct passwd *flrn_user;
char *mailbox;
char *name_program;

void Usage(char *argv[])
{
  printf("Utilisation : %s [-d] [-c] [-v] [-s server[:port]] [--show-config] [--stupid-term] [-n name] [<newsgroup>]\n",argv[0]);
}

void Help(char *argv[])
{
  Usage(argv);
  printf(
" -d|--debug  : affiche plein d'informations sur la sortie d'erreur\n"
" -c|--co     : donne les nouveaux newsgroups, et le nombre d'articles non lus\n"
" -s|--server : impose le nom du serveur (et éventuellement le port)\n"
" -n|--name   : change le nom d'appel\n"
" --stupid-term : cette option activee, le terminal ne defile jamais\n"
" --show-config : donne un .flrn avec les valeurs par defaut.\n"
" -v|--version: donne la version et quitte\n"
" -h|--help   : affiche ce message\n\n%s\n",version_string);
}

int main(int argc, char *argv[])
{
  int code, res=0, res_opt_c=0;
  int opt_c=0;
  int i;
  char *newsgroup=NULL, *serveur_impose=NULL, *buf;

  debug=0; with_direc=-1;
  name_program=strrchr(argv[0],'/');
  if (name_program) name_program++; else
  name_program=argv[0];
  /* J'aime pas les sigpipe */
  {
    struct sigaction ign;
    /* les champs n'ont pas le même nom sous tous les os */
    /* par contre, on veut toujours les mettre a 0 */
    memset(&ign,0,sizeof(struct sigaction));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGPIPE,&ign,NULL);
  }

  for (i=1; i<argc; i++)
  {
    if ((strncmp(argv[i],"-d",2)==0)||(strcmp(argv[i],"--debug")==0))
    {debug=1; continue;}
    if ((strncmp(argv[i],"-n",2)==0)||(strcmp(argv[i],"--name")==0)) {
      if ((i+1!=argc) && (*(argv[i+1])!='-'))
        name_program=argv[++i];  
      continue;
    }
    if ((strncmp(argv[i],"-v",2)==0)||(strcmp(argv[i],"--version")==0))
    {print_version_info(stdout, "flrn"); exit(0);}
    if ((strncmp(argv[i],"-c",2)==0)||(strcmp(argv[i],"--co")==0))
    {opt_c=1; continue;}
    if (strcmp(argv[i],"--stupid-term")==0) { stupid_terminal=1; continue; }
    if ((strncmp(argv[i],"-h",2)==0)||(strcmp(argv[i],"--help")==0))
    {Help(argv); exit(0);}
    if (strcmp(argv[i],"--show-config")==0)
    {dump_variables(stdout); exit(0);}
    if ((strncmp(argv[i],"-s",2)==0)||(strcmp(argv[i],"--server")==0)) {
      if ((i+1!=argc) && (*(argv[i+1])!='-'))
        serveur_impose=argv[++i];  
      continue;
    }
    if (argv[i][0]!='-') { newsgroup=argv[i]; continue; }
    printf("Option non reconnue :%s\n",argv[i]);
    Usage(argv);
    exit(1);
  }
  flrn_user=getpwuid(getuid());
#ifdef CHECK_MAIL
  mailbox = getenv("MAIL");
  if (!mailbox) {
     mailbox=safe_malloc(strlen(DEFAULT_MAIL_PATH)+
     				strlen(flrn_user->pw_gecos)+1);
     strcpy(mailbox,DEFAULT_MAIL_PATH);
     strcat(mailbox,flrn_user->pw_gecos);
  }
#endif
#ifdef WITH_CHARACTER_SETS
  init_charsets ();
#endif
#ifdef USE_SLANG_LANGUAGE
  if (flrn_init_SLang()<0) {
     fprintf(stdout, "Erreur dans l'initialisation du langage SLang.\nImpossible d'utiliser le langage.\n");
  }
#endif
  init_Flcmd_rev();
  init_Flcmd_pager_rev();
  init_Flcmd_menu_rev();
  init_options();
  if (serveur_impose) {
     Options.serveur_name=safe_strdup(serveur_impose);
     buf=strrchr(Options.serveur_name,':');
     Options.port=DEFAULT_NNTP_PORT;
     if (buf) {
        *(buf++)='\0';
	Options.port=atoi(buf);
     }
     fprintf(stdout,"Serveur : %s    port : %i\n",Options.serveur_name,
        Options.port);
  }
  load_help_line_file();
  init_kill_file();
  if (debug) fprintf(stderr,"Serveur : %s\n",Options.serveur_name);
  code=connect_server(Options.serveur_name, 0);
  if ((code!=200) && (code!=201)) {
     fprintf(stderr, "La connexion au serveur a échoué...\n");
     if (code>500) fprintf(stderr, "Le serveur refuse la connexion.\n");
     return 1;
  }
  adjust_time();
  code=get_overview_fmt();
  if (code==-2) {
     fprintf(stderr, "Il semble que le serveur cherche une authentification pour continuer...\n");
     quit_server();
     return 1;
  }

  init_groups(); /* on y voit un exit */
  		 /* donc il faut le faire avant l'init de l'ecran */
  if ((opt_c==0) || (newsgroup==NULL)) new_groups(opt_c); 
  if (!opt_c) {
     load_history();
     res=Init_screen(stupid_terminal);
     if (res==0) return 1;
     res=Init_keyboard(1);
     if (res<0) return 1;
  }

  if (!opt_c) res=loop(newsgroup); else res_opt_c=aff_opt_c(newsgroup);
  if (!opt_c) {
    Reset_screen();
    Reset_keyboard();
  }
  if (res) new_groups(0);
  quit_server();
  if (res) save_history();
  free_groups(res);
  free_options();
  free_kill(); /* pour sauver le kill-file... */
  free_highlights();
  free_Macros();
#ifdef CHECK_MAIL
  if (getenv("MAIL")==NULL) free(mailbox);
#endif
  if (!opt_c) fprintf(stdout,"That's all folks !\n");
  return 0;
}
