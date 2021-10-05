/* signaltester, catch & print signals
 * This is used for testing out sb2-monitor.
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * License: LGPL-2.1
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define _GNU_SOURCE /* to get strsignal(), a non-standard extension */
#include <string.h>

static int signals_received = 0;

static void signal_handler(int sig, siginfo_t *info, void *ptr)
{
	(void)ptr; /* unused param */

	signals_received++;
	
	fprintf(stderr, "\nGot signal %d (%s), received %d signals\n",
		sig, strsignal(sig), signals_received);

	switch (sig) {
	case SIGILL: case SIGFPE: case SIGSEGV:
		if ((info->si_code != SI_USER) &&
		   (info->si_code != SI_QUEUE)) {
			/* the signal is a real, fatal, deadly 
			 * that must not be ignored. */
			fprintf(stderr, "Deadly signal\n");
			exit(249);
		}
		/* else it is user-generated signal */
		break;

	case SIGTSTP:
		/* Job control.. stop request from tty */
		fprintf(stderr, "Stopping myself.\n");
		raise(SIGSTOP);
		return;
	}

	if (info->si_code == SI_QUEUE) {
		fprintf(stderr, "SI_QUEUE int=%d ptr=0x%x\n",
			info->si_value.sival_int,
			(int)(info->si_value.sival_ptr));
	} else if (info->si_code == SI_USER) {
		fprintf(stderr, "SI_USER\n");
	} else {
		fprintf(stderr, "si_code is %d\n", info->si_code);
	}
}

static void initialize_signal_handler(int sig)
{
	struct	sigaction	act;

	memset(&act, 0, sizeof(act));

	act.sa_sigaction = signal_handler;
	act.sa_flags = SA_SIGINFO;

	/* block all other signals while this signal is being processed */
	sigfillset(&act.sa_mask);

	sigaction(sig, &act, NULL);
}

/* Set up a signal handler for all signals that can be caught. */
static void catch_all_signals(void)
{
	/* order here tries to follows the numerical order of signals 
	 * on Linux (see signal(7)) and Mac OS X (see signal(2))
	*/
	initialize_signal_handler(SIGHUP); /* 1 */
	initialize_signal_handler(SIGINT);
	initialize_signal_handler(SIGQUIT);
	initialize_signal_handler(SIGILL);
	initialize_signal_handler(SIGTRAP); /* 5 */
	initialize_signal_handler(SIGABRT);
#ifdef SIGIOT /* old name for SIGABRT */
	initialize_signal_handler(SIGIOT);
#endif
#ifdef SIGEMT
	initialize_signal_handler(SIGEMT);
#endif
	initialize_signal_handler(SIGFPE);

	/* SIGKILL, 9, cannot be caught or ignored */

	initialize_signal_handler(SIGBUS);
	initialize_signal_handler(SIGSEGV); /* 11 */
	initialize_signal_handler(SIGSYS);
	initialize_signal_handler(SIGPIPE);
	initialize_signal_handler(SIGALRM);
	initialize_signal_handler(SIGTERM); /* 15 */
	initialize_signal_handler(SIGURG);

	/* SIGSTOP, 17, cannot be caught or ignored */

	initialize_signal_handler(SIGTSTP);
	initialize_signal_handler(SIGCONT);
	initialize_signal_handler(SIGCHLD); /* 20 */
	initialize_signal_handler(SIGTTIN);

	initialize_signal_handler(SIGTTOU);
	initialize_signal_handler(SIGIO);
	initialize_signal_handler(SIGXCPU);
	initialize_signal_handler(SIGXFSZ); /* 25 */
	initialize_signal_handler(SIGVTALRM);
	initialize_signal_handler(SIGPROF);
	initialize_signal_handler(SIGWINCH);
#ifdef SIGPWR
	initialize_signal_handler(SIGPWR);
#endif
#ifdef SIGLOST
	initialize_signal_handler(SIGLOST);
#endif
#ifdef SIGINFO
	initialize_signal_handler(SIGINFO);
#endif
	initialize_signal_handler(SIGUSR1);
	initialize_signal_handler(SIGUSR2);
#ifdef SIGTHR
	initialize_signal_handler(SIGTHR);
#endif
}

int main(int argc, char *argv[])
{
	char	*progname = argv[0];
	int	num_signals_to_receive = 10;
	int	opt;
	
	while ((opt = getopt(argc, argv, "n:g")) != -1) {
		switch (opt) {
		case 'n': num_signals_to_receive = atoi(optarg); break;
		case 'g': setpgrp(); break;
		}
	}

	/* check parameters */
	if (optind < argc) {
		fprintf(stderr, "Wrong number of parameters");
		exit(1);
	}

	fprintf(stderr, "\n%s: PID=%d PGID=%d: waiting for %d signals\n", 
		progname, (int)getpid(), (int)getpgrp(),
		num_signals_to_receive);

	catch_all_signals();

	/* all initializations done, wait until the child has finished. */
	while (signals_received < num_signals_to_receive) {
		if (sleep(5)) fprintf(stderr, "S");
		else fprintf(stderr, ".");
	}

	return(0);
}

