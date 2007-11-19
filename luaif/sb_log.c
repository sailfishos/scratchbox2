/* Error + trace + debug logging routines for SB2.
 *
 * Copyright (C) 2007 Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <sb2.h>

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
} sb_log_state = {
	.sbl_logfile = NULL,
	.sbl_binary_name = "UNKNOWN",
	.sbl_print_file_and_line = 0,
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
		if ((logfd = open_nomap_nolog(sb_log_state.sbl_logfile,
					O_APPEND | O_RDWR | O_CREAT,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
					| S_IROTH | S_IWOTH)) > 0) {
			write(logfd, msg, msglen);
			close(logfd);
		}
	}
}

/* ===================== public functions ===================== */

void sblog_init(void)
{
	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) {
		const char	*level_str;
		const char	*bin;

		bin = getenv("__SB2_BINARYNAME");
		if (bin) sb_log_state.sbl_binary_name = bin;

		sb_log_state.sbl_logfile = getenv("SBOX_MAPPING_LOGFILE");
		level_str = getenv("SBOX_MAPPING_LOGLEVEL");
		if (sb_log_state.sbl_logfile) {
			if (level_str) {
				/* Both logfile and loglevel have been set. */
				switch(*level_str) {
				case 'e': case 'E':
					sb_loglevel__ = SB_LOGLEVEL_ERROR;
					break;
				case 'w': case 'W':
					sb_loglevel__ = SB_LOGLEVEL_WARNING;
					break;
				case 'i': case 'I':
					sb_loglevel__ = SB_LOGLEVEL_INFO;
					break;
				case 'd': case 'D':
					sb_loglevel__ = SB_LOGLEVEL_DEBUG;
					sb_log_state.sbl_print_file_and_line = 1;
					break;
				case 'n': case 'N':
					sb_loglevel__ = SB_LOGLEVEL_NOISE;
					sb_log_state.sbl_print_file_and_line = 1;
					break;

				default:
					sb_loglevel__ = SB_LOGLEVEL_INFO;
					break;
				}
			} else {
				/* logfile set, no level specified: */
				sb_loglevel__ = SB_LOGLEVEL_INFO;
			}
		} else {
			/* no logfile, don't log anything. */
			sb_loglevel__ = SB_LOGLEVEL_NONE;
		}
		/* initialized, write a mark to logfile. */
		SB_LOG(SB_LOGLEVEL_INFO, "---------- Starting ----------");
	}
}

/* a vprintf-like routine for logging. This will
 * - prefix the line with current timestamp and PID
 * - add a newline, if the message does not already end to a newline.
*/
void sblog_vprintf_line_to_logfile(
	const char	*file,
	int		line,
	const char	*format,
	va_list		ap)
{
	char	tstamp[LOG_TIMESTAMP_BUFSIZE];
	char	*logmsg = NULL;
	char	*finalmsg = NULL;
	int	msglen;
	char	*forbidden_chrp;
	char	*optional_src_location;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) sblog_init();

	/* first, the timestamp: */
	make_log_timestamp(tstamp, sizeof(tstamp));

	/* next, print the log message to a buffer: */
	vasprintf(&logmsg, format, ap);

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
		asprintf(&optional_src_location, "\t[%s:%d]", file, line);
	} else {
		optional_src_location = strdup("");
	}
	asprintf(&finalmsg, "%s\t%s[%d]\t%s%s\n",
		tstamp, sb_log_state.sbl_binary_name, getpid(),
		logmsg, optional_src_location);

	write_to_logfile(finalmsg, strlen(finalmsg));

	free(finalmsg);
	free(logmsg);
	free(optional_src_location);
}

void sblog_printf_line_to_logfile(
	const char	*file,
	int		line,
	const char	*format,
	...)
{
	va_list	ap;

	if (sb_loglevel__ == SB_LOGLEVEL_uninitialized) sblog_init();

	va_start(ap, format);
	sblog_vprintf_line_to_logfile(file, line, format, ap);
	va_end(ap);
}
