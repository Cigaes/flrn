/* flrn
 * Gestion des bindings de touches
 * Commun a tous les contextes */

#include <stdlib.h>
#include "flrn.h"
#include "options.h"
#include "flrn_macros.h"
#include "flrn_command.h"

/* le tableau touche -> commande */
int Flcmd_rev[NUMBER_OF_CONTEXTS][MAX_FL_KEY];
Flcmd_macro_t *Flcmd_macro=NULL;
int Flcmd_num_macros=0;
static int Flcmd_num_macros_max=0;

/* renvoie -1 en cas de pb */
int Bind_command_new(int key, int command, char *arg, int context ) {
   if (arg != NULL) {
     if (Flcmd_num_macros == Flcmd_num_macros_max) {
        Flcmd_macro=safe_realloc(Flcmd_macro,
	    (Flcmd_num_macros_max+4)*sizeof(Flcmd_macro_t));
	Flcmd_num_macros_max += 4;
     }
     Flcmd_macro[Flcmd_num_macros].cmd = command;
     Flcmd_macro[Flcmd_num_macros].arg = safe_strdup(arg);
     Flcmd_rev[context][key]=Flcmd_num_macros | FLCMD_MACRO;
     Flcmd_num_macros++;
     return 0;
   } else {
     Flcmd_rev[context][key]=command;
     return 0;
   }
}

void free_Macros(void) {
   int i;
   for (i=0;i<Flcmd_num_macros;i++) {
     free(Flcmd_macro[i].arg);
   }
   Flcmd_num_macros=0;
   Flcmd_num_macros_max=0;
   free(Flcmd_macro);
}
