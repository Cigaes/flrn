#ifndef FLRN_MACROS_H
#define FLRN_MACROS_H

#define MAX_FL_MACRO 256
#define MAX_FL_MACRO_MENU 32 /* Pas exagerer non plus */
#define MAX_FL_MACRO_PAGER 32 /* Pas exagerer non plus */

typedef struct {
  int cmd;
  char *arg;
} Flcmd_macro_t;

#endif
