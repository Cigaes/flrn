/* flrn v 0.1                                                           */
/*             tty_keyboard.c         27/11/97                          */
/*                                                                      */
/*  Programme de gestion du clavier.					*/
/*  Essentiellement de betes appels des fontions de la lib.             */
/*                                                                      */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "flrn.h"
#include "flrn_slang.h"
#include "tty_display.h"
#include "tty_keyboard.h"

/* Gestion élémentaire des signaux */
/* Je me base sur ce que fait slrn, parce que je comprend pas grand chose */
void sigtstp_handler(int sig) {
    sigset_t mask;
  
    sigemptyset (&mask);
    sigaddset (&mask, SIGTSTP);
/*    SLsig_block_signals ();  */
    Reset_keyboard();
    Screen_suspend();
/*    SLsig_unblock_signals ();  */

    /*SL*/signal(SIGTSTP, SIG_DFL);
    kill (getpid (), SIGTSTP);
    sigprocmask (SIG_UNBLOCK, &mask, NULL);
    
    Screen_resume();
    Put_tty_single_input(-1,0,1);
    /*SL*/signal(SIGTSTP, sigtstp_handler);
    React_suspend_char(1);
    return;
}

void sigterm_handler(int sig) {
   Reset_screen();
   Reset_keyboard();
   fprintf(stderr, "Killed by SIGTERM\n");
   exit (sig);
}

static void init_signals (void) {
#ifdef SIGTSTP
    /*SL*/signal (SIGTSTP, sigtstp_handler);
#endif
#ifdef SIGTERM
    /*SL*/signal (SIGTERM, sigterm_handler);
#endif
}


/* Initialisation du clavier. On suppose que SLtt_get_terminfo a déjà 	*/
/* été lancé (appel à Init_screen).					*/
int Init_keyboard() {
   int res;

/*   SLsig_block_signals (); */

   res=init_clavier();
   if (res<0) {
      fprintf(stderr, "Echec dans le lancement des routines clavier\n");
/*      SLsig_unblock_signals ();  */
      return -1;
   }
   
   init_signals ();  /* Signaux (de base) */

   res=Put_tty_single_input (-1, 0, 1);
   if (res<0) { 
      fprintf(stderr, "Echec dans l'initialisation du clavier...\n");
/*      SLsig_unblock_signals ();  */
      return -1;
   }
   React_suspend_char(1);  /* Cas du ^Z */

/*   SLsig_unblock_signals ();  */
   Set_Abort_Signal(NULL);
   Set_Ignore_User_Abort(1);
   return 0;
} 


/* Attente d'une touche clavier... Renvoie le code de la touche.	*/
/* Cette fontion remet a zéro la variable SLKeyBoard_Quit.		*/
/* On fait un refresh automatique si key=^L, et on reprend attend-touche*/
int Attend_touche() {
   int key;

   if (error_fin_displayed==0)
      Screen_refresh(); /* Quasiment le seul endroit où on le met */
   error_fin_displayed=0; /* en particulier, ^L élimine le message d'erreur */
   KeyBoard_Quit=0;
   /* ^L n'est pas une commande autre que refresh */
   while((key=Keyboard_getkey())==12) {
      Screen_touch_lines (0, Screen_Rows);
      Screen_refresh();
   }
   if (key==8) key=FL_KEY_BACKSPACE;
   if (key=='\177') key=FL_KEY_DELETE;
   if (key==FL_KEY_DELETE) key=FL_KEY_BACKSPACE;
   return key;
}

/* Prend une ligne. Renvoie -1 si ^C, -2 avec backspace en debut de ligne */
/* 0 sinon.								*/
/* buf DOIT etre initialise en le debut de la ligne...			*/
/* renvoie qqchose >0 si on a appuye sur une des touches magiques et flag=0 */
/* le code de la touche si ce n'est pas une touche magique et flag=1 */
int magic_getline(char *buf, int buffsize, int row, int col, char *magic, int flag, int key_pressed)
{
   int place;
   int key=0;
   char *ptr;

   place=strlen(buf);
   col+=place;
   do
   {
     if (key_pressed) {
       key=key_pressed;
       key_pressed=0;
     } else key=Attend_touche();
     /* if (key=='\r') key='\n'; */
     if (KeyBoard_Quit) return -1; /* on n'a rien fait... */
     if (key==FL_KEY_BACKSPACE) {
        if (place==0) return -2;
        col--; place--;
        Cursor_gotorc(row,col);
        Screen_erase_eol();
        continue;
      }
     buf[place++]=key; col++;
     if (!flag) {
        if ((ptr=index(magic,key))) {
          buf[--place]='\0';           /* He oui... */
          return 1+ptr-magic;
        }
     } else if ((ptr=index(magic,key))==0) {
          buf[--place]='\0';	       /* l'ai oublie ! (NdDamien) */
          return key;
     }
     if (key!='\r') Screen_write_char(key);
   } while ((key != '\r')&&(place < buffsize-1));
   buf[--place]='\0';           /* He oui... */
   return 0;
}

int getline(char *buf, int buffsize, int row, int col)
{
  return magic_getline(buf,buffsize,row,col,"",0,0);
}

