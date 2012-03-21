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

/* -------- Config: buffer sizes: */

#define LOG_TIMESTAMP_BUFSIZE 24

/* max.len. of the log message, including \0 but not including line headers */
#define LOG_MSG_MAXLEN	500

/* max.len. of source file name + line number field */
#define LOG_SRCLOCATION_MAXLEN	150

#define LOG_LEVELNAME_MAXLEN 10

#define LOG_BINARYNAME_MAXLEN 80

#define LOG_PIDANDTID_MAXLEN 80

/* max. size of one line:
 * timestamp (levelname)\tbinaryname,process_and_thread_id\tlogmsg,srclocation */
#define LOG_LINE_BUFSIZE (LOG_TIMESTAMP_BUFSIZE+2+LOG_LEVELNAME_MAXLEN+2+ \
			  LOG_BINARYNAME_MAXLEN+LOG_PIDANDTID_MAXLEN+1+ \
			  LOG_MSG_MAXLEN+LOG_SRCLOCATION_MAXLEN)

#define LOGFILE_NAME_BUFSIZE 512

/* ===================== Internal state variables =====================
 *
 * N.B. no mutex protecting concurrent writing to these variables.
 * currently a mutex is not needed, because all of these are
 * written only at startup and values come from environment variables...
*/

static struct sb_log_state_s {
	int		sbl_print_file_and_line;
	int		sbl_simple_format;
	char		sbl_binary_name[LOG_BINARYNAME_MAXLEN];
	char		sbl_logfile[LOGFILE_NAME_BUFSIZE];
} sb_log_state = {
	.sbl_print_file_and_line = 0,
	.sbl_simple_format = 0,
	.sbl_binary_name = {0},
	.sbl_logfile = {0},
};

/* ===================== public variables ===================== */

/* loglevel needs to be public, it is used from the logging macros */
int sb_loglevel__ = SB_LOGLEVEL_uninitialized;

int sb_log_initial_pid__ = 0;

/* ===================== private functions ===================== */

/* create a timestamp in format "YYYY-MM-DD HH:MM:SS.sss", where "sss"
 * is the decimal part of current second (milliseconds).
*/
static void make_log_timestamp(char *buf, size_t bufsize)
{
	struct timeval	now;

	if (gettimeofday(&now, (struct timezone *)NULL) < 0) {
		*buf = '\0';
		return;
	}
	/* localtime_r() or gmtime_r() can cause deadlocks
	 * inside glibc (if this logger is called via a signal
	 * handler), so can't convert the time to a more
	 * user-friedly format. Sad. */
	snprintf(buf, bufsize, "%u.%03u",
		(unsigned int)now.tv_sec, (unsigned int)(now.tv_usec/1000));
}

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

	if (sb_log_state.sbl_logfile[0]) {
		if (sb_log_state.sbl_logfile[0] == '-' &&
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

int sblog_level_name_to_number(const char *level_str)
{
	int level;

	/* Both logfile and loglevel have been set. */
	if (!strcmp(level_str,"error")) {
		level = SB_LOGLEVEL_ERROR;
	} else if (!strcmp(level_str,"warning")) {
		level = SB_LOGLEVEL_WARNING;
	} else if (!strcmp(level_str,"net")) {
		level = SB_LOGLEVEL_NETWORK;
	} else if (!strcmp(level_str,"notice")) {
		level = SB_LOGLEVEL_NOTICE;
	} else if (!strcmp(level_str,"info")) {
		level = SB_LOGLEVEL_INFO;
	} else if (!strcmp(level_str,"debug")) {
		level = SB_LOGLEVEL_DEBUG;
	} else if (!strcmp(level_str,"noise")) {
		level = SB_LOGLEVEL_NOISE;
	} else if (!strcmp(level_str,"noise2")) {
		level = SB_LOGLEVEL_NOISE2;
	} else if (!strcmp(level_str,"noise3")) {
		level = SB_LOGLEVEL_NOISE3;
	} else {
		level = SB_LOGLEVEL_INFO;
	}
	return(level);
}

void sblog_init_level_logfile_format(const char *opt_level, const char *opt_logfile, const char *opt_format)
{
	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) {
		const char	*level_str;
		const char	*format_str;
		const char	*filename;

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
			char *cp;

			if ((cp=strrchr(sbox_exec_name, '/')) != NULL) {
				cp++;
			} else {
				cp = sbox_exec_name;
			}
			snprintf(sb_log_state.sbl_binary_name, sizeof(sb_log_state.sbl_binary_name),
				"%s{%s}", cp, sbox_binary_name);
		} else {
			snprintf(sb_log_state.sbl_binary_name, sizeof(sb_log_state.sbl_binary_name),
				"%s", sbox_binary_name ? sbox_binary_name : "");
		}

		filename = (opt_logfile ? opt_logfile : getenv("SBOX_MAPPING_LOGFILE"));
		if (filename)
			snprintf(sb_log_state.sbl_logfile, sizeof(sb_log_state.sbl_logfile),
				"%s", (opt_logfile ? opt_logfile : getenv("SBOX_MAPPING_LOGFILE")));
		else
			sb_log_state.sbl_logfile[0] = '\0';

		level_str = opt_level ? opt_level : getenv("SBOX_MAPPING_LOGLEVEL");
		if (sb_log_state.sbl_logfile[0]) {
			if (level_str) {
				/* Both logfile and loglevel have been set. */
				sb_loglevel__ = sblog_level_name_to_number(level_str);
				if (sb_loglevel__ >= SB_LOGLEVEL_DEBUG)
					sb_log_state.sbl_print_file_and_line = 1;
			} else {
				/* logfile set, no level specified: */
				sb_loglevel__ = SB_LOGLEVEL_INFO;
			}
		} else {
			/* no logfile, don't log anything. */
			sb_loglevel__ = SB_LOGLEVEL_NONE;
		}

		format_str = opt_format ? opt_format : getenv("SBOX_MAPPING_LOGFORMAT");
		if (format_str) {
			if (!strcmp(format_str,"simple")) {
				sb_log_state.sbl_simple_format = 1;
			}
		}

		/* initialized, write a mark to logfile. */
		/* NOTE: Following SB_LOG() call is used by the log
		 *       postprocessor script "sb2logz". Do not change
		 *       without making a corresponding change to the script!
		*/
		SB_LOG(SB_LOGLEVEL_INFO,
			 "---------- Starting (" SCRATCHBOX2_VERSION ")"
			" [] "
			"ppid=%d <%s> (%s) ----------%s",
			getppid(),
			sbox_exec_name ? 
				sbox_exec_name : "",
			sbox_active_exec_policy_name ?
				sbox_active_exec_policy_name : "",
			sbox_mapping_method ?
				sbox_mapping_method : "");

		sb_log_initial_pid__ = getpid();
	}
}

