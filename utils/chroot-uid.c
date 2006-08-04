/* suid root'able version of 'chroot'.  See 'info chroot'.
 * 
 * This binary has to be suid root as only root can use chroot() call.
 * 1. After that has been done, the real user ID and GID have to be
 *    restored so that user can't run anything with root priviledges.
 * 2. Then the program user gave, will be run as chrooted.
 * 
 * Copyright (c) 2002 Eero Tamminen <eero.tamminen@creanor.com>
 *
 * chroot-uid is licensed under GPL.
 * You can read the full license at http://www.gnu.org/licenses/gpl.txt
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

extern char **environ;

int main(int argc, char *argv[])
{
  char *path, *prog, **args;

  /* check argument validity */
  if (argc < 3 || argv[1][0] != '/') {
    fprintf(stderr, "usage: %s   /absolute-path  path/program\n", *argv);
    fprintf(stderr, "  See 'man chroot'.  This is a version that can be run suid root\n");
    fprintf(stderr, "  as it restores real user ID before running the program.\n");
    fprintf(stderr, "  Path to the program should be relative to first path!\n");
    return -1;
  }
  path = argv[1];
  prog = argv[2];
  args = &(argv[2]);
  
  /* change to given directory and chroot to it */
  if (chdir(path) < 0) {
    perror("ERROR");
    fprintf(stderr, "chdir(%s)\n", path);
    return -1;
  }
  if (chroot(path) < 0) {
    perror("ERROR");
    fprintf(stderr, "chroot(%s)\n", path);
    return -1;
  }
  
  /* read _real_ user ID  and group and restore them */
  if (setuid(getuid()) < 0) {
    perror("ERROR");
    fprintf(stderr, "ERROR: setuid()\n");
    return -1;
  }
  if (setgid(getgid()) < 0) {
    perror("ERROR");
    fprintf(stderr, "ERROR: setgid()\n");
    return -1;
  }

  putenv("LD_PRELOAD=/scratchbox/fakechroot/libsb2.so");

  /* run the given program */
  if (execve(prog, args, environ) < 0) {
    perror("ERROR");
    fprintf(stderr, "ERROR: execve(%s, %s..., %s...)\n", prog, *args, *environ);
    return -1;
  }
  return 0;
}
