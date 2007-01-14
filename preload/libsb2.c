/*
 * libsb2 -- scratchbox2 preload library
 *
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 * parts contributed by 
 * 	Riku Voipio <riku.voipio@movial.com>
 *	Toni Timonen <toni.timonen@movial.com>
 * 
 * Heavily based on the libfakechroot library by 
 * Piotr Roszatycki <dexter@debian.org>
 */

/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include "config.h"
#include "config_hardcoded.h"

#define _GNU_SOURCE
#define __BSD_VISIBLE

#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <asm/unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <glob.h>
#include <utime.h>
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#ifdef HAVE_FTW_H
#include <ftw.h>
#endif
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include <elf.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <mapping.h>
#include <sb2.h>

#if defined(PATH_MAX)
#define SBOX_MAXPATH PATH_MAX
#elif defined(_POSIX_PATH_MAX)
#define SBOX_MAXPATH _POSIX_PATH_MAX
#elif defined(MAXPATHLEN)
#define SBOX_MAXPATH MAXPATHLEN
#else
#define SBOX_MAXPATH 2048
#endif

/* XXX: current code allows ./usr/bin/../../../../../ style escaping 
 *      from chroot 
 *      if ( amount /../ > amount /other/ ) ) remove extra /../
 */

#define SBOX_MAP_PROLOGUE() \
	char *sbox_path;

#define SBOX_MAP_AT_PROLOGUE() \
	char *sbox_path;

#define SBOX_MAP_PATH_NARROW(path, sbox_path) \
{ \
	if ((path) != NULL && *((char *)(path)) != '\0') { \
		sbox_path = scratchbox_path(__FUNCTION__, path); \
	} \
}

#define SBOX_MAP_PATH(path, sbox_path) \
{ \
	if ((path) != NULL) { \
		sbox_path = scratchbox_path(__FUNCTION__, path); \
	} \
}

#define SBOX_MAP_PATH_AT(dirfd, path, sbox_path) \
{ \
	if ((path) != NULL) { \
		if (path[0] == '/') { \
			/* absolute path */ \
			sbox_path = scratchbox_path(__FUNCTION__, path); \
		} else { \
			sbox_path = strdup(path); \
		}\
	} \
}