void sblog_init(void)
{
	sblog_init_level_logfile_format(NULL,NULL,NULL);
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
	char	logmsg[LOG_MSG_MAXLEN];
	char	finalmsg[LOG_LINE_BUFSIZE];
	int	msglen;
	char	*forbidden_chrp;
	char	optional_src_location[LOG_SRCLOCATION_MAXLEN];
	char	*levelname = NULL;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) sblog_init();

	if (sb_log_state.sbl_simple_format) {
		*tstamp = '\0';
	} else {
		/* first, the timestamp: */
		if (level > SB_LOGLEVEL_WARNING) {
			/* no timestamps to errors & warnings */
			make_log_timestamp(tstamp, sizeof(tstamp));
		} else {
			*tstamp = '\0';
		}
	}

	/* next, print the log message to a buffer: */
	msglen = vsnprintf(logmsg, sizeof(logmsg), format, ap);

	if (msglen < 0) {
		/* OOPS. should log an error message, but this is the
		 * logger... can't do it */
		logmsg[0] = '\0';
	} else if (msglen > (int)sizeof(logmsg)) {
		/* message was truncated. logmsg[LOG_MSG_MAXLEN-1] is '\0' */
		logmsg[LOG_MSG_MAXLEN-3] = logmsg[LOG_MSG_MAXLEN-2] = '.';
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
		snprintf(optional_src_location, sizeof(optional_src_location), "\t[%s:%d]", file, line);
	} else {
		optional_src_location[0] = '\0';
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
			snprintf(finalmsg, sizeof(finalmsg), "(%s)\t%s\t%s%s\n",
				levelname, sb_log_state.sbl_binary_name, 
				logmsg, optional_src_location);
		} else {
			snprintf(finalmsg, sizeof(finalmsg), "(%d)\t%s\t%s%s\n",
				level, sb_log_state.sbl_binary_name, 
				logmsg, optional_src_location);
		}
	} else {
		char	process_and_thread_id[LOG_PIDANDTID_MAXLEN];

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
			snprintf(finalmsg, sizeof(finalmsg), "%s (%s)\t%s%s\t%s%s\n",
				tstamp, levelname, sb_log_state.sbl_binary_name, 
				process_and_thread_id, logmsg,
				optional_src_location);
		} else {
			snprintf(finalmsg, sizeof(finalmsg), "%s (%d)\t%s%s\t%s%s\n",
				tstamp, level, sb_log_state.sbl_binary_name, 
				process_and_thread_id, logmsg,
				optional_src_location);
		}
	}

	write_to_logfile(finalmsg, strlen(finalmsg));
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
