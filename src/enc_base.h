/* flrn : lecteur de news en mode texte
 * Copyright (C) 1998-1999  Damien Massé et Joël-Yann Fourré
 *
 *      enc_base.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation. See the file COPYING for details.
 */

#ifndef FLRN_ENC_BASE_H
#define FLRN_ENC_BASE_H

#include <stdlib.h>

/* définition du caractère interne du programme */
#if 0
typedef wchar_t flrn_ch; 
#else
typedef char flrn_char;
#endif

/* définition spécifique wide - not wide : les chaînes internes sont
 *  * toujours en multi-bytes */
typedef wchar_t wflrn_char;
typedef char nflrn_char;


/* manipulation des chaînes */
#if 0
#define fl_static(X)	L ## X
#define fl_strcmp(X,Y)	wcscmp((X),(Y))
#define fl_strncmp(X,Y,Z)	wcsncmp((X),(Y),(Z))
#define fl_strrchr(X,Y)	wcsrchr((X),(Y))
#define fl_strchr(X,Y)	wcschr((X),(Y))
#define fl_strcpy(X,Y)	wcscpy((X),(Y))
#define fl_strcat(X,Y)	wcscat((X),(Y))
#define fl_strlen(X)	wcslen((X))
static inline flraw_strcpy(char *x, flrn_char *y) {
    while (*y) { *(x++) = (char) *(y++); }
    *x = '\0';
}
static inline flrraw_strcpy(flrn_char *x, char *y) {
    while (*y) { *(x++) = (flrn_char) *(y++); }
    *x = L'\0';
}
#define fl_strncpy(X,Y,N)	wcsncpy((X),(Y),(N))
#define fl_strncat(X,Y,N)	wcsncat((X),(Y),(N))
static inline flraw_strncpy(char *x, flrn_char *y, size_t n) {
    while ((*y) && (n>0)) { *(x++) = (char) *(y++); n--; }
    if (n>0) *x = '\0';
}
static inline flrraw_strncpy(flrn_char *x, char *y, size_t n) {
    while ((*y) && (n>0)) { *(x++) = (flrn_char) *(y++); n--; }
    if (n>0) *x = L'\0';
}
/* cette fonction est une extension GNU -> changer ça */
#define fl_strcasecmp(X,Y)	wcscasecmp((X),(Y))
#define fl_strncasecmp(X,Y,N)	wcsncasecmp((X),(Y))
static inline flraw_strncasecmp(char *x, flrn_char *y, size_t n) {
    char *z=safe_malloc(wcslen(y)+1); /* ca buggue ici, mais le principe */
    int res;
    flraw_strcpy(z,y);
    res=strncasecmp(x,z,n); free(z); return res;
}
#define empty_flstr	L""
#define fl_atoi(X)	(int) wcstol((X),NULL,10)
#define fl_strstr(X,Y)	wcsstr((X),(Y))
#define fl_isalnum(X)	iswalnum((X))
#define fl_isdigit(X)	iswdigit((X))
#define fl_isblank(X)	iswblank((X))
#define fl_isspace(X)	iswspace((X))
#define fl_isalpha(X)	iswalpha((X))
#define fl_strtok_r(X,Y,Z)	wcstok((X),(Y),(Z))
#define fl_strpbrk(X,Y)	wcspbrk((X),(Y))
#define fl_strtol(X,Y,Z)	wcstol(X,Y,Z)
extern flrn_char *fl_static_tran(const char *);
extern char *fl_static_rev(const flrn_char *);
extern flrn_char *fl_dynamic_tran(const char *, int *);
extern char *fl_dynamic_rev(const flrn_char *, int *);
#define fl_strspn(X,Y)	wcsspn((X),(Y))
#define fl_strcspn(X,Y)	wcscspn((X),(Y))
#define fl_snprintf(...)	swprintf(__VA_ARGS__)
#define fl_memset(X,Y,Z)	wmemset((X),(Y),(Z))
#define fl_strcspn(X,Y)		wcscspn((X),(Y))
#define fl_toupper(X)		towupper((X))
#else
#define fl_static(X)	X
#define fl_strcmp(X,Y)	strcmp((X),(Y))
#define fl_strncmp(X,Y,Z)	strncmp((X),(Y),(Z))
#define fl_strrchr(X,Y)	strrchr((X),(Y))
#define fl_strchr(X,Y)	strchr((X),(Y))
#define fl_strcpy(X,Y)	strcpy((X),(Y))
#define fl_strcat(X,Y)	strcat((X),(Y))
#define fl_strlen(X)	strlen((X))
#define flraw_strcpy(X,Y)	strcpy((X),(Y))
#define flrraw_strcpy(X,Y)	strcpy((X),(Y))
#define fl_strncpy(X,Y,N)	strncpy((X),(Y),(N))
#define fl_strncat(X,Y,N)	strncat((X),(Y),(N))
#define flraw_strncpy(X,Y,N)	strncpy((X),(Y),(N))
#define flrraw_strncpy(X,Y,N)	strncpy((X),(Y),(N))
#define fl_strcasecmp(X,Y)	strcasecmp((X),(Y))
#define fl_strncasecmp(X,Y,N)	strncasecmp((X),(Y),(N))
#define flraw_strncasecmp(X,Y,N)	strncasecmp((X),(Y),(N))
#define empty_flstr	""
#define fl_atoi(X)	(int) atoi((X))
#define fl_regcomp(X,Y,Z)	regcomp((X),(Y),(Z))
#define fl_regexec(X,Y,Z,T,U)	regexec((X),(Y),(Z),(T),(U))
#define fl_regfree(X)	regfree((X))
#define fl_strstr(X,Y)	strstr((X),(Y))
#define fl_isalnum(X)	isalnum((int)(X))
#define fl_isdigit(X)	isdigit((int)(X))
#define fl_isblank(X)	isblank((int)(X))
#define fl_isspace(X)	isspace((int)(X))
#define fl_isalpha(X)	isalpha((int)(X))
#define fl_strtok_r(X,Y,Z)	strtok_r((X),(Y),(Z))
#define fl_strpbrk(X,Y)	strpbrk((X),(Y))
#define fl_strtol(X,Y,Z)	strtol(X,Y,Z)
#define fl_static_tran(X)	(X)
#define fl_static_rev(X)	(X)
#define fl_dynamic_tran(X,Y)	(X)
#define fl_dynamic_rev(X,Y)	(X)
#define fl_strspn(X,Y)	strspn((X),(Y))
#define fl_strcspn(X,Y)	strcspn((X),(Y))
#define fl_snprintf(...)	snprintf(__VA_ARGS__)
#define fl_memset(X,Y,Z)	memset((X),(Y),(Z))
#define fl_strcspn(X,Y)		strcspn((X),(Y))
#define fl_toupper(X)		toupper(X)
#endif

#endif