#define nextsym(function, name) \
{ \
	char *msg; \
	if (next_##function == NULL) { \
		*(void **)(&next_##function) = dlsym(RTLD_NEXT, name); \
		if ((msg = dlerror()) != NULL) { \
			fprintf (stderr, "%s: dlsym(%s): %s\n", PACKAGE_NAME, name, msg); \
		} \
	} \
}


#ifndef __GLIBC__
extern char **environ;
#endif


#ifndef HAVE_STRCHRNUL
/* Find the first occurrence of C in S or the final NUL byte.  */
static char *strchrnul (const char *s, int c_in)
{
	const unsigned char *char_ptr;
	const unsigned long int *longword_ptr;
	unsigned long int longword, magic_bits, charmask;
	unsigned char c;

	c = (unsigned char) c_in;

	/* Handle the first few characters by reading one character at a time.
	   Do this until CHAR_PTR is aligned on a longword boundary.  */
	for (char_ptr = s; ((unsigned long int) char_ptr
				& (sizeof(longword) - 1)) != 0; ++char_ptr)
		if (*char_ptr == c || *char_ptr == '\0')
			return (void *) char_ptr;

	/* All these elucidatory comments refer to 4-byte longwords,
	   but the theory applies equally well to 8-byte longwords.  */

	longword_ptr = (unsigned long int *) char_ptr;

	/* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
	 * the "holes."  Note that there is a hole just to the left of
	 * each byte, with an extra at the end:
	 *
	 * bits:  01111110 11111110 11111110 11111111
	 * bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD
	 *
	 * The 1-bits make sure that carries propagate to the next 0-bit.
	 * The 0-bits provide holes for carries to fall into.
	 * */
	switch (sizeof(longword)) {
		case 4:
			magic_bits = 0x7efefeffL;
			break;
		case 8:
			magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL;
			break;
		default:
			abort();
	}

	/* Set up a longword, each of whose bytes is C.  */
	charmask = c | (c << 8);
	charmask |= charmask << 16;
	if (sizeof(longword) > 4)
		/* Do the shift in two steps to avoid a warning if long has 32 bits.  */
		charmask |= (charmask << 16) << 16;
	if (sizeof(longword) > 8)
		abort();

	/* Instead of the traditional loop which tests each character,
	   we will test a longword at a time.  The tricky part is testing
	   if *any of the four* bytes in the longword in question are zero.  */
	for (;;) {
		/* We tentatively exit the loop if adding MAGIC_BITS to
		   LONGWORD fails to change any of the hole bits of LONGWORD.

		   1) Is this safe?  Will it catch all the zero bytes?
		   Suppose there is a byte with all zeros.  Any carry bits
		   propagating from its left will fall into the hole at its
		   least significant bit and stop.  Since there will be no
		   carry from its most significant bit, the LSB of the
		   byte to the left will be unchanged, and the zero will be
		   detected.

		   2) Is this worthwhile?  Will it ignore everything except
		   zero bytes?  Suppose every byte of LONGWORD has a bit set
		   somewhere.  There will be a carry into bit 8.  If bit 8
		   is set, this will carry into bit 16.  If bit 8 is clear,
		   one of bits 9-15 must be set, so there will be a carry
		   into bit 16.  Similarly, there will be a carry into bit
		   24.  If one of bits 24-30 is set, there will be a carry
		   into bit 31, so all of the hole bits will be changed.

		   The one misfire occurs when bits 24-30 are clear and bit
		   31 is set; in this case, the hole at bit 31 is not
		   changed.  If we had access to the processor carry flag,
		   we could close this loophole by putting the fourth hole
		   at bit 32!

		   So it ignores everything except 128's, when they're aligned
		   properly.

		   3) But wait!  Aren't we looking for C as well as zero?
		   Good point.  So what we do is XOR LONGWORD with a longword,
		   each of whose bytes is C.  This turns each byte that is C
		   into a zero.  */

		longword = *longword_ptr++;

		/* Add MAGIC_BITS to LONGWORD.  */
		if ((((longword + magic_bits)
			/* Set those bits that were unchanged by the addition.  */
			^ ~longword)

			/* Look at only the hole bits.  If any of the hole bits
			 * are unchanged, most likely one of the bytes was a
			 * zero.
			 */
			& ~magic_bits) != 0 ||
			/* That caught zeroes.  Now test for C.  */
			((((longword ^ charmask) +
				magic_bits) ^ ~(longword ^ charmask))
				& ~magic_bits) != 0) {
			
			/* Which of the bytes was C or zero?
			 * If none of them were, it was a misfire; continue the search.
			 */

			const unsigned char *cp =
				(const unsigned char *) (longword_ptr - 1);

			if (*cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (*++cp == c || *cp == '\0')
				return (char *) cp;
			if (sizeof(longword) > 4) {
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
				if (*++cp == c || *cp == '\0')
					return (char *) cp;
			}
		}
	}

	/* This should never happen.  */
	return NULL;
}
#endif


#ifdef HAVE___LXSTAT
static int     (*next___lxstat) (int ver, const char *filename, struct stat *buf) = NULL;
#endif
#ifdef HAVE___LXSTAT64
static int     (*next___lxstat64) (int ver, const char *filename, struct stat64 *buf) = NULL;
#endif
#ifdef HAVE___OPEN
static int     (*next___open) (const char *pathname, int flags, ...) = NULL;
#endif
#ifdef HAVE___OPEN64
static int     (*next___open64) (const char *pathname, int flags, ...) = NULL;
#endif
#ifdef HAVE___OPENDIR2
static DIR *   (*next___opendir2) (const char *name, int flags) = NULL;
#endif
#ifdef HAVE___XMKNOD
static int     (*next___xmknod) (int ver, const char *path, mode_t mode, dev_t *dev) = NULL;
#endif
#ifdef HAVE___XSTAT
static int     (*next___xstat) (int ver, const char *filename, struct stat *buf) = NULL;
#endif
#ifdef HAVE___XSTAT64
static int     (*next___xstat64) (int ver, const char *filename, struct stat64 *buf) = NULL;
#endif
#ifdef HAVE__XFTW
static int     (*next__xftw) (int mode, const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd) = NULL;
#endif
#ifdef HAVE__XFTW64
static int     (*next__xftw64) (int mode, const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd) = NULL;
#endif
static int     (*next_access) (const char *pathname, int mode) = NULL;
static int     (*next_acct) (const char *filename) = NULL;
#ifdef HAVE_CANONICALIZE_FILE_NAME
static char *  (*next_canonicalize_file_name) (const char *name) = NULL;
#endif
static int     (*next_chdir) (const char *path) = NULL;
static int     (*next_chmod) (const char *path, mode_t mode) = NULL;
static int     (*next_chown) (const char *path, uid_t owner, gid_t group) = NULL;
/* static int     (*next_chroot) (const char *path) = NULL; */
static int     (*next_creat) (const char *pathname, mode_t mode) = NULL;
static int     (*next_creat64) (const char *pathname, mode_t mode) = NULL;
#ifdef HAVE_DLMOPEN
static void *  (*next_dlmopen) (Lmid_t nsid, const char *filename, int flag) = NULL;
#endif
static void *  (*next_dlopen) (const char *filename, int flag) = NULL;
#ifdef HAVE_EUIDACCESS
static int     (*next_euidaccess) (const char *pathname, int mode) = NULL;
#endif
/* static int     (*next_execl) (const char *path, const char *arg, ...) = NULL; */
/* static int     (*next_execle) (const char *path, const char *arg, ...) = NULL; */
/* static int     (*next_execlp) (const char *file, const char *arg, ...) = NULL; */
/* static int     (*next_execv) (const char *path, char *const argv []) = NULL; */
int     (*next_execve) (const char *filename, char *const argv [], char *const envp[]) = NULL;
static int     (*next_execvp) (const char *file, char *const argv []) = NULL;
#ifdef HAVE_FACCESSAT
static int (*next_faccessat) (int dirfd, const char *pathname, int mode, int flags) = NULL;
#endif
#ifdef HAVE_FCHMODAT
static int (*next_fchmodat) (int dirfd, const char *pathname, mode_t mode, int flags) = NULL;
#endif
#ifdef HAVE_FCHOWNAT
static int (*next_fchownat) (int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) = NULL;
#endif
static FILE *  (*next_fopen) (const char *path, const char *mode) = NULL;
static FILE *  (*next_fopen64) (const char *path, const char *mode) = NULL;
static FILE *  (*next_freopen) (const char *path, const char *mode, FILE *stream) = NULL;
static FILE *  (*next_freopen64) (const char *path, const char *mode, FILE *stream) = NULL;
#ifdef HAVE_FSTATAT
static int (*next_fstatat) (int dirfd, const char *pathname, struct stat *buf, int flags) = NULL;
#endif
#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
static FTS *   (*next_fts_open) (char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **)) = NULL;
#endif
#endif
#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
static int     (*next_ftw) (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd) = NULL;
#endif
#endif
#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
static int     (*next_ftw64) (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd) = NULL;
#endif
#endif
#ifdef HAVE_FUTIMESAT
static int (*next_futimesat) (int dirfd, const char *pathname, const struct timeval times[2]) = NULL;
#endif
#ifdef HAVE_GET_CURRENT_DIR_NAME
static char *  (*next_get_current_dir_name) (void) = NULL;
#endif
static char *  (*next_getcwd) (char *buf, size_t size) = NULL;
static char *  (*next_getwd) (char *buf) = NULL;
#ifdef HAVE_GETXATTR
static ssize_t (*next_getxattr) (const char *path, const char *name, void *value, size_t size) = NULL;
#endif
static int     (*next_glob) (const char *pattern, int flags, int (*errfunc) (const char *, int), glob_t *pglob) = NULL;
#ifdef HAVE_GLOB64
static int     (*next_glob64) (const char *pattern, int flags, int (*errfunc) (const char *, int), glob64_t *pglob) = NULL;
#endif
#ifdef HAVE_GLOB_PATTERN_P
static int     (*next_glob_pattern_p) (const char *pattern, int quote) = NULL;
#endif
#ifdef HAVE_LCHMOD
static int     (*next_lchmod) (const char *path, mode_t mode) = NULL;
#endif
static int     (*next_lchown) (const char *path, uid_t owner, gid_t group) = NULL;
#ifdef HAVE_LCKPWDF
/* static int     (*next_lckpwdf) (void) = NULL; */
#endif
#ifdef HAVE_LGETXATTR
static ssize_t (*next_lgetxattr) (const char *path, const char *name, void *value, size_t size) = NULL;
#endif
static int     (*next_link) (const char *oldpath, const char *newpath) = NULL;
#ifdef HAVE_LINKAT
static int     (*next_linkat) (int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) = NULL;
#endif
#ifdef HAVE_LISTXATTR
static ssize_t (*next_listxattr) (const char *path, char *list, size_t size) = NULL;
#endif
#ifdef HAVE_LLISTXATTR
static ssize_t (*next_llistxattr) (const char *path, char *list, size_t size) = NULL;
#endif
#ifdef HAVE_LREMOVEXATTR
static int     (*next_lremovexattr) (const char *path, const char *name) = NULL;
#endif
#ifdef HAVE_LSETXATTR
static int     (*next_lsetxattr) (const char *path, const char *name, const void *value, size_t size, int flags) = NULL;
#endif
#if !defined(HAVE___LXSTAT)
static int     (*next_lstat) (const char *file_name, struct stat *buf) = NULL;
#endif
#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
static int     (*next_lstat64) (const char *file_name, struct stat64 *buf) = NULL;
#endif
#endif
#ifdef HAVE_LUTIMES
static int     (*next_lutimes) (const char *filename, const struct timeval tv[2]) = NULL;
#endif
static int     (*next_mkdir) (const char *pathname, mode_t mode) = NULL;
#ifdef HAVE_MKDIRAT
static int (*next_mkdirat) (int dirfd, const char *pathname, mode_t mode) = NULL;
#endif
#ifdef HAVE_MKDTEMP
static char *  (*next_mkdtemp) (char *template) = NULL;
#endif
static int     (*next_mknod) (const char *pathname, mode_t mode, dev_t dev) = NULL;
#ifdef HAVE_MKNODAT
static int     (*next_mknodat) (int dirfd, const char *pathname, mode_t mode, dev_t dev) = NULL;
#endif
static int     (*next_mkfifo) (const char *pathname, mode_t mode) = NULL;
#ifdef HAVE_MKFIFOAT
static int     (*next_mkfifoat) (int dirfd, const char *pathname, mode_t mode) = NULL;
#endif
static int     (*next_mkstemp) (char *template) = NULL;
static int     (*next_mkstemp64) (char *template) = NULL;
static char *  (*next_mktemp) (char *template) = NULL;
#ifdef HAVE_NFTW
static int     (*next_nftw) (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s), int nopenfd, int flags) = NULL;
#endif
#ifdef HAVE_NFTW64
static int     (*next_nftw64) (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag, struct FTW *s), int nopenfd, int flags) = NULL;
#endif
static int     (*next_open) (const char *pathname, int flags, ...) = NULL;
static int     (*next_open64) (const char *pathname, int flags, ...) = NULL;
#ifdef HAVE_OPENAT
static int     (*next_openat) (int dirfd, const char *pathname, int flags, ...) = NULL;
#endif
#ifdef HAVE_OPENAT64
static int     (*next_openat64) (int dirfd, const char *pathname, int flags, ...) = NULL;
#endif
#if !defined(HAVE___OPENDIR2)
static DIR *   (*next_opendir) (const char *name) = NULL;
#endif
static long    (*next_pathconf) (const char *path, int name) = NULL;
static int     (*next_readlink) (const char *path, char *buf, READLINK_TYPE_ARG3) = NULL;
#ifdef HAVE_READLINKAT
static int     (*next_readlinkat) (int dirfd, const char *pathname, char *buf, size_t bufsiz) = NULL;
#endif
static char *  (*next_realpath) (const char *name, char *resolved) = NULL;
static int     (*next_remove) (const char *pathname) = NULL;
#ifdef HAVE_REMOVEXATTR
static int     (*next_removexattr) (const char *path, const char *name) = NULL;
#endif
static int     (*next_rename) (const char *oldpath, const char *newpath) = NULL;
#ifdef HAVE_RENAMEAT
static int     (*next_renameat) (int olddirfd, const char *oldpath, int newdirfd, const char *newpath) = NULL;
#endif
#ifdef HAVE_REVOKE
static int     (*next_revoke) (const char *file) = NULL;
#endif
static int     (*next_rmdir) (const char *pathname) = NULL;
#ifdef HAVE_SCANDIR
static int     (*next_scandir) (const char *dir, struct dirent ***namelist, SCANDIR_TYPE_ARG3, int(*compar)(const void *, const void *)) = NULL;
#endif
#ifdef HAVE_SCANDIR64
static int     (*next_scandir64) (const char *dir, struct dirent64 ***namelist, int(*filter)(const struct dirent64 *), int(*compar)(const void *, const void *)) = NULL;
#endif
#ifdef HAVE_SETXATTR
static int     (*next_setxattr) (const char *path, const char *name, const void *value, size_t size, int flags) = NULL;
#endif
#if !defined(HAVE___XSTAT)
static int     (*next_stat) (const char *file_name, struct stat *buf) = NULL;
#endif
#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
static int     (*next_stat64) (const char *file_name, struct stat64 *buf) = NULL;
#endif
#endif
static int     (*next_symlink) (const char *oldpath, const char *newpath) = NULL;
#ifdef HAVE_SYMLINKAT
static int     (*next_symlinkat) (const char *oldpath, int newdirfd, const char *newpath) = NULL;
#endif
static char *  (*next_tempnam) (const char *dir, const char *pfx) = NULL;
static char *  (*next_tmpnam) (char *s) = NULL;
static int     (*next_truncate) (const char *path, off_t length) = NULL;
#ifdef HAVE_TRUNCATE64
static int     (*next_truncate64) (const char *path, off64_t length) = NULL;
#endif
static int     (*next_unlink) (const char *pathname) = NULL;
#ifdef HAVE_UNLINKAT
static int     (*next_unlinkat) (int dirfd, const char *pathname, int flags) = NULL;
#endif
#ifdef HAVE_ULCKPWDF
/* static int     (*next_ulckpwdf) (void) = NULL; */
#endif
static int     (*next_utime) (const char *filename, const struct utimbuf *buf) = NULL;
static int     (*next_utimes) (const char *filename, const struct timeval tv[2]) = NULL;

