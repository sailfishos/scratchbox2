/* suid root'able version of 'chroot'.  See 'info chroot'.
 * 
 * 2006-08-06 lle:	Add sudo support, fix indentation
 * 
 * Copyright (C) 2006 Lauri Leukkunen <lle@rahina.org>
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
	char *tmp;
	gid_t sudo_gid = 0;
	uid_t sudo_uid = 0;

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

	tmp = getenv("SUDO_GID");
	if (tmp) sudo_gid = atoi(tmp);

	tmp = getenv("SUDO_UID");
	if (tmp) sudo_uid = atoi(tmp);

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

	/* if sudo_uid > 0 we assume sudo was used, 
	 * otherwise use the setuid logic 
	 */
	if (sudo_uid) {
		if (setgid(sudo_gid)) {
			perror("SUDO ERROR: setgid()");
			return -1;
		}
		if (setuid(sudo_uid)) {
			perror("SUDO ERROR: setuid()");
			return -1;
		}
	} else {
		/* read _real_ user ID  and group and restore them */
		if (setuid(getuid()) < 0) {
			perror("SETUID ERROR: setuid()");
			return -1;
		}
		if (setgid(getgid()) < 0) {
			perror("SETUID ERROR: setgid()");
			return -1;
		}
	}

	//putenv("LD_PRELOAD=/scratchbox/lib/libsb2.so");

	/* run the given program */
	if (execve(prog, args, environ) < 0) {
		perror("ERROR");
		fprintf(stderr, "ERROR: execve(%s, %s..., %s...)\n", prog, *args, *environ);
		return -1;
	}
	return 0;
}
