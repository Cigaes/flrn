#ifndef FLRN_COMMAND_H
#define FLRN_COMMAND_H

#include "flrn_config.h"
#include "flrn_macros.h"

#define NUMBER_OF_CONTEXTS 3

#define CONTEXT_COMMAND 0
#define CONTEXT_PAGER 1
#define CONTEXT_MENU 2

/* tableau des commandes */
extern int Flcmd_rev[NUMBER_OF_CONTEXTS][MAX_FL_KEY];
extern Flcmd_macro_t *Flcmd_macro;
extern int Flcmd_num_macros;

/* les fonctions */

extern int Bind_command_new(int, int, char *, int, int);
extern void free_Macros(void);

#endif
