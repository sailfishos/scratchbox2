#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif
#ifdef HAVE__NSGETENVIRON
#define environ (*_NSGetEnviron())
#else
 #ifndef __GLIBC__
  extern char **environ;
 #endif
#endif
