#ifndef FLRN_MACROS_H
#define FLRN_MACROS_H

/* on partage les macros entre tous les contextes */
#define MAX_FL_MACRO 256

typedef struct {
  int cmd;
  char *arg;
} Flcmd_macro_t;

#endif
