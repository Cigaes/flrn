/*   getdate.h  */

#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/times.h>

extern time_t get_date (const char *, const time_t *);
