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
#include "enc/enc_strings.h"
/* les langages. Les #iddef sont directement dans les fichiers inclus */
#include "slang_flrn.h"

static UNUSED char rcsid[]="$Id$";

extern int with_direc;
int debug;
int stupid_terminal=0;
struct passwd *flrn_user;
char *mailbox;
flrn_char *name_program;

void Usage(char *argv[])
{
  printf("Utilisation : %s [-d] [-c] [-C] [-v] [-s server[:port]] [--show-config] [--stupid-term] [-n name] [<newsgroup>]\n",argv[0]);
}

void Help(char *argv[])
{
  Usage(argv);
  printf(
" -d|--debug  : affiche plein d'informations sur la sortie d'erreur\n"
" -c|--co     : donne les nouveaux newsgroups, et le nombre d'articles non lus\n"
" -C|--Co     : comme -c, mais sans distinguer abonné/non abonné\n"
" -s|--server : impose le nom du serveur (et éventuellement le port)\n"
" -n|--name   : change le nom d'appel\n"
" --stupid-term : cette option activee, le terminal ne defile jamais\n"
" --show-config : donne un .flrn avec les valeurs par defaut.\n"
" -v|--version: donne la version et quitte\n"
" -h|--help   : affiche ce message\n\n%s\n",version_string);
}

/* Cette fonction peut être appelée par tty_keyboard (les handlers de signaux).
   Elle s'occupe de gérer la sauvegarde finale des fichiers.
   Appel actuel : res=1 -> on sauve     res=0 -> on sauve pas .
   Chaque fonction devra veiller à ne pas planter en cas de deux appels
   consécutifs, ne pas libérer deux fois la même chose. Ce n'est pas que
   ce soit très grave, mais il est théoriquement possible qu'un signal mal
   placé crée un double-appel */
void save_and_free_all(int res) {
  if (res) save_history();
  free_groups(res);
  free_options();
  free_kill(); /* pour sauver le kill-file... */
  free_highlights();
  free_Macros();
#ifdef CHECK_MAIL
  if ((getenv("MAIL")==NULL) && (mailbox)) {
    free(mailbox);
    mailbox=NULL;
  }
#endif
}

int main(int argc, char *argv[])
{
  int code, res, res_opt_c=0, rc;
  int opt_c=0;
  int i;
  flrn_char *newsgroup=NULL;
  char *newsgroup_opt=NULL,*serveur_impose=NULL, *buf;
  char *tmp_name_program;

  debug=0; with_direc=-1;
  tmp_name_program=strrchr(argv[0],'/');
  if (tmp_name_program) tmp_name_program++; else
  tmp_name_program=argv[0];
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
        tmp_name_program=argv[++i];  
      continue;
    }
    if ((strncmp(argv[i],"-v",2)==0)||(strcmp(argv[i],"--version")==0))
    {print_version_info(stdout, "flrn"); exit(0);}
    if ((strncmp(argv[i],"-c",2)==0)||(strcmp(argv[i],"--co")==0))
    {opt_c=1; continue;}
    if ((strncmp(argv[i],"-C",2)==0)||(strcmp(argv[i],"--Co")==0))
    {opt_c=2; continue;}
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
    if (argv[i][0]!='-') { newsgroup_opt=argv[i]; continue; }
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
  init_charsets ();
  init_conv_base ();
  res=conversion_from_terminal(tmp_name_program, &name_program,0,(size_t)(-1));
  if (res>0) name_program=safe_flstrdup(name_program);
  res=0;
#ifdef USE_SLANG_LANGUAGE
  if (flrn_init_SLang()<0) {
     fprintf(stdout, "Erreur dans l'initialisation du langage SLang.\nImpossible d'utiliser le langage.\n");
  }
#endif
  init_assoc_key_cmd();
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
	Options.port=(int)strtol(buf,NULL,10);
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
     free(name_program);
     return 1;
  }
  adjust_time();
  code=get_overview_fmt();
  if (code==-2) {
     fprintf(stderr, "Il semble que le serveur cherche une authentification pour continuer...\n");
     quit_server();
     free(name_program);
     return 1;
  }

  init_groups(); /* on y voit un exit */
  		 /* donc il faut le faire avant l'init de l'ecran */
  if ((opt_c==0) || (newsgroup_opt==NULL)) new_groups(opt_c); 
  if (opt_c==0) {
     load_history();
     res=Init_screen(stupid_terminal);
     if (res==0) { free(name_program); return 1; }
     res=Init_keyboard(1);
     if (res<0) { free(name_program); return 1; }
  }

  if (newsgroup_opt) 
      rc=conversion_from_terminal(newsgroup_opt,&newsgroup,0,(size_t)(-1));
  else rc=1;
  if (!opt_c) res=loop(newsgroup); else res_opt_c=aff_opt_c(newsgroup,opt_c==1);
  if (rc==0) free(newsgroup);
  if (!opt_c) {
    Reset_screen();
    Reset_keyboard();
  }
  if (res) new_groups(0);
  quit_server();
  /* On fait la fin séparément */
  save_and_free_all(res);
  if (!opt_c) fprintf(stdout,_("That's all folks !\n"));
  free(name_program);
  return 0;
}
