#ifndef FLRN_RFC_2047_H
#define FLRN_RFC_2047_H

#include <stdio.h>

#define SHORT_STRING 128
#define ENCQUOTEDPRINTABLE 1
#define ENCBASE64 2

/* les fonctions */

extern void rfc2047_encode_string (char *, unsigned char *, size_t);
extern void rfc2047_decode (char *, const char *, size_t);

#endif
