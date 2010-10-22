/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      flrn_shell.c : fork, system, execve, etc...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/wait.h>  /* pour le fils */
#include <sys/stat.h>  /* S_IRUSR S_IWUSR */
#include <signal.h>

#ifndef S_IRUSR
#define S_IRUSR 00400
#endif
#ifndef S_IWUSR
#define S_IWUSR 00200
#endif

#include "flrn.h"
#include "options.h"
#include "flrn_shell.h"
#include "tty_display.h"
#include "tty_keyboard.h"
#include "enc/enc_strings.h"

static UNUSED char rcsid[]="$Id$";

/* Lancement de l'éditeur sur le fichier à éditer..                    */
/* flag est pour l'instant inutilisé. 				       */
static volatile int sigwinchcatch;

void sigwinch_while_fork(int sig) {
    sigwinchcatch=1; 
    /*SL*/signal(SIGWINCH, sigwinch_while_fork);
}   

int Launch_Editor (int flag, char *name) {
    char *editor, command[16];
    pid_t pid;
    int retval=0, ret;

    sigwinchcatch=0;

    Reset_keyboard();
    Screen_suspend();

    signal(SIGTSTP, SIG_DFL); /* on s'en fout */
    signal(SIGWINCH, sigwinch_while_fork);

    editor = getenv("EDITOR") ? "$EDITOR" : getenv("VISUAL") ? "$VISUAL" :
        "editor";
    snprintf(command, sizeof(command), "%s \"$1\"", editor);
    fprintf(stderr, "\n\neditor = [%s]\n", command);
    switch ((pid=fork())) {
       case -1 : retval = -1;
                 break;
       case 0 : close(tcp_fd);
                execl("/bin/sh", "sh", "-c", command, editor, name, NULL);
                _exit(-1);
       default : while ((wait(&ret)<0) && (errno==EINTR));
    }
    if (WIFEXITED(ret) && (WEXITSTATUS(ret)==0xff)) retval=-1;
    Screen_resume();
    Init_keyboard(0); /* Ca remet SIGTSTP correct */
    signal(SIGWINCH, sig_winch);
    if (sigwinchcatch) sig_winch(0);
    sigwinch_received=0; /* pas nécessaire d'arreter l'édition */
    return retval;
}

/* ouvre le pipe et renvoie un file descr. */
/* si flag est !=0, on met la sortie std dans un fichier */
int Pipe_Msg_Start (int flagin ,int flagout, flrn_char *cmd, char *name) {
    char *home,*tmpchr;
    pid_t pid;
    int fd[2];
    int fdfile;
    char *cmdline;
    int rc=1;

    if (!cmd) {
       cmdline=getenv("PAGER");
       if (!cmdline)
           cmdline="less";
    } else rc=conversion_to_file(cmd,&cmdline,0,(size_t)(-1));

    if (flagin && (pipe(fd)<0)) {
	if (rc==0) free(cmdline); 
	return -1;
    }

    if (flagout) {
      if (NULL == (home = getenv ("FLRNHOME")))
              home = getenv ("HOME");
      if (home==NULL) {
	  if (rc==0) free(cmdline);
	  return -1;  /* TRES improbable :-) */
      }
#if (defined(USE_MKSTEMP) || defined(USE_TMPEXT))
      strncpy(name,home,MAX_PATH_LEN-10-strlen(TMP_PIPE_FILE));
#else
      strncpy(name,home,MAX_PATH_LEN-2-strlen(TMP_PIPE_FILE));
#endif
      tmpchr=name+strlen(name);
#ifdef USE_MKSTEMP
      sprintf(tmpchr,"/%s.XXXXXX",TMP_PIPE_FILE);
      /* we open the temporary file in order to create the name, then
       * we close it. It's awful */
      fdfile = mkstemp(name);
      close (fdfile);
#else
#ifdef USE_TMPEXT
      {
	  int a = 0;
	  struct stat sf;
	  while (a<1000000) {
              fl_snprintf(tmpchr,strlen(TMP_PIPE_FILE)+9,
		      "/%s.%06d",TMP_PIPE_FILE,a);
	      if ((stat(name,&sf)<0) && (errno==ENOENT)) break;
	      a++;
	  }
      }
#else
      sprintf(tmpchr,"/%s",TMP_PIPE_FILE);
#endif
#endif
    }

    sigwinchcatch=0;
    Reset_keyboard();
    Screen_suspend();

    signal(SIGTSTP, SIG_DFL); /* on s'en fout */
    signal(SIGWINCH, sigwinch_while_fork);

    switch ((pid=fork())) {
       case -1 : break;
       case 0 : {
                  if (flagin) {
                    close(tcp_fd); close(fd[1]);
                    close(0);
                    if (dup2(fd[0],0)<0) _exit(-1);
                  }
                  if (flagout) {
                    if ((fdfile=open(name,O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR))<0)
                      _exit(-1);
                    if (dup2(fdfile,1)<0) _exit(-1);
                  }
                  if (system(cmdline)<0)
                    _exit(-1);
                  _exit(0);
                }
       default : if (rc==0) free(cmdline);
		 if (flagin) {
                   close(fd[0]);
                   return fd[1];
                 } else return 0;
    }
    if (rc==0) free(cmdline);
    if (flagin) {
      close(fd[0]);
      Pipe_Msg_Stop(fd[1]);
    }
    return -1;
}

/* ferme le pipe et attend */
int Pipe_Msg_Stop(int fd) {
   if (fd>0) close(fd);
   while ((wait(NULL)<0) && (errno==EINTR));
   Screen_resume();
   Init_keyboard(0); /* Ca remet SIGTSTP correct */
   signal(SIGWINCH, sig_winch);
   /* system ne renvoie pas les sigwinch, on va plutôt tester directement */
   /* si la taille de l'écran a changé */
   screen_changed_size(); 
   sigwinch_received=0; /* On n'en a rien a faire... */
   return 0;
}

