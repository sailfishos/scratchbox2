/* Copied from glibc-2.7 to Scratchbox 2,
 * from glibc's sysdep/gnu/glob64.c.
*/

// Apple doesn't have 64bit dirent?
#ifndef __APPLE__

#include <dirent.h>
#include <glob.h>
#include <sys/stat.h>

#define dirent dirent64
#define __readdir(dirp) __readdir64 (dirp)

#define glob_t glob64_t
#if 0
#define glob(pattern, flags, errfunc, pglob) \
  glob64 (pattern, flags, errfunc, pglob)
#endif
#define globfree(pglob) globfree64 (pglob)

#undef stat
#define stat stat64
#undef __stat
#define __stat(file, buf) __xstat64 (_STAT_VER, file, buf)

#define NO_GLOB_PATTERN_P 1

#define COMPILE_GLOB64  1

#include "glob.c"

#endif

