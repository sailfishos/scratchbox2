/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef __SB2_H
#define __SB2_H

#include <syscall.h>
#include <stdio.h>

int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

int do_exec(const char *orig_file, const char *file,
		char *const *argv, char *const *envp);

int ld_so_run_app(char *file, char **argv, char *const *envp);
int run_app(char *file, char **argv, char *const *envp);
int run_cputransparency(char *orig_file, char *file,
		char **argv, char *const *envp);

int run_sbrsh(char *sbrsh_bin, char *target_root, char *orig_file,
              char **argv, char *const *envp);
int run_qemu(char *qemu_bin, char *orig_file, char *file,
		char **argv, char *const *envp);

time_t get_sb2_timestamp(void);

/* ------ debug/trace logging system for sb2: */
#define SB_LOGLEVEL_uninitialized (-1)
#define SB_LOGLEVEL_NONE	0
#define SB_LOGLEVEL_ERROR	1
#define SB_LOGLEVEL_WARNING	2
#define SB_LOGLEVEL_INFO	5
#define SB_LOGLEVEL_DEBUG	8
#define SB_LOGLEVEL_NOISE	9

extern void sblog_init(void);
extern void sblog_vprintf_line_to_logfile(const char *file, int line,
	const char *format, va_list ap);
extern void sblog_printf_line_to_logfile(const char *file, int line,
	const char *format,...);

extern int sb_loglevel__; /* do not access directly */

#define SB_LOG(level, ...) \
	do { \
		if((level) <= sb_loglevel__) { \
			sblog_printf_line_to_logfile( \
				__FILE__, __LINE__, __VA_ARGS__); \
		} \
	} while (0)

#define LIBSB2 "libsb2.so.1"

#endif
