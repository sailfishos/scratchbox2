/* Error + trace + debug logging routines for SB2.
 *
 * Copyright (C) 2007 Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

/* Two log formats are supported:
 * 1) Normal (default) log entry consists of three or four tab-separated
 *    fields:
 *     - Timestamp and log level
 *     - Process name ("binaryname") and process id (number)
 *     - The log message. If the original message contains tabs and/or
 *       newlines, those will be filtered out.
 *     - Optionally, source file name and line number.
 * 2) Simple format minimizes varying information and makes it easier to 
 *    compare log files from different runs. Fields are:
 *     - Log level
 *     - Process name
 *     - The message
 *     - Optionally, source file name and line number.
 *    Simple format can be enabled by setting environment variable 
 *    "SBOX_MAPPING_LOGFORMAT" to "simple".
 *
 * Note that logfiles are used by at least two other components
 * of sb2: sb2-monitor/sb2-exitreport notices if errors or warnings have
 * been generated during the session, and sb2-logz can be used to generate
 * summaries.
*/

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>

#include <sb2.h>
#include <config.h>

#include "exported.h"
#include "scratchbox2_version.h"

/* ===================== Internal state variables =====================
 *
 * N.B. no mutex protecting concurrent writing to these variables.
 * currently a mutex is not needed, because all of these are
 * written only at startup and values come from environment variables...
*/

static struct sb_log_state_s {
	const char	*sbl_logfile;
	const char	*sbl_binary_name;
	int		sbl_print_file_and_line;
	int		sbl_simple_format;
} sb_log_state = {
	.sbl_logfile = NULL,
	.sbl_binary_name = "UNKNOWN",
	.sbl_print_file_and_line = 0,
	.sbl_simple_format = 0,
};

/* ===================== public variables ===================== */

/* loglevel needs to be public, it is used from the logging macros */
int sb_loglevel__ = SB_LOGLEVEL_uninitialized;

/* ===================== private functions ===================== */

/* create a timestamp in format "YYYY-MM-DD HH:MM:SS.sss", where "sss"
 * is the decimal part of current second (milliseconds).
*/
static void make_log_timestamp(char *buf, size_t bufsize)
{
	struct timeval	now;
	struct tm	tm;

	if (gettimeofday(&now, (struct timezone *)NULL) < 0) {
		*buf = '\0';
		return;
	}

	localtime_r(&now.tv_sec, &tm);
	snprintf(buf, bufsize, "%4d-%02d-%02d %02d:%02d:%02d.%03d",
		tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(now.tv_usec/1000));
}
#define LOG_TIMESTAMP_BUFSIZE 24

/* Write a message block to a logfile.
 *
 * Note that a safer but slower design was selected intentionally:
 * a routine which always opens a file, writes the message and then closes
 * the file is really stupid if only performance is considered. Usually
 * anyone would open the log file only once and keep it open, but here
 * a different strategy was selected because this library should be transparent
 * to the running program and having an extra open fd hanging around might
 * introduce really peculiar problems with some programs.
*/
static void write_to_logfile(const char *msg, int msglen)
{
	int logfd;

	if (sb_log_state.sbl_logfile) {
		if (*sb_log_state.sbl_logfile == '-' &&
		    sb_log_state.sbl_logfile[1] == '\0') {
			/* log to stdout. */
			int r; /* needed to get around some unnecessary warnings from gcc*/
			r = write(1, msg, msglen);
			(void)r;
		} else if ((logfd = open_nomap_nolog(sb_log_state.sbl_logfile,
					O_APPEND | O_RDWR | O_CREAT,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
					| S_IROTH | S_IWOTH)) > 0) {
			int r; /* needed to get around some unnecessary warnings from gcc*/
			r = write(logfd, msg, msglen);
			(void)r;
			close_nomap_nolog(logfd);
		}
	}
}

/* ===================== public functions ===================== */