static int     (*next_uname) (struct utsname *buf) = NULL;





void libsb2_init (void) __attribute((constructor));
void libsb2_init (void)
{
	//DBGOUT("fakechroot init start: %i\n", getpid());
#ifdef HAVE___LXSTAT
	nextsym(__lxstat, "__lxstat");
#endif
#ifdef HAVE___LXSTAT64
	nextsym(__lxstat64, "__lxstat64");
#endif
#ifdef HAVE___OPEN
	nextsym(__open, "__open");
#endif
#ifdef HAVE___OPEN64
	nextsym(__open64, "__open64");
#endif
#ifdef HAVE___OPENDIR2
	nextsym(__opendir2, "__opendir2");
#endif
#ifdef HAVE___XMKNOD
	nextsym(__xmknod, "__xmknod");
#endif
#ifdef HAVE___XSTAT
	nextsym(__xstat, "__xstat");
#endif
#ifdef HAVE___XSTAT64
	nextsym(__xstat64, "__xstat64");
#endif
	nextsym(access, "access");
	nextsym(acct, "acct");
#ifdef HAVE_CANONICALIZE_FILE_NAME
	nextsym(canonicalize_file_name, "canonicalize_file_name");
#endif
	nextsym(chdir, "chdir");
	nextsym(chmod, "chmod");
	nextsym(chown, "chown");
	nextsym(creat, "creat");
	nextsym(creat64, "creat64");
#ifdef HAVE_DLMOPEN
	nextsym(dlmopen, "dlmopen");
#endif
	nextsym(dlopen, "dlopen");
#ifdef HAVE_EUIDACCESS
	nextsym(euidaccess, "euidaccess");
#endif
	/*    nextsym(execl, "execl"); */
	/*    nextsym(execle, "execle"); */
	/*    nextsym(execlp, "execlp"); */
	/*    nextsym(execv, "execv"); */
	nextsym(execve, "execve");
	nextsym(execvp, "execvp");
#ifdef HAVE_FACCESSAT
	nextsym(faccessat, "faccessat");
#endif
#ifdef HAVE_FCHMODAT
	nextsym(fchmodat, "fchmodat");
#endif
#ifdef HAVE_FCHOWNAT
	nextsym(fchownat, "fchownat");
#endif
	nextsym(fopen, "fopen");
	nextsym(fopen64, "fopen64");
	nextsym(freopen, "freopen");
	nextsym(freopen64, "freopen64");
#ifdef HAVE_FSTATAT
	nextsym(fstatat, "fstatat");
#endif
#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
	nextsym(fts_open, "fts_open");
#endif
#endif
#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
	nextsym(ftw, "ftw");
#endif
#endif
#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
	nextsym(ftw64, "ftw64");
#endif
#endif
#ifdef HAVE_FUTIMESAT
	nextsym(futimesat, "futimesat");
#endif
#ifdef HAVE_GET_CURRENT_DIR_NAME
	nextsym(get_current_dir_name, "get_current_dir_name");
#endif
	nextsym(getcwd, "getcwd");
	nextsym(getwd, "getwd");
#ifdef HAVE_GETXATTR
	nextsym(getxattr, "getxattr");
#endif
	nextsym(glob, "glob");
#ifdef HAVE_GLOB64
	nextsym(glob64, "glob64");
#endif
#ifdef HAVE_GLOB_PATTERN_P
	nextsym(glob_pattern_p, "glob_pattern_p");
#endif
#ifdef HAVE_LCHMOD
	nextsym(lchmod, "lchmod");
#endif
	nextsym(lchown, "lchown");
#ifdef HAVE_LCKPWDF
	/*    nextsym(lckpwdf, "lckpwdf"); */
#endif
#ifdef HAVE_LGETXATTR
	nextsym(lgetxattr, "lgetxattr");
#endif
	nextsym(link, "link");
#ifdef HAVE_LINKAT
	nextsym(linkat, "linkat");
#endif
#ifdef HAVE_LISTXATTR
	nextsym(listxattr, "listxattr");
#endif
#ifdef HAVE_LLISTXATTR
	nextsym(llistxattr, "llistxattr");
#endif
#ifdef HAVE_LREMOVEXATTR
	nextsym(lremovexattr, "lremovexattr");
#endif
#ifdef HAVE_LSETXATTR
	nextsym(lsetxattr, "lsetxattr");
#endif
#if !defined(HAVE___LXSTAT)
	nextsym(lstat, "lstat");
#endif
#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
	nextsym(lstat64, "lstat64");
#endif
#endif
#ifdef HAVE_LUTIMES
	nextsym(lutimes, "lutimes");
#endif
	nextsym(mkdir, "mkdir");
#ifdef HAVE_MKDIRAT
	nextsym(mkdirat, "mkdirat");
#endif
#ifdef HAVE_MKDTEMP
	nextsym(mkdtemp, "mkdtemp");
#endif
	nextsym(mknod, "mknod");
#ifdef HAVE_MKNODAT
	nextsym(mknodat, "mknodat");
#endif
	nextsym(mkfifo, "mkfifo");
#ifdef HAVE_MKFIFOAT
	nextsym(mkfifoat, "mkfifoat");
#endif
	nextsym(mkstemp, "mkstemp");
	nextsym(mkstemp64, "mkstemp64");
	nextsym(mktemp, "mktemp");
#ifdef HAVE_NFTW
	nextsym(nftw, "nftw");
#endif
#ifdef HAVE_NFTW64
	nextsym(nftw64, "nftw64");
#endif
	nextsym(open, "open");
#ifdef HAVE_OPENAT
	nextsym(openat, "openat");
#endif
	nextsym(open64, "open64");
#ifdef HAVE_OPENAT64
	nextsym(openat64, "openat64");
#endif
#if !defined(HAVE___OPENDIR2)
	nextsym(opendir, "opendir");
#endif
	nextsym(pathconf, "pathconf");
	nextsym(readlink, "readlink");
#ifdef HAVE_READLINKAT
	nextsym(readlinkat, "readlinkat");
#endif
	nextsym(realpath, "realpath");
	nextsym(remove, "remove");
#ifdef HAVE_REMOVEXATTR
	nextsym(removexattr, "removexattr");
#endif
	nextsym(rename, "rename");
#ifdef HAVE_RENAMEAT
	nextsym(renameat, "renameat");
#endif
#ifdef HAVE_REVOKE
	nextsym(revoke, "revoke");
#endif
	nextsym(rmdir, "rmdir");
#ifdef HAVE_SCANDIR
	nextsym(scandir, "scandir");
#endif
#ifdef HAVE_SCANDIR64
	nextsym(scandir64, "scandir64");
#endif
#ifdef HAVE_SETXATTR
	nextsym(setxattr, "setxattr");
#endif
#if !defined(HAVE___XSTAT)
	nextsym(stat, "stat");
#endif
#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
	nextsym(stat64, "stat64");
#endif
#endif
	nextsym(symlink, "symlink");
#ifdef HAVE_SYMLINKAT
	nextsym(symlinkat, "symlinkat");
#endif
	nextsym(tempnam, "tempnam");
	nextsym(tmpnam, "tmpnam");
	nextsym(truncate, "truncate");
#ifdef HAVE_TRUNCATE64
	nextsym(truncate64, "truncate64");
#endif
	nextsym(unlink, "unlink");
#ifdef HAVE_UNLINKAT
	nextsym(unlinkat, "unlinkat");
#endif
#ifdef HAVE_ULCKPWDF
	/*    nextsym(ulckpwdf, "ulckpwdf"); */
#endif
	nextsym(utime, "utime");
	nextsym(utimes, "utimes");

	nextsym(uname, "uname");
}


