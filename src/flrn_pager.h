/* 			flrn_pager.h				*/

#ifndef FLRN_FLRN_PAGER_H
#define FLRN_FLRN_PAGER_H

#ifdef IN_FLRN_PAGER_C

#define NB_FLCMD_PAGER 5
#define FLCMD_PAGER_UNDEF -1

char *Flcmds_pager[NB_FLCMD_PAGER] = 
   { "up",
#define FLCMD_PAGER_UP 0
     "down",
#define FLCMD_PAGER_DOWN 1
     "pgup",
#define FLCMD_PAGER_PGUP 2
     "pgdown",
#define FLCMD_PAGER_PGDOWN 3
     "quit",
#define FLCMD_PAGER_QUIT 4
};

#define CMD_DEF_PAGER (sizeof(Cmd_Def_Pager)/sizeof(Cmd_Def_Pager[0]))
struct cmd_predef_pager {
  int key;
  int cmd;
} Cmd_Def_Pager[] = {
  { 11, FLCMD_PAGER_UP },
  { 10, FLCMD_PAGER_DOWN },
  { '\r', FLCMD_PAGER_DOWN },
  { 2 , FLCMD_PAGER_PGUP },
  { 6 , FLCMD_PAGER_PGDOWN },
  { ' ', FLCMD_PAGER_PGDOWN },
  { 'q', FLCMD_PAGER_QUIT },
};

#endif /* IN_FLRN_PAGER_C */

/* les fonctions */

extern int Page_message (int /*num_elem*/, int /*short_exit*/, int /*key*/,
        int /*act_row*/, int /*row_deb*/, char * /*exit_chars*/, char *,
	int in_wait (int));
extern int Bind_command_pager(char *, int, char *, int);
extern void init_Flcmd_pager_rev(void);

#endif
