#ifndef FLRN_COLOR_H
#define FLRN_COLOR_H

#ifdef DEF_FILD_NAMES
static char *Field_names[] ={
  "normal",
  "header",
  "status",
  "error",
  "quoted",
  "fin",
  "sig",
  "file",
  "summary",
};
#endif

#define FIELD_NORMAL  0
#define FIELD_HEADER  1
#define FIELD_STATUS  2
#define FIELD_ERROR   3
#define FIELD_QUOTED  4
#define FIELD_AFF_FIN 5
#define FIELD_SIG     6
#define FIELD_FILE    7
#define FIELD_SUMMARY 8

#define NROF_FIELDS   9

#endif
