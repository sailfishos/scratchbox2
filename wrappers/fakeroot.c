/* Copyright (c) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* Fakeroot wrapper: Emulate the "fakeroot" command within
 * SB2 session, by using SB2's features.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

static struct option long_fakeroot_opts[] = {
	{"lib", 1, NULL, 0},
	{"faked", 1, NULL, 0},
	{"unknown-is-real", 1, NULL, 'u'},
};

int main(int argc, char *argv[])
{
	int opt = 0;
	int opt_ind = 0;
	const char *progname = argv[0];

	/* default: set owner and group of unknown files to 0 and 0 */
	const char *vperm_request = "u0:0:0:0,g0:0:0:0,f0.0";

	/* alternative: use real owner and group info */
	const char *vperm_request_u_is_r = "u0:0:0:0,g0:0:0:0";

	while (opt != -1) {
		opt = getopt_long(argc, argv, "+l:s:i:ub:hv",
			long_fakeroot_opts, &opt_ind);
		switch (opt) {
		case -1: /* end of options */
			break;
		case '0': /* long option */
			fprintf(stderr, "SB2 %s: Long option '%s', ignored.\n",
				progname, long_fakeroot_opts[opt_ind].name);
			break;
		default: /* short option */
			fprintf(stderr, "SB2 %s: option '%c', ignored.\n",
				progname, opt);
			break;
		case 'u':
			/* unknown-is-real flag: */
			vperm_request = vperm_request_u_is_r;
			break;
		case 'h':
		case 'v':
			fprintf(stderr, "SB2 %s: A wrapper which emulates 'fakeroot'\n",
				progname);
			return(0);
		}
	}
	setenv("PS1", "[SB2-root] \\u@\\h \\W # ", 1);
	setenv("SBOX_VPERM_REQUEST", vperm_request, 1);
	if (argv[optind]) {
		execvp(argv[optind], argv+optind);
	} else {
		const char *shell = getenv("SHELL");

		if (!shell) shell = "/bin/sh"; 
		execl(shell, shell, "--noprofile", "--norc", NULL);
	}
	return(1);
}

