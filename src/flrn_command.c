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

/* cree une macro
 * maintenant, on recherche une place libre avant d'ajouter a la fin...
 * donc plusieurs bind sur la meme touche ne doivent plus faire augmenter
 * la consommation memoire */
static int do_macro(int cmd, char *arg) {
  int num;
  for (num=0; num < Flcmd_num_macros; num++) {
    if (Flcmd_macro[num].cmd < 0)
      break;
  }
  if (num == Flcmd_num_macros_max) {
    Flcmd_macro=safe_realloc(Flcmd_macro,
	(Flcmd_num_macros_max+4)*sizeof(Flcmd_macro_t));
    Flcmd_num_macros_max += 4;
  }
  if (num == Flcmd_num_macros)
    Flcmd_num_macros++;
  Flcmd_macro[num].cmd = cmd;
  if (arg != NULL) 
    Flcmd_macro[num].arg = safe_strdup(arg);
  else
    Flcmd_macro[num].arg = NULL;
  Flcmd_macro[num].next_cmd = -1;
  return num;
}

static void del_macro(int num) {
  if (Flcmd_macro[num].arg)
    free(Flcmd_macro[num].arg);
  Flcmd_macro[num].cmd = -1;
  return;
}

/* renvoie -1 en cas de pb
 * si add!=0, on ajoute la commande... */
int Bind_command_new(int key, int command, char *arg, int context, int add ) {
  int *to_add=NULL;
  int mac_num;
  if (add) {
    if (Flcmd_rev[context][key] < 0) {
      return -1;
    }
    if ((Flcmd_rev[context][key] & FLCMD_MACRO) == 0) {
      mac_num = do_macro(Flcmd_rev[context][key] ,NULL);
      Flcmd_rev[context][key] = mac_num | FLCMD_MACRO;
      to_add = & Flcmd_macro[mac_num].next_cmd;
    } else {
      to_add = & Flcmd_macro[Flcmd_rev[context][key] ^ FLCMD_MACRO].next_cmd;
    }
  } else {
    to_add = &Flcmd_rev[context][key];
    /* pour liberer la macro */
    if ((Flcmd_rev[context][key]>=0) &&
	(Flcmd_rev[context][key] & FLCMD_MACRO))
      del_macro(Flcmd_rev[context][key] ^ FLCMD_MACRO);
  }
  if (arg != NULL) {
    mac_num = do_macro(command,arg);
    *to_add = mac_num | FLCMD_MACRO;
    return 0;
  } else {
    *to_add = command;
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
