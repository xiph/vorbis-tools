#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <stdio.h>

#ifdef _WIN32

char *rindex(char *s, int c);
void setbinmode(FILE *);

#else /* Unix, mostly */

#define setbinmode(x) 

#endif

#endif /* __PLATFORM_H */