int sb_next_execve(const char *file, char *const *argv, char *const *envp)
{
	if (next_execve == NULL) libsb2_init();
	return next_execve(file, argv, envp);
}


#ifdef HAVE___LXSTAT
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __lxstat (int ver, const char *filename, struct stat *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next___lxstat == NULL) libsb2_init();
	ret = next___lxstat(ver, sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___LXSTAT64
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __lxstat64 (int ver, const char *filename, struct stat64 *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next___lxstat64 == NULL) libsb2_init();
	ret = next___lxstat64(ver, sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___OPEN
/* Internal libc function */
int __open (const char *pathname, int flags, ...)
{
	SBOX_MAP_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH(pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next___open == NULL) libsb2_init();
	ret = next___open(sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___OPEN64
/* Internal libc function */
int __open64 (const char *pathname, int flags, ...)
{
	SBOX_MAP_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH(pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next___open64 == NULL) libsb2_init();
	ret = next___open64(sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___OPENDIR2
/* Internal libc function */
/* #include <dirent.h> */
DIR *__opendir2 (const char *name, int flags)
{
	SBOX_MAP_PROLOGUE();
	DIR *ret;

	SBOX_MAP_PATH(name, sbox_path);
	if (next___opendir2 == NULL) libsb2_init();
	ret = next___opendir2(sbox_path, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___XMKNOD
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xmknod (int ver, const char *path, mode_t mode, dev_t *dev)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next___xmknod == NULL) libsb2_init();
	ret = next___xmknod(ver, sbox_path, mode, dev);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___XSTAT
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xstat (int ver, const char *filename, struct stat *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next___xstat == NULL) libsb2_init();
	ret = next___xstat(ver, sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE___XSTAT64
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int __xstat64 (int ver, const char *filename, struct stat64 *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next___xstat64 == NULL) libsb2_init();
	ret = next___xstat64(ver, sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE__XFTW
/* include <ftw.h> */
int _xftw (int mode, const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next__xftw == NULL) libsb2_init();
	ret = next__xftw(mode, sbox_path, fn, nopenfd);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE__XFTW64
/* include <ftw.h> */
int _xftw64 (int mode, const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next__xftw64 == NULL) libsb2_init();
	ret = next__xftw64(mode, sbox_path, fn, nopenfd);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
int access (const char *pathname, int mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_access == NULL) libsb2_init();
	ret = next_access(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <unistd.h> */
int acct (const char *filename)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_acct == NULL) libsb2_init();
	ret = next_acct(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_CANONICALIZE_FILE_NAME
/* #include <stdlib.h> */
char *canonicalize_file_name (const char *name)
{
	SBOX_MAP_PROLOGUE();
	char *ret;

	SBOX_MAP_PATH(name, sbox_path);
	if (next_canonicalize_file_name == NULL) libsb2_init();
	ret = next_canonicalize_file_name(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
int chdir (const char *path)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_chdir == NULL) libsb2_init();
	ret = next_chdir(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int chmod (const char *path, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_chmod == NULL) libsb2_init();
	ret = next_chmod(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <sys/types.h> */
/* #include <unistd.h> */
int chown (const char *path, uid_t owner, gid_t group)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_chown == NULL) libsb2_init();
	ret = next_chown(sbox_path, owner, group);
	if (sbox_path) free(sbox_path);
	return ret;
}



/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int creat (const char *pathname, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_creat == NULL) libsb2_init();
	ret = next_creat(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int creat64 (const char *pathname, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_creat64 == NULL) libsb2_init();
	ret = next_creat64(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_DLMOPEN
/* #include <dlfcn.h> */
void *dlmopen (Lmid_t nsid, const char *filename, int flag)
{
	SBOX_MAP_PROLOGUE();
	void *ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_dlmopen == NULL) libsb2_init();
	ret = next_dlmopen(nsid, sbox_path, flag);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <dlfcn.h> */
void *dlopen (const char *filename, int flag)
{
	SBOX_MAP_PROLOGUE();
	void *ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_dlopen == NULL) libsb2_init();
	ret = next_dlopen(sbox_path, flag);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_EUIDACCESS
/* #include <unistd.h> */
int euidaccess (const char *pathname, int mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_euidaccess == NULL) libsb2_init();
	ret = next_euidaccess(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
int execl (const char *path, const char *arg, ...)
{
	size_t argv_max = 1024;
	const char **argv = alloca (argv_max * sizeof (const char *));
	unsigned int i;
	va_list args;

	argv[0] = arg;

	va_start (args, arg);
	i = 0;
	while (argv[i++] != NULL)
	{
		if (i == argv_max)
		{
			const char **nptr = alloca ((argv_max *= 2) * sizeof (const char *));

			if ((char *) argv + i == (char *) nptr)
				/* Stack grows up.  */
				argv_max += i;
			else
				/* We have a hole in the stack.  */
				argv = (const char **) memcpy (nptr, argv,
						i * sizeof (const char *));
		}

		argv[i] = va_arg (args, const char *);
	}
	va_end (args);

	return execve (path, (char *const *) argv, environ);
}


/* #include <unistd.h> */
int execle (const char *path, const char *arg, ...)
{
	size_t argv_max = 1024;
	const char **argv = alloca (argv_max * sizeof (const char *));
	const char *const *envp;
	unsigned int i;
	va_list args;
	argv[0] = arg;

	va_start (args, arg);
	i = 0;
	while (argv[i++] != NULL)
	{
		if (i == argv_max)
		{
			const char **nptr = alloca ((argv_max *= 2) * sizeof (const char *));

			if ((char *) argv + i == (char *) nptr)
				/* Stack grows up.  */
				argv_max += i;
			else
				/* We have a hole in the stack.  */
				argv = (const char **) memcpy (nptr, argv,
						i * sizeof (const char *));
		}

		argv[i] = va_arg (args, const char *);
	}

	envp = va_arg (args, const char *const *);
	va_end (args);

	return execve (path, (char *const *) argv, (char *const *) envp);
}

/* Execute FILE, searching in the `PATH' environment variable if
   it contains no slashes, with all arguments after FILE until a
   NULL pointer and environment from `environ'.  */
	int
execlp (const char *file, const char *arg, ...)
{
	size_t argv_max = 1024;
	const char **argv = alloca (argv_max * sizeof (const char *));
	unsigned int i;
	va_list args;

	argv[0] = arg;

	va_start (args, arg);
	i = 0;
	while (argv[i++] != NULL)
	{
		if (i == argv_max)
		{
			const char **nptr = alloca ((argv_max *= 2) * sizeof (const char *));

#ifndef _STACK_GROWS_UP
			if ((char *) nptr + argv_max == (char *) argv)
			{
				/* Stack grows down.  */
				argv = (const char **) memcpy (nptr, argv,
						i * sizeof (const char *));
				argv_max += i;
			}
			else
#endif
#ifndef _STACK_GROWS_DOWN
				if ((char *) argv + i == (char *) nptr)
					/* Stack grows up.  */
					argv_max += i;
				else
#endif
					/* We have a hole in the stack.  */
					argv = (const char **) memcpy (nptr, argv,
							i * sizeof (const char *));
		}

		argv[i] = va_arg (args, const char *);
	}
	va_end (args);

	return execvp (file, (char *const *) argv);
}



/* #include <unistd.h> */
int execv (const char *path, char *const argv [])
{
	return execve (path, argv, environ);
}


/* #include <unistd.h> */
int execve (const char *filename, char *const argv [], char *const envp[])
{
	SBOX_MAP_PROLOGUE();
	char *hb_sbox_path;
	int ret;
	int file;
	char hashbang[SBOX_MAXPATH];
	size_t argv_max = 1024;
	const char **newargv = alloca (argv_max * sizeof (const char *));
	char newfilename[SBOX_MAXPATH], argv0[SBOX_MAXPATH];
	char *ptr;
	int k;
	unsigned int i, j, n;
	char c;

	SBOX_MAP_PATH(filename, sbox_path);

	if ((file = open(sbox_path, O_RDONLY)) == -1) {
		errno = ENOENT;
		if (sbox_path) free(sbox_path);
		return -1;
	}

	k = read(file, hashbang, SBOX_MAXPATH-2);
	close(file);
	if (k == -1) {
		errno = ENOENT;
		if (sbox_path) free(sbox_path);
		return -1;
	}

	if (hashbang[0] != '#' || hashbang[1] != '!') {
		ret = do_exec(sbox_path, argv, envp);
		if (sbox_path) free(sbox_path);
		return ret;
	}

	/* if we're here we have a script */

	//printf("hashbang: %s\n", hashbang);
	for (i = j = 2; (hashbang[i] == ' ' || hashbang[i] == '\t') && i < SBOX_MAXPATH; i++, j++) {
		//printf("looping\n");
	}

	//printf("hashbanging: i=%u\n",i);
	//hashbang[i] = hashbang[i+1] = 0;

	for (n = 0; i < SBOX_MAXPATH; i++) {
		c = hashbang[i];
		if (hashbang[i] == 0 || hashbang[i] == ' ' || hashbang[i] == '\t' || hashbang[i] == '\n') {
			hashbang[i] = 0;
			if (i > j) {
				if (n == 0) {
					ptr = &hashbang[j];
					//printf("hashbanging ptr, sbox_path: %s, %s\n", ptr, sbox_path);
					SBOX_MAP_PATH(ptr, hb_sbox_path);
					strcpy(newfilename, hb_sbox_path);
					strcpy(argv0, &hashbang[j]);
					newargv[n++] = argv0;
					free(hb_sbox_path);
					hb_sbox_path = NULL;
				} else {
					newargv[n++] = &hashbang[j];
				}
			}
			j = i + 1;
		}
		if (c == '\n' || c == 0) break;
	}

	//printf("hashbanging: %s, %s\n", filename, sbox_path);
	SBOX_MAP_PATH(filename, hb_sbox_path);
	newargv[n++] = hb_sbox_path;

	for (i = 1; argv[i] != NULL && i < argv_max; ) {
		newargv[n++] = argv[i++];
	}

	newargv[n] = 0;

	ret = do_exec(newfilename, (char *const *)newargv, envp);
	if (hb_sbox_path) free(hb_sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <unistd.h> */
int execvp (const char *file, char *const argv [])
{
	if (*file == '\0')
	{
		/* We check the simple case first. */
		errno = ENOENT;
		return -1;
	}

	if (strchr (file, '/') != NULL)
	{
		/* Don't search when it contains a slash.  */
		return execve (file, argv, environ);
	}
	else
	{
		int got_eacces = 0;
		char *path, *p, *name;
		size_t len;
		size_t pathlen;

		path = getenv ("PATH");
		if (path) path = strdup(path);
		if (path == NULL)
		{
			/* There is no `PATH' in the environment.
			   The default search path is the current directory
			   followed by the path `confstr' returns for `_CS_PATH'.  */
			len = confstr (_CS_PATH, (char *) NULL, 0);
			path = (char *) alloca (1 + len);
			path[0] = ':';
			(void) confstr (_CS_PATH, path + 1, len);
		}

		len = strlen (file) + 1;
		pathlen = strlen (path);
		name = alloca (pathlen + len + 1);
		/* Copy the file name at the top.  */
		name = (char *) memcpy (name + pathlen + 1, file, len);
		/* And add the slash.  */
		*--name = '/';

		p = path;
		do
		{
			char *startp;

			path = p;
			p = strchrnul (path, ':');

			if (p == path)
				/* Two adjacent colons, or a colon at the beginning or the end
				   of `PATH' means to search the current directory.  */
				startp = name + 1;
			else
				startp = (char *) memcpy (name - (p - path), path, p - path);

			/* Try to execute this name.  If it works, execv will not return.  */
			execve (startp, argv, environ);

			switch (errno)
			{
				case EACCES:
					/* Record the we got a `Permission denied' error.  If we end
					   up finding no executable we can use, we want to diagnose
					   that we did find one but were denied access.  */
					got_eacces = 1;
				case ENOENT:
				case ESTALE:
				case ENOTDIR:
					/* Those errors indicate the file is missing or not executable
					   by us, in which case we want to just try the next path
					   directory.  */
					break;

				default:
					/* Some other error means we found an executable file, but
					   something went wrong executing it; return the error to our
					   caller.  */
					return -1;
			}
		}
		while (*p++ != '\0');

		/* We tried every element and none of them worked.  */
		if (got_eacces)
			/* At least one failure was due to permissions, so report that
			   error.  */
			errno = EACCES;
	}

	/* Return the error from the last attempt (probably ENOENT).  */
	return -1;
}


#ifdef HAVE_FACCESSAT
int faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_faccessat == NULL) libsb2_init();
	ret = next_faccessat(dirfd, sbox_path, mode, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

#ifdef HAVE_FCHMODAT
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_fchmodat == NULL) libsb2_init();
	ret = next_fchmodat(dirfd, sbox_path, mode, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif
#ifdef HAVE_FCHOWNAT
int fchownat (int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_fchownat == NULL) libsb2_init();
	ret = next_fchownat(dirfd, sbox_path, owner, group, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <stdio.h> */
FILE *fopen (const char *path, const char *mode)
{
	SBOX_MAP_PROLOGUE();
	FILE *ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_fopen == NULL) libsb2_init();
	ret = next_fopen(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <stdio.h> */
FILE *fopen64 (const char *path, const char *mode)
{
	SBOX_MAP_PROLOGUE();
	FILE *ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_fopen64 == NULL) libsb2_init();
	ret = next_fopen64(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <stdio.h> */
FILE *freopen (const char *path, const char *mode, FILE *stream)
{
	SBOX_MAP_PROLOGUE();
	FILE *ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_freopen == NULL) libsb2_init();
	ret = next_freopen(sbox_path, mode, stream);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <stdio.h> */
FILE *freopen64 (const char *path, const char *mode, FILE *stream)
{
	SBOX_MAP_PROLOGUE();
	FILE *ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_freopen64 == NULL) libsb2_init();
	ret = next_freopen64(sbox_path, mode, stream);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_FSTATAT
int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_fstatat == NULL) libsb2_init();
	ret = next_fstatat(dirfd, sbox_path, buf, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_FTS_OPEN
#if !defined(HAVE___OPENDIR2)
/* #include <fts.h> */
FTS * fts_open (char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **)) {
	SBOX_MAP_PROLOGUE();
	char *path;
	char * const *p;
	char **new_path_argv;
	char **np;
	int n;

	for (n=0, p=path_argv; *p; n++, p++);
	if ((new_path_argv = malloc(n*(sizeof(char *)))) == NULL) {
		return NULL;
	}

	for (n=0, p=path_argv, np=new_path_argv; *p; n++, p++, np++) {
		path = *p;
		SBOX_MAP_PATH(path, sbox_path);
		*np = sbox_path;
	}

	if (next_fts_open == NULL) libsb2_init();
	return next_fts_open(new_path_argv, options, compar);
}
#endif
#endif


#ifdef HAVE_FTW
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW)
/* include <ftw.h> */
int ftw (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_ftw == NULL) libsb2_init();
	ret = next_ftw(sbox_path, fn, nopenfd);
	if (sbox_path) free(sbox_path);
	return ret;

}
#endif
#endif


#ifdef HAVE_FTW64
#if !defined(HAVE___OPENDIR2) && !defined(HAVE__XFTW64)
/* include <ftw.h> */
int ftw64 (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_ftw64 == NULL) libsb2_init();
	ret = next_ftw64(sbox_path, fn, nopenfd);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif
#endif


#ifdef HAVE_FUTIMESAT
int futimesat(int dirfd, const char *pathname, const struct timeval times[2])
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_futimesat == NULL) libsb2_init();
	ret = next_futimesat(dirfd, sbox_path, times);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_GET_CURRENT_DIR_NAME
/* #include <unistd.h> */
char * get_current_dir_name (void) 
{
	SBOX_MAP_PROLOGUE();
	char *cwd;

	if (next_get_current_dir_name == NULL) libsb2_init();

	if ((cwd = next_get_current_dir_name()) == NULL) {
		return NULL;
	}
	SBOX_MAP_PATH_NARROW(cwd, sbox_path);
	free(cwd);
	return sbox_path;
}
#endif


/* #include <unistd.h> */
char * getcwd (char *buf, size_t size)
{
	SBOX_MAP_PROLOGUE();
	char *cwd;

	if (next_getcwd == NULL) libsb2_init();

	if ((cwd = next_getcwd(buf, size)) == NULL) {
		return NULL;
	}
	SBOX_MAP_PATH_NARROW(cwd, sbox_path);
	if (sbox_path) {
		strncpy(buf, sbox_path, size);
		free(sbox_path);
	}
	return cwd;
}


/* #include <unistd.h> */
char * getwd (char *buf)
{
	SBOX_MAP_PROLOGUE();
	char *cwd;

	if (next_getwd == NULL) libsb2_init();

	if ((cwd = next_getwd(buf)) == NULL) {
		return NULL;
	}
	SBOX_MAP_PATH_NARROW(cwd, sbox_path);
	if (sbox_path) {
		strcpy(buf, sbox_path);
		free(sbox_path);
	}
	return cwd;
}


#ifdef HAVE_GETXATTR
/* #include <sys/xattr.h> */
ssize_t getxattr (const char *path, const char *name, void *value, size_t size)
{
	SBOX_MAP_PROLOGUE();
	ssize_t ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_getxattr == NULL) libsb2_init();
	ret = next_getxattr(sbox_path, name, value, size);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <glob.h> */
int glob (const char *pattern, int flags, int (*errfunc) (const char *, int), glob_t *pglob)
{
	SBOX_MAP_PROLOGUE();
	int rc;
	unsigned int i;
	char tmp[SBOX_MAXPATH];

	SBOX_MAP_PATH(pattern, sbox_path);

	if (next_glob == NULL) libsb2_init();

	rc = next_glob(sbox_path, flags, errfunc, pglob);
	if (sbox_path) free(sbox_path);
	
	if (rc < 0) return rc;

	for(i = 0; i < pglob->gl_pathc; i++) {
		strcpy(tmp,pglob->gl_pathv[i]);
		sbox_path = scratchbox_path(__FUNCTION__, tmp);
		strcpy(pglob->gl_pathv[i], sbox_path);
		if (sbox_path) free(sbox_path);
	}
	return rc;
}


#ifdef HAVE_GLOB64
/* #include <glob.h> */
int glob64 (const char *pattern, int flags, int (*errfunc) (const char *, int), glob64_t *pglob)
{
	SBOX_MAP_PROLOGUE();
	int rc;
	unsigned int i;
	char tmp[SBOX_MAXPATH];

	if (next_glob64 == NULL) libsb2_init();
	SBOX_MAP_PATH(pattern, sbox_path);

	rc = next_glob64(sbox_path, flags, errfunc, pglob);
	if (sbox_path) free(sbox_path);

	if (rc < 0) return rc;

	for(i = 0; i < pglob->gl_pathc; i++) {
		strcpy(tmp,pglob->gl_pathv[i]);
		sbox_path = scratchbox_path(__FUNCTION__, tmp);
		strcpy(pglob->gl_pathv[i], sbox_path);
		if (sbox_path) free(sbox_path);
	}
	return rc;
}
#endif


#ifdef HAVE_GLOB_PATTERN_P
/* #include <glob.h> */
int glob_pattern_p (const char *pattern, int quote)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pattern, sbox_path);
	if (next_glob_pattern_p == NULL) libsb2_init();
	ret = next_glob_pattern_p(sbox_path, quote);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_LCHMOD
/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int lchmod (const char *path, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_lchmod == NULL) libsb2_init();
	ret = next_lchmod(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <sys/types.h> */
/* #include <unistd.h> */
int lchown (const char *path, uid_t owner, gid_t group)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_lchown == NULL) libsb2_init();
	ret = next_lchown(sbox_path, owner, group);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_LCKPWDF
/* #include <shadow.h> */
int lckpwdf (void)
{
	return 0;
}
#endif


#ifdef HAVE_LGETXATTR
/* #include <sys/xattr.h> */
ssize_t lgetxattr (const char *path, const char *name, void *value, size_t size)
{
	SBOX_MAP_PROLOGUE();
	ssize_t ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_lgetxattr == NULL) libsb2_init();
	ret = next_lgetxattr(sbox_path, name, value, size);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
int link(const char *oldpath, const char *newpath)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH(oldpath, sbox_path_old);
	SBOX_MAP_PATH(newpath, sbox_path_new);
	if (next_link == NULL) libsb2_init();
	ret = next_link(sbox_path_old, sbox_path_new);
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	return ret;
}


#ifdef HAVE_LINKAT
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH_AT(olddirfd, oldpath, sbox_path_old);
	SBOX_MAP_PATH_AT(newdirfd, newpath, sbox_path_new);

	if (next_linkat == NULL) libsb2_init();
	ret = next_linkat(olddirfd, sbox_path_old, newdirfd, sbox_path_new, flags);
	
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	
	return ret;
}
#endif


#ifdef HAVE_LISTXATTR
/* #include <sys/xattr.h> */
ssize_t listxattr (const char *path, char *list, size_t size)
{
	SBOX_MAP_PROLOGUE();
	ssize_t ret;
	SBOX_MAP_PATH(path, sbox_path);
	if (next_listxattr == NULL) libsb2_init();
	ret = next_listxattr(sbox_path, list, size);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_LLISTXATTR
/* #include <sys/xattr.h> */
ssize_t llistxattr (const char *path, char *list, size_t size)
{
	SBOX_MAP_PROLOGUE();
	ssize_t ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_llistxattr == NULL) libsb2_init();
	ret = next_llistxattr(sbox_path, list, size);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_LREMOVEXATTR
/* #include <sys/xattr.h> */
int lremovexattr (const char *path, const char *name)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_lremovexattr == NULL) libsb2_init();
	ret = next_lremovexattr(sbox_path, name);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_LSETXATTR
/* #include <sys/xattr.h> */
int lsetxattr (const char *path, const char *name, const void *value, size_t size, int flags)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_lsetxattr == NULL) libsb2_init();
	ret = next_lsetxattr(sbox_path, name, value, size, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#if !defined(HAVE___LXSTAT)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int lstat (const char *file_name, struct stat *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(file_name, sbox_path);
	if (next_lstat == NULL) libsb2_init();
	ret = next_lstat(sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_LSTAT64
#if !defined(HAVE___LXSTAT64)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int lstat64 (const char *file_name, struct stat64 *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(file_name, sbox_path);
	if (next_lstat64 == NULL) libsb2_init();
	ret = next_lstat64(sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif
#endif


#ifdef HAVE_LUTIMES
/* #include <sys/time.h> */
int lutimes(const char *filename, const struct timeval tv[2])
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_lutimes == NULL) libsb2_init();
	ret = next_lutimes(sbox_path, tv);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <sys/stat.h> */
/* #include <sys/types.h> */
int mkdir(const char *pathname, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;
	
	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_mkdir == NULL) libsb2_init();
	ret = next_mkdir(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_MKDIRAT
int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_mkdirat == NULL) libsb2_init();
	ret = next_mkdirat(dirfd, sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_MKDTEMP
/* #include <stdlib.h> */
char *mkdtemp (char *template)
{
	//    char tmp[FAKECHROOT_MAXPATH], *oldtemplate, *ptr;
	//    SBOX_MAP_PROLOGUE();

	//    oldtemplate = template;

	//    SBOX_MAP_PATH(template, sbox_path);

	if (next_mkdtemp == NULL) libsb2_init();

	if (next_mkdtemp(template) == NULL) {
		return NULL;
	}
	//    ptr = tmp;
	//    strcpy(ptr, template);
	//    narrow_chroot_path(ptr, sbox_path);
	//    if (ptr == NULL) {
	//        return NULL;
	//    }
	//    strcpy(oldtemplate, ptr);
	//    DBGOUT("before returning from mkdtemp\n");
	return template;
}
#endif


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
int mkfifo (const char *pathname, mode_t mode)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_mkfifo == NULL) libsb2_init();
	ret = next_mkfifo(sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_MKFIFOAT
int mkfifoat(int dirfd, const char *pathname, mode_t mode)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_mkfifoat == NULL) libsb2_init();
	ret = next_mkfifoat(dirfd, sbox_path, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
/* #include <unistd.h> */
int mknod (const char *pathname, mode_t mode, dev_t dev)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_mknod == NULL) libsb2_init();
	ret = next_mknod(sbox_path, mode, dev);
	if (sbox_path) free(sbox_path);
	return ret;
}

#ifdef HAVE_MKNODAT
int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_mknodat == NULL) libsb2_init();
	ret = next_mknodat(dirfd, sbox_path, mode, dev);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

/* #include <stdlib.h> */
int mkstemp (char *template)
{
	if (next_mkstemp == NULL) libsb2_init();
	return next_mkstemp(template);
}


/* #include <stdlib.h> */
int mkstemp64 (char *template)
{
	if (next_mkstemp64 == NULL) libsb2_init();
	return next_mkstemp64(template);
}


/* #include <stdlib.h> */
char *mktemp (char *template)
{
	if (next_mktemp == NULL) libsb2_init();
	return next_mktemp(template);
}


#ifdef HAVE_NFTW
/* #include <ftw.h> */
int nftw (const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s), int nopenfd, int flags)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_nftw == NULL) libsb2_init();
	ret = next_nftw(sbox_path, fn, nopenfd, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_NFTW64
/* #include <ftw.h> */
int nftw64 (const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag, struct FTW *s), int nopenfd, int flags)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_nftw64 == NULL) libsb2_init();
	ret = next_nftw64(sbox_path, fn, nopenfd, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int open(const char *pathname, int flags, ...) 
{
	SBOX_MAP_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH(pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next_open == NULL) libsb2_init();
	ret = next_open(sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <sys/types.h> */
/* #include <sys/stat.h> */
/* #include <fcntl.h> */
int open64 (const char *pathname, int flags, ...)
{
	SBOX_MAP_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH(pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next_open64 == NULL) libsb2_init();
	ret = next_open64(sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_OPENAT
int openat(int dirfd, const char *pathname, int flags, ...)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next_openat == NULL) libsb2_init();
	ret = next_openat(dirfd, sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_OPENAT64
int openat64(int dirfd, const char *pathname, int flags, ...)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;
	int mode = 0;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);

	if (flags & O_CREAT) {
		va_list arg;
		va_start (arg, flags);
		mode = va_arg (arg, int);
		va_end (arg);
	}

	if (next_openat64 == NULL) libsb2_init();
	ret = next_openat64(dirfd, sbox_path, flags, mode);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

#if !defined(HAVE___OPENDIR2)
/* #include <sys/types.h> */
/* #include <dirent.h> */
DIR *opendir (const char *name)
{
	SBOX_MAP_PROLOGUE();
	DIR *ret;

	SBOX_MAP_PATH(name, sbox_path);
	if (next_opendir == NULL) libsb2_init();
	ret = next_opendir(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
long pathconf (const char *path, int name)
{
	SBOX_MAP_PROLOGUE();
	long ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_pathconf == NULL) libsb2_init();
	ret = next_pathconf(sbox_path, name);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <unistd.h> */
/* XXX: add proc pid/exe wrapper from libsb to here */
int readlink (const char *path, char *buf, READLINK_TYPE_ARG3)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);

	if (next_readlink == NULL) libsb2_init();
	ret = next_readlink(sbox_path, buf, bufsiz);
	if (sbox_path) free(sbox_path);
	return ret;
}

#ifdef HAVE_READLINKAT
int readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);

	if (next_readlinkat == NULL) libsb2_init();
	ret = next_readlinkat(dirfd, sbox_path, buf, bufsiz);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

/* #include <stdlib.h> */
char *realpath (const char *name, char *resolved)
{
	SBOX_MAP_PROLOGUE();
	char *ret;

	SBOX_MAP_PATH(name, sbox_path);
	if (next_realpath == NULL) libsb2_init();
	ret = next_realpath(sbox_path, resolved);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <stdio.h> */
int remove(const char *pathname)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_remove == NULL) libsb2_init();
	ret = next_remove(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_REMOVEXATTR
/* #include <sys/xattr.h> */
int removexattr(const char *path, const char *name)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_removexattr == NULL) libsb2_init();
	ret = next_removexattr(sbox_path, name);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <stdio.h> */
int rename(const char *oldpath, const char *newpath)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH(oldpath, sbox_path_old);
	SBOX_MAP_PATH(newpath, sbox_path_new);
	if (next_rename == NULL) libsb2_init();
	ret = next_rename(sbox_path_old, sbox_path_new);
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	return ret;
}


#ifdef HAVE_RENAMEAT
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH_AT(olddirfd, oldpath, sbox_path_old);
	SBOX_MAP_PATH_AT(newdirfd, newpath, sbox_path_new);

	if (next_renameat == NULL) libsb2_init();
	ret = next_renameat(olddirfd, sbox_path_old, newdirfd, sbox_path_new);
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	return ret;
}
#endif

#ifdef HAVE_REVOKE
/* #include <unistd.h> */
int revoke(const char *file)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(file, sbox_path);
	if (next_revoke == NULL) libsb2_init();
	ret = next_revoke(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


/* #include <unistd.h> */
int rmdir(const char *pathname)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_rmdir == NULL) libsb2_init();
	ret = next_rmdir(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_SCANDIR
/* #include <dirent.h> */
int scandir (const char *dir, struct dirent ***namelist, SCANDIR_TYPE_ARG3, int(*compar)(const void *, const void *))
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_scandir == NULL) libsb2_init();
	ret = next_scandir(sbox_path, namelist, filter, compar);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_SCANDIR64
/* #include <dirent.h> */
int scandir64 (const char *dir, struct dirent64 ***namelist, int(*filter)(const struct dirent64 *), int(*compar)(const void *, const void *))
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_scandir64 == NULL) libsb2_init();
	ret = next_scandir64(sbox_path, namelist, filter, compar);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_SETXATTR
/* #include <sys/xattr.h> */
int setxattr (const char *path, const char *name, const void *value, size_t size, int flags)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_setxattr == NULL) libsb2_init();
	ret = next_setxattr(sbox_path, name, value, size, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#if !defined(HAVE___XSTAT)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int stat(const char *file_name, struct stat *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(file_name, sbox_path);
	if (next_stat == NULL) libsb2_init();
	ret = next_stat(sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_STAT64
#if !defined(HAVE___XSTAT64)
/* #include <sys/stat.h> */
/* #include <unistd.h> */
int stat64(const char *file_name, struct stat64 *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(file_name, sbox_path);
	if (next_stat64 == NULL) libsb2_init();
	ret = next_stat64(sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif
#endif


/* #include <unistd.h> */
int symlink(const char *oldpath, const char *newpath)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH(oldpath, sbox_path_old);
	SBOX_MAP_PATH(newpath, sbox_path_new);

	if (next_symlink == NULL) libsb2_init();
	ret = next_symlink(sbox_path_old, sbox_path_new);
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	return ret;
}


#ifdef HAVE_SYMLINKAT
int symlinkat(const char *oldpath, int newdirfd, const char *newpath)
{
	char *sbox_path_old, *sbox_path_new;
	int ret;

	SBOX_MAP_PATH(oldpath, sbox_path_old);
	SBOX_MAP_PATH_AT(newdirfd, newpath, sbox_path_new);
	if (next_symlinkat == NULL) libsb2_init();
	ret = next_symlinkat(sbox_path_old, newdirfd, sbox_path_new);
	if (sbox_path_old) free(sbox_path_old);
	if (sbox_path_new) free(sbox_path_new);
	return ret;
}
#endif

/* #include <stdio.h> */
char *tempnam (const char *dir, const char *pfx)
{
	SBOX_MAP_PROLOGUE();
	char *ret;

	SBOX_MAP_PATH(dir, sbox_path);
	if (next_tempnam == NULL) libsb2_init();
	ret = next_tempnam(sbox_path, pfx);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <stdio.h> */
char *tmpnam (char *s)
{
	if (next_tmpnam == NULL) libsb2_init();
	return next_tmpnam(s);
}


/* #include <unistd.h> */
/* #include <sys/types.h> */
int truncate (const char *path, off_t length)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_truncate == NULL) libsb2_init();
	ret = next_truncate(sbox_path, length);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_TRUNCATE64
/* #include <unistd.h> */
/* #include <sys/types.h> */
int truncate64 (const char *path, off64_t length)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(path, sbox_path);
	if (next_truncate64 == NULL) libsb2_init();
	ret = next_truncate64(sbox_path, length);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif


#ifdef HAVE_ULCKPWDF
/* #include <shadow.h> */
int ulckpwdf (void)
{
	return 0;
}
#endif


/* #include <unistd.h> */
int unlink(const char *pathname)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(pathname, sbox_path);
	if (next_unlink == NULL) libsb2_init();
	ret = next_unlink(sbox_path);
	if (sbox_path) free(sbox_path);
	return ret;
}


#ifdef HAVE_UNLINKAT
int unlinkat(int dirfd, const char *pathname, int flags)
{
	SBOX_MAP_AT_PROLOGUE();
	int ret;

	SBOX_MAP_PATH_AT(dirfd, pathname, sbox_path);
	if (next_unlinkat == NULL) libsb2_init();
	ret = next_unlinkat(dirfd, sbox_path, flags);
	if (sbox_path) free(sbox_path);
	return ret;
}
#endif

/* #include <sys/types.h> */
/* #include <utime.h> */
int utime (const char *filename, const struct utimbuf *buf)
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_utime == NULL) libsb2_init();
	ret = next_utime(sbox_path, buf);
	if (sbox_path) free(sbox_path);
	return ret;
}


/* #include <sys/time.h> */
int utimes (const char *filename, const struct timeval tv[2])
{
	SBOX_MAP_PROLOGUE();
	int ret;

	SBOX_MAP_PATH(filename, sbox_path);
	if (next_utimes == NULL) libsb2_init();
	ret = next_utimes(sbox_path, tv);
	if (sbox_path) free(sbox_path);
	return ret;
}


int uname(struct utsname *buf)
{
	if (next_uname == NULL) libsb2_init();

	if (next_uname(buf) < 0) {
		return -1;
	}
	/* this may be called before environ is properly setup */
	if (environ) {
		strncpy(buf->machine, getenv("SBOX_UNAME_MACHINE"), sizeof(buf->machine));
	}
	return 0;
}

