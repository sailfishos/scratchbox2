/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef __SB2_H
#define __SB2_H

#include <syscall.h>
#include <stdio.h>

#define DBGOUT(fmt...) fprintf(stderr, fmt)

int sb_next_execve(const char *filename, char *const argv [],
			char *const envp[]);

int do_exec(const char *orig_file, const char *file,
		char *const *argv, char *const *envp);

int ld_so_run_app(char *file, char **argv, char *const *envp);
int run_app(char *file, char **argv, char *const *envp);
int run_cputransparency(char *orig_file, char *file,
		char **argv, char *const *envp);

int run_sbrsh(char *sbrsh_bin, char *target_root, char *orig_file,char *file,
		char **argv, char *const *envp);
int run_qemu(char *qemu_bin, char *orig_file, char *file,
		char **argv, char *const *envp);

time_t get_sb2_timestamp(void);

#endif
