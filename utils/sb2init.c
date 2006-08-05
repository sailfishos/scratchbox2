/*
 * Copyright (C) 2006 Lauri Leukkunen <lle@rahina.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * The sole purpose of this binary is to serve as a static executable
 * entry point to scratchbox, we simply fire up a bash shell and let the 
 * scripts take over.
 */

extern char **environ;

int main(int argc, char **argv)
{
	putenv("LD_PRELOAD=/scratchbox/lib/libsb2.so");
	execl("/scratchbox/sarge/lib/ld-linux.so.2",
		"/bin/bash",
		"--library-path",
		"/scratchbox/sarge/lib",
		"/scratchbox/sarge/bin/bash",
		"/scratchbox/sb2rc",
		NULL);
	return 0;
}