void sblog_init(void)
{
	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) {
		const char	*level_str;
		const char	*format_str;

		if (!sb2_global_vars_initialized__) {
			sb2_initialize_global_variables();
			if (sb2_global_vars_initialized__) {
				/* All ok. sb2_initialize_global_variables()
				 * called us, no need to continue here.
				*/
				return;
			}
		}

		if (sbox_exec_name &&
		    sbox_orig_binary_name &&
		    strcmp(sbox_exec_name, sbox_orig_binary_name)) {
			/* this is an interpreter, running a script.
			 * set .sbl_binary_name to 
			 * scriptbasename{interpreterbasename}
			*/
			char *full_name = NULL;
			char *cp;

			if ((cp=strrchr(sbox_exec_name, '/')) != NULL) {
				cp++;
			} else {
				cp = sbox_exec_name;
			}
			if (asprintf(&full_name, "%s{%s}", cp, sbox_binary_name) < 0) {
				if (sbox_binary_name)
					sb_log_state.sbl_binary_name = sbox_binary_name;
			} else {
				sb_log_state.sbl_binary_name = full_name;
			}
		} else {
			if (sbox_binary_name)
				sb_log_state.sbl_binary_name = sbox_binary_name;
		}

		sb_log_state.sbl_logfile = getenv("SBOX_MAPPING_LOGFILE");
		level_str = getenv("SBOX_MAPPING_LOGLEVEL");
		if (sb_log_state.sbl_logfile) {
			if (level_str) {
				/* Both logfile and loglevel have been set. */
				if (!strcmp(level_str,"error")) {
					sb_loglevel__ = SB_LOGLEVEL_ERROR;
				} else if (!strcmp(level_str,"warning")) {
					sb_loglevel__ = SB_LOGLEVEL_WARNING;
				} else if (!strcmp(level_str,"net")) {
					sb_loglevel__ = SB_LOGLEVEL_NETWORK;
				} else if (!strcmp(level_str,"notice")) {
					sb_loglevel__ = SB_LOGLEVEL_NOTICE;
				} else if (!strcmp(level_str,"info")) {
					sb_loglevel__ = SB_LOGLEVEL_INFO;
				} else if (!strcmp(level_str,"debug")) {
					sb_loglevel__ = SB_LOGLEVEL_DEBUG;
					sb_log_state.sbl_print_file_and_line = 1;
				} else if (!strcmp(level_str,"noise")) {
					sb_loglevel__ = SB_LOGLEVEL_NOISE;
					sb_log_state.sbl_print_file_and_line = 1;
				} else if (!strcmp(level_str,"noise2")) {
					sb_loglevel__ = SB_LOGLEVEL_NOISE2;
					sb_log_state.sbl_print_file_and_line = 1;
				} else if (!strcmp(level_str,"noise3")) {
					sb_loglevel__ = SB_LOGLEVEL_NOISE3;
					sb_log_state.sbl_print_file_and_line = 1;
				} else {
					sb_loglevel__ = SB_LOGLEVEL_INFO;
				}
			} else {
				/* logfile set, no level specified: */
				sb_loglevel__ = SB_LOGLEVEL_INFO;
			}
		} else {
			/* no logfile, don't log anything. */
			sb_loglevel__ = SB_LOGLEVEL_NONE;
		}

		format_str = getenv("SBOX_MAPPING_LOGFORMAT");
		if (format_str) {
			if (!strcmp(format_str,"simple")) {
				sb_log_state.sbl_simple_format = 1;
			}
		}

		/* initialized, write a mark to logfile. */
		SB_LOG(SB_LOGLEVEL_INFO,
			 "---------- Starting (" SCRATCHBOX2_VERSION ")"
			" [" __DATE__ " " __TIME__ "] "
			"ppid=%d <%s> (%s) ----------",
			getppid(),
			sbox_exec_name ? 
				sbox_exec_name : "",
			sbox_active_exec_policy_name ?
				sbox_active_exec_policy_name : "");
	}
}

