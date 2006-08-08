/*
 * Copyright (C) 2006 Lauri Leukkunen <lle@rahina.org>
 */

#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * The sole purpose of this binary is to serve as a static executable
 * entry point to scratchbox, we simply fire up a bash shell and let the 
 * scripts take over.
 */


extern char **environ;

int run_sb2rc(void)
{
	putenv("LD_PRELOAD=/scratchbox/lib/libsb2.so");
	execl("/scratchbox/sarge/lib/ld-linux.so.2",
		"/bin/bash",
		"--library-path",
		"/scratchbox/sarge/lib",
		"/scratchbox/sarge/bin/bash",
		"/scratchbox/sb2rc",
		NULL);
	return -1;
}

int mount_proc(void)
{
	char *proc_path;
	char *tmp;
	char *line = NULL;
	FILE *f = NULL;
	size_t len;
	ssize_t c;

	//printf("in mount_proc()\n");

	if ( (proc_path = realpath("./proc", NULL)) == NULL) {
		fprintf(stderr, "sb2init: unable to mount proc\n");
		return -1;
	}
	tmp = malloc(strlen("proc ") + strlen(proc_path) + strlen(" proc ") + 1);
	strcpy(tmp, "proc ");
	strcat(tmp, proc_path);
	strcat(tmp, " proc ");

	/* first check if it's already mounted */

	if ( (f = fopen("/proc/mounts", "r")) == NULL ) {
		perror("unable to open /proc/mounts");
		return -1;
	}

	while ((c = getline((char **)&line, (size_t *)&len, f)) != -1) {
		line[strlen(line) - 1] = '\0';
		//printf("[%s][%s]\n", line, tmp);
		if (strstr(line, tmp) == line) {
			/* found it */
			fclose(f);
			free(tmp);
			free(line);
			return 1;
		}
	}
	
	fclose(f);
	free(tmp);
	free(line);

	/* wasn't mounted so do it */

	if (mount("proc", proc_path, "proc", MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) < 0) {
		perror("mounting proc failed");
		return -1;
	}

	return 0;


}

int register_binfmt_misc(void)
{
	return 0;
}

int main(int argc, char *argv[])
{
	char *tmp;
	gid_t sudo_gid = 0;
	uid_t sudo_uid = 0;


	/* before chroot'ing or dropping privileges, 
	 * mount the necessary directories and do other
	 * preparatory work
	 */

	if (mount_proc() < 0) {
		fprintf(stderr, "sb2init: mount_proc() failed\n");
		return -1;
	}
	if (register_binfmt_misc() < 0) {
		fprintf(stderr, "sb2init: register_binfmt_misc() failed\n");
		return -1;
	}


	/* now chroot and drop the privileges */

	tmp = getenv("SUDO_GID");
	if (tmp) sudo_gid = atoi(tmp);

	tmp = getenv("SUDO_UID");
	if (tmp) sudo_uid = atoi(tmp);

	tmp = realpath("./", NULL);
	//printf("chrooting to: [%s]\n", tmp);

	if (chroot(tmp) < 0) {
		perror("ERROR");
		fprintf(stderr, "chroot failed\n");
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
	
	run_sb2rc(); /* this should never return */

	perror("running /scratchbox/sb2rc failed");
	return -1;
}
