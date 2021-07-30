/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 */

#ifndef SB2_STAT_H
#define SB2_STAT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern int real_lstat(const char *path, struct stat *statbuf);
extern int real_lstat64(const char *path, struct stat64 *statbuf);
extern int real_stat(const char *path, struct stat *statbuf);
extern int real_stat64(const char *path, struct stat64 *statbuf);
extern int real_fstat(int fd, struct stat *statbuf);
extern int real_fstat64(int fd, struct stat64 *statbuf);
extern int real_fstatat(int dirfd, const char *path, struct stat *statbuf, int flags);
extern int real_fstatat64(int dirfd, const char *path, struct stat64 *statbuf, int flags);

extern int i_virtualize_struct_stat(const char *realfnname,
	struct stat *buf, struct stat64 *buf64);

extern int sb2_stat_file(const char *path, struct stat *buf, int *result_errno_ptr,
	int (*statfn_with_ver_ptr)(int ver, const char *filename, struct stat *buf),
	int ver,
	int (*statfn_ptr)(const char *filename, struct stat *buf));

extern int sb2_stat64_file(const char *path, struct stat64 *buf, int *result_errno_ptr,
	int (*stat64fn_with_ver_ptr)(int ver, const char *filename, struct stat64 *buf),
	int ver,
	int (*stat64fn_ptr)(const char *filename, struct stat64 *buf));

extern int sb2_fstat64(int fd, struct stat64 *statbuf);

#endif