/* a vprintf-like routine for logging. This will
 * - prefix the line with current timestamp, log level of the message, and PID
 * - add a newline, if the message does not already end to a newline.
*/
void sblog_vprintf_line_to_logfile(
	const char	*file,
	int		line,
	int		level,
	const char	*format,
	va_list		ap)
{
	char	tstamp[LOG_TIMESTAMP_BUFSIZE];
	char	*logmsg = NULL;
	char	*finalmsg = NULL;
	int	msglen;
	char	*forbidden_chrp;
	char	*optional_src_location;
	char	*levelname = NULL;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) sblog_init();

	if (sb_log_state.sbl_simple_format) {
		*tstamp = '\0';
	} else {
		/* first, the timestamp: */
		make_log_timestamp(tstamp, sizeof(tstamp));
	}

	/* next, print the log message to a buffer: */
	if (vasprintf(&logmsg, format, ap) < 0) {
		/* OOPS. should log an error message, but this is the
		 * logger... can't do it */
		logmsg = NULL;
	}

	/* post-format the log message.
	 *
	 * First, clean all newlines: some people like to use \n chars in
	 * messages, but that is forbidden (attempt to manually reformat
	 * log messages *will* break all post-processing tools).
	 * - remove trailing newline(s)
	 * - replace embedded newlines by $
	 *
	 * Second, replace all tabs by spaces because of similar reasons
	 * as above. We'll use tabs to separate the pre-defined fields below.
	*/
	msglen = strlen(logmsg);
	while ((msglen > 0) && (logmsg[msglen-1] == '\n')) {
		logmsg[msglen--] = '\0';
	}
	while ((forbidden_chrp = strchr(logmsg,'\n')) != NULL) {
		*forbidden_chrp = '$'; /* newlines to $ */
	}
	while ((forbidden_chrp = strchr(logmsg,'\t')) != NULL) {
		*forbidden_chrp = ' '; /* tabs to spaces */
	}

	/* combine the timestamp and log message to another buffer.
	 * here we use tabs to separate fields. Note that the location,
	 * if present, should always be the last field (so that same
	 * post-processing tools can be used in both cases)  */
	if (sb_log_state.sbl_print_file_and_line) {
		if (asprintf(&optional_src_location, "\t[%s:%d]", file, line) < 0) {
			optional_src_location = NULL;
		}
	} else {
		optional_src_location = strdup("");
	}

	switch(level) {
	case SB_LOGLEVEL_ERROR:		levelname = "ERROR"; break;
	case SB_LOGLEVEL_WARNING:	levelname = "WARNING"; break;
	case SB_LOGLEVEL_NETWORK:	levelname = "NET"; break;
	case SB_LOGLEVEL_NOTICE:	levelname = "NOTICE"; break;
	/* default is to pass level info as numbers */
	}
	
	if (sb_log_state.sbl_simple_format) {
		/* simple format. No timestamp or pid, this makes
		 * it easier to compare logfiles.
		*/
		if(levelname) {
			if (asprintf(&finalmsg, "(%s)\t%s\t%s%s\n",
				levelname, sb_log_state.sbl_binary_name, 
				logmsg, optional_src_location) < 0) {
				finalmsg = NULL;
			}
		} else {
			if (asprintf(&finalmsg, "(%d)\t%s\t%s%s\n",
				level, sb_log_state.sbl_binary_name, 
				logmsg, optional_src_location) < 0) {
				finalmsg = NULL;
			}
		}
	} else {
		char	process_and_thread_id[80];

		if (pthread_library_is_available && pthread_self_fnptr) {
			pthread_t	tid = (*pthread_self_fnptr)();

			snprintf(process_and_thread_id, sizeof(process_and_thread_id),
				"[%d/%ld]", getpid(), (long)tid);
		} else {
			snprintf(process_and_thread_id, sizeof(process_and_thread_id),
				"[%d]", getpid());
		}

		/* full format */
		if(levelname) {
			if (asprintf(&finalmsg, "%s (%s)\t%s%s\t%s%s\n",
				tstamp, levelname, sb_log_state.sbl_binary_name, 
				process_and_thread_id, logmsg,
				optional_src_location) < 0) {
				finalmsg = NULL;
			}
		} else {
			if (asprintf(&finalmsg, "%s (%d)\t%s%s\t%s%s\n",
				tstamp, level, sb_log_state.sbl_binary_name, 
				process_and_thread_id, logmsg,
				optional_src_location) < 0) {
				finalmsg = NULL;
			}
		}
	}

	write_to_logfile(finalmsg, strlen(finalmsg));

	free(finalmsg);
	free(logmsg);
	free(optional_src_location);
}

void sblog_printf_line_to_logfile(
	const char	*file,
	int		line,
	int		level,
	const char	*format,
	...)
{
	va_list	ap;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) sblog_init();

	va_start(ap, format);
	sblog_vprintf_line_to_logfile(file, line, level, format, ap);
	va_end(ap);
}
