#include <stdio.h>
#include<stdarg.h>

#include "config.h"

#ifndef HAVE_SNPRINTF
int snprintf(char *out, int len, char *fmt, ...) {
   va_list ap;
   int res;
   va_start(ap, fmt);
   res=vsprintf (out, fmt, ap);
   va_end(ap);
   return res;
}
#endif
