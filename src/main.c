/* flrn v 0.1                                                           */
/*              main.c  	      22/11/97                          */
/*                                                                      */
/* Programme principal de flrn. Pour l'intant, ne fait quasiment        */
/* rien (normal, ce n'est pas prevu pour faire quelque chose).          */
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "flrn.h"
#include "options.h"
#include "group.h"
#include "version.h"

int debug;
struct passwd *flrn_user;
char *mailbox;

void Usage(char *argv[])
{
  printf("Utilisation : %s [-d] [-c] [-v] [-s] [<newsgroup>]\n",argv[0]);
}

void Help(char *argv[])
{
  Usage(argv);
  printf(
" -d|--debug  : affiche plein d'informations sur la sortie d'erreur\n"
" -c|--co     : donne les nouveaux newsgroups, et le nombre d'articles non lus\n"
" -s|--show-config : donne un .flrn avec les valeurs par defaut.\n"
" -v|--version: donne la version et quitte\n"
" -h|--help   : affiche ce message\n\n%s\n",version_string);
}

int main(int argc, char *argv[])
{
  int code, res=0;
  int opt_c=0;
  int i;
  char *newsgroup=NULL;

  debug=0;
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
    if ((strncmp(argv[i],"-v",2)==0)||(strcmp(argv[i],"--version")==0))
    {printf("%s\n",version_string); exit(0);}
    if ((strncmp(argv[i],"-c",2)==0)||(strcmp(argv[i],"--co")==0))
    {opt_c=1; continue;}
    if ((strncmp(argv[i],"-h",2)==0)||(strcmp(argv[i],"--help")==0))
    {Help(argv); exit(0);}
    if ((strncmp(argv[i],"-s",2)==0)||(strcmp(argv[i],"--show-config")==0))
    {dump_variables(stdout); exit(0);}
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
  init_Flcmd_rev();
  init_Flcmd_pager_rev();
  init_options();
  init_kill_file();
  if (debug) fprintf(stderr,"Serveur : %s\n",Options.serveur_name);
  code=connect_server(Options.serveur_name, 0);
  if ((code!=200) && (code!=201)) {
     fprintf(stderr, "La connexion au serveur a échoué...\n");
     if (code>500) fprintf(stderr, "Le serveur refuse la connexion.\n");
     return 1;
  }
  adjust_time();
  get_overview_fmt();

  if (!opt_c) {
     res=Init_screen();
     if (res==0) return 1;
     res=Init_keyboard();
     if (res<0) return 1;
  }

  init_groups();
  new_groups(opt_c); 
  if (!opt_c) res=loop(newsgroup); else aff_opt_c();
  quit_server();
  free_groups(res);
  if (!opt_c) {
    Reset_screen();
    Reset_keyboard();
  }
  free_options();
#ifdef CHECK_MAIL
  if (getenv("MAIL")==NULL) free(mailbox);
#endif
  fprintf(stdout,"That's all folks !\n");
  return 0;
}
