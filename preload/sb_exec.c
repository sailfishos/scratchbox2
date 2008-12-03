/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1
 */

/* This file contains the "exec core" of SB2; Being able to alter
 * how the execl(), execve(), etc. exec-class functions are handled 
 * is one of the most important features of SB2.
 *
 * Brief description of the algorithm follows:
 * 
 * 0. When an application wants to execute another program, it makes a call
 *    to one of execl(), execle(), execlp(), execv(), execve() or execvp().
 *    That call will be handled by one of the gate functions in 
 *    preload/libsb2.c. Eventually, the gate function will call "do_exec()"
 *    from this file; do_exec() calls prepare_exec() and that is the place
 *    where everything interesting happens:
 *
 * 1. First, prepare_exec() calls an exec preprocessing function (implemented
 *    as Lua code, and also known as the argv&envp mangling code).
 *    The purpose of the preprocessing phase is to determine WHAT FILE needs
 *    to be executed. And that might well be something else than what the
 *    application requested: For example, an attempt to run /usr/bin/gcc might 
 *    be replaced by a path to the cross compiler (e.g. /some/path/to/cross-gcc)
 *    Arguments may also be added, deleted, or modified during preprocessing.
 *
 * 2. Second, prepare_exec() needs to determine WHERE THE FILE IS. It makes a
 *    call the regular path mapping engine of SB2 to get a real path to 
 *    the program. (For example, "/bin/ls" might be replaced by 
 *    "/opt/tools_root/bin/ls" by the path mapping engine).
 *    This step involves applying the path mapping Lua code (A notable
 *    side-effect of that is that the Lua code also returns an "execution
 *    policy object", that will be used during step 4)
 *
 * 3. Third, prepare_exec() finds out the TYPE OF THE FILE TO EXECUTE. That
 *    will be done by the inspect_binary() function in this file.
 *
 * 4. Last, prepare_exec() needs to decide HOW TO DO THE ACTUAL EXECUTION of the
 *    file. Based on the type of the file, prepare_exec() will do one of the
 *    following:
 *
 *    4a. For scripts (files starting with #!), script interpreter name 
 *        will be read from the file and it will be processed again by 
 *	  prepare_exec()
 *
 *    4b. For native binaries, an additional call to an exec postprocessing 
 *        function will be made, because the type of the file is not enough
 *        to spefify the environment that needs to be used for the file:
 *        This is the place where the "exec policy" rules (determined by step
 *        2 above) will be applied.
 *        There are at least three different cases where additional settings
 *        may need to be applied:
 *         - native binaries that are compiled for the host system can
 *           be started directly (for example, the sb2-show command that
 *           belongs to SB2's utilities)
 *         - programs from the tools_root collection may need to load
 *           dynamic libraries from a different place (e.g.
 *           "/opt/tools_root/bin/ls" may need to use libraries from
 *           "/opt/tools_root/lib", instead of using them from "/lib").
 *           This is implemented by making an explicit call to the 
 *           dynamic loader (ld.so) to start the program, with additional
 *           options for ld.so.
 *         - if the target architecture is the same as the host architecture,
 *           binaries may also need special settings. This also uses an
 *           explicit call to ld.so.
 *
 *    4c. Target binaries (when target architecture != host architecture)
 *        are started in "cpu transparency mode", which typically means
 *        that either "qemu" or "sbrsh" is used to execute them.
 *        This is also handled by exec postprocessing (like 4b. above).
 *
 * 5. When all decisions and all possible conversions have been made,
 *    prepare_exec() returns argument- and environment vectors to
 *    do_exec(), which will call sb_next_execve(). It transfers control to
 *    the real execve() funtion of the C library, which will make the
 *    system call to the kernel.
 *
 * N.B. For debugging, prepare_exec() can also be called from the
 * "sb2-show" application, so that "sb2-show" can print out how the
 * parameters are mangled without performing step 5.
 *
 * (there are some minor execptions, see the code for further details)
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>
#include <libgen.h>

#include <sys/utsname.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <config.h>
#include <sb2.h>
#include <mapping.h>
#include <elf.h>

#include "libsb2.h"
#include "exported.h"

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(array) (sizeof (array) / sizeof ((array)[0]))
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
# define HOST_ELF_DATA ELFDATA2MSB
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define HOST_ELF_DATA ELFDATA2LSB
#else
# error Invalid __BYTE_ORDER
#endif

#ifdef __i386__
# define HOST_ELF_MACHINE EM_386
#elif defined(__x86_64__)
# define HOST_ELF_MACHINE EM_X86_64
#elif defined(__ia64__)
# define HOST_ELF_MACHINE EM_IA_64
#elif defined(__powerpc__)
# define HOST_ELF_MACHINE EM_PPC
#else
# error Unsupported host CPU architecture
#endif

#ifndef PAGE_MASK
# define PAGE_MASK sysconf(_SC_PAGE_SIZE)
#endif

#ifdef __x86_64__
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
#else
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
#endif

struct target_info {
	char name[8];
	uint16_t machine;
	unsigned int default_byteorder;
	int multi_byteorder;
};

static const struct target_info target_table[] = {
	{ "arm",	EM_ARM,		ELFDATA2LSB,	1 },
	{ "mips",	EM_MIPS,	ELFDATA2MSB,	1 },
	{ "ppc",	EM_PPC,		ELFDATA2MSB,	0 },
	{ "sh",		EM_SH, 		ELFDATA2LSB,	1 },
};

static int elf_hdr_match(Elf_Ehdr *ehdr, uint16_t match, int ei_data);

static int prepare_exec(const char *exec_fn_name,
	const char *orig_file, char *const *orig_argv, char *const *orig_envp,
	char **new_file, char ***new_argv, char ***new_envp);

static uint16_t byte_swap(uint16_t a)
{
	uint16_t b;
	uint8_t *r;
	uint8_t *t;
	t = (uint8_t *)&a;
	r = (uint8_t *)&b;
	*r++ = *(t + 1);
	*r = *t;
	return b;
}

enum binary_type {
	BIN_NONE, 
	BIN_UNKNOWN,
	BIN_INVALID,
	BIN_HOST_STATIC,
	BIN_HOST_DYNAMIC,
	BIN_TARGET,
	BIN_HASHBANG,
};

#if 0
static int is_subdir(const char *root, const char *subdir)
{
	size_t rootlen;

	if (strstr(subdir, root) != subdir)
		return 0;

	rootlen = strlen(root);
	return subdir[rootlen] == '/' || subdir[rootlen] == '\0';
}
#endif

static int elem_count(char *const *elems)
{
	int count = 0;
	char **p = (char **)elems;
	while (*p) {
		p++; count++;
	}
	return count;
}

int token_count(char *str);
char **split_to_tokens(char *str);

int token_count(char *str)
{
	char *p;
	int count = 0;

	for (p = str; *p; ) {
		if (!isspace(*p)) {
			count++;
			while (*p && !isspace(*p))
				p++;
		} else {
			while (*p && isspace(*p))
				p++;
		}
	}

	return count;
}

char **split_to_tokens(char *str)
{
	int c, i, len;
	char **tokens, *start, *end;

	c = token_count(str);
	tokens = calloc(c + 1, sizeof(char *));
	i = 0;
	for (start = str; *start; start++) {
		if (isspace(*start))
			continue;
		end = start;
		while (*end && !isspace(*end))
			end++;
		len = end - start;
		tokens[i] = malloc(sizeof(char) * (len + 1));
		strncpy(tokens[i], start, len);
		tokens[i][len] = '\0';
		start = end - 1;
		i++;
	}
	tokens[i] = NULL;
	return tokens;
}

static int elf_hdr_match(Elf_Ehdr *ehdr, uint16_t match, int ei_data)
{
	int swap;

	if (ehdr->e_ident[EI_DATA] != ei_data)
		return 0;

#ifdef WORDS_BIGENDIAN
	swap = (ei_data == ELFDATA2LSB);
#else
	swap = (ei_data == ELFDATA2MSB);
#endif

	if (swap && ehdr->e_machine == byte_swap(match))
		return 1;

	if (!swap && ehdr->e_machine == match)
		return 1;

	return 0;
}

static enum binary_type inspect_binary(const char *filename)
{
	enum binary_type retval;
	int fd, phnum, j;
	struct stat status;
	char *region;
	unsigned int ph_base, ph_frag, ei_data;
	uint16_t e_machine;
#ifdef __x86_64__
	int64_t reloc0;
#else
	int reloc0;
#endif
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;

	retval = BIN_NONE; /* assume it doesn't exist, until proven otherwise */
	if (access_nomap_nolog(filename, X_OK) < 0) {
		int saved_errno = errno;
		char *sb1_bug_emulation_mode =
			sb2__read_string_variable_from_lua__(
				"sbox_emulate_sb1_bugs");

		if (access_nomap_nolog(filename, F_OK) < 0) {
			/* file is missing completely, or can't be accessed
			 * at all.
			 * errno has been set */
			goto _out;
		}

		if (sb1_bug_emulation_mode && 
		    strchr(sb1_bug_emulation_mode,'x')) {
			/* the old scratchbox didn't have the x-bit check, so 
			 * having R-bit set was enough to exec a file. That is 
			 * of course wrong, but unfortunately there are now 
			 * lots of packages out there that have various scripts 
			 * with wrong permissions.
			 * We'll provide a compatibility mode, so that broken 
			 * packages can be built.
			 *
			 * an 'x' in the env.var. means that we should not 
			 * worry about x-bits when checking exec parmissions
			*/
			if (access_nomap_nolog(filename, R_OK) < 0) {
				saved_errno = errno;
				SB_LOG(SB_LOGLEVEL_DEBUG, "no X or R "
					"permission for '%s'", filename);
				retval = BIN_INVALID;
				errno = saved_errno;
				goto _out;
			}
			SB_LOG(SB_LOGLEVEL_WARNING, 
				"X permission missing, but exec enabled by"
				" SB1 bug emulation mode ('%s')", filename);
		} else {
			/* The normal case:
			 * can't execute it. Possible errno codes from access() 
			 * are all possible from execve(), too, so there is no
			 * need to convert errno.
			*/
			SB_LOG(SB_LOGLEVEL_DEBUG, "no X permission for '%s'",
				filename);
			retval = BIN_INVALID;
			errno = saved_errno;
			goto _out;
		}
	}

	fd = open_nomap_nolog(filename, O_RDONLY, 0);
	if (fd < 0) {
		retval = BIN_HOST_DYNAMIC; /* can't peek in to look, assume dynamic */
		goto _out;
	}

	retval = BIN_UNKNOWN;

	if (fstat(fd, &status) < 0) {
		goto _out_close;
	}

	if ((status.st_mode & S_ISUID)) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"SUID bit set for '%s' (SB2 may be disabled)",
			filename);
	}

	if ((status.st_mode & S_ISGID)) {
		SB_LOG(SB_LOGLEVEL_WARNING,
			"SGID bit set for '%s' (SB2 may be disabled)",
			filename);
	}

	if (!S_ISREG(status.st_mode) && !S_ISLNK(status.st_mode)) {
		goto _out_close;
	}

	if (status.st_size < 4) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"File size is too small, can't exec (%s)", filename);
		errno = ENOEXEC;
		retval = BIN_NONE;
		goto _out_close;
	}

	region = mmap(0, status.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!region) {
		goto _out_close;
	}

	/* check for hashbang */

	if (region[0] == '#' && region[1] == '!') {
		retval = BIN_HASHBANG;
		goto _out_munmap;
	}

	ehdr = (Elf_Ehdr *) region;

#ifdef __x86_64__
	if (elf_hdr_match(ehdr, EM_386, HOST_ELF_DATA) ||
		elf_hdr_match(ehdr, EM_X86_64, HOST_ELF_DATA)) {
#else
	if (elf_hdr_match(ehdr, HOST_ELF_MACHINE, HOST_ELF_DATA)) {
#endif
		retval = BIN_HOST_STATIC;

		phnum = ehdr->e_phnum;
		reloc0 = ~0;
		ph_base = ehdr->e_phoff & PAGE_MASK;
		ph_frag = ehdr->e_phoff - ph_base;

		phdr = (Elf_Phdr *) (region + ph_base + ph_frag);

		for (j = phnum; --j >= 0; ++phdr)
			if (PT_LOAD == phdr->p_type && ~0 == reloc0)
				reloc0 = phdr->p_vaddr - phdr->p_offset;

		phdr -= phnum;

		for (j = phnum; --j >= 0; ++phdr) {
			if (PT_DYNAMIC != phdr->p_type)
				continue;

			retval = BIN_HOST_DYNAMIC;
		}
	} else {
		static char *target_cpu = NULL;

		if (!target_cpu) {
			target_cpu = sb2__read_string_variable_from_lua__(
				"sbox_cpu");

			if (!target_cpu)
				target_cpu = "arm";
		}

		ei_data = ELFDATANONE;
		e_machine = EM_NONE;

		for (j = 0; (size_t) j < ARRAY_SIZE(target_table); j++) {
			const struct target_info *ti = &target_table[j];

			if (strncmp(target_cpu, ti->name, strlen(ti->name)))
				continue;

			ei_data = ti->default_byteorder;
			e_machine = ti->machine;

			if (ti->multi_byteorder &&
			    strlen(target_cpu) >= strlen(ti->name) + 2) {
				size_t len = strlen(target_cpu);
				const char *tail = target_cpu + len - 2;

				if (strcmp(tail, "eb") == 0)
					ei_data = ELFDATA2MSB;
				else if (strcmp(tail, "el") == 0)
					ei_data = ELFDATA2LSB;
			}

			break;
		}

		if (elf_hdr_match(ehdr, e_machine, ei_data))
			retval = BIN_TARGET;
	}

_out_munmap:
	munmap(region, status.st_size);
_out_close:
	close(fd);
_out:
	return retval;
}

static int prepare_hashbang(
	char **mapped_file,
	char *orig_file,
	char ***argvp,
	char ***envpp)
{
	int argc, fd, c, i, j, n;
	char ch;
	char *ptr, *mapped_interpreter;
	char **new_argv;
	char hashbang[SBOX_MAXPATH]; /* only 60 needed on linux, just be safe */
	char interpreter[SBOX_MAXPATH];

	if ((fd = open_nomap(*mapped_file, O_RDONLY)) < 0) {
		/* unexpected error, just run it */
		return 0;
	}

	if ((c = read(fd, &hashbang[0], SBOX_MAXPATH - 1)) < 2) {
		/* again unexpected error, close fd and run it */
		close(fd);
		return 0;
	}

	argc = elem_count(*argvp);

	/* extra element for hashbang argument */
	new_argv = calloc(argc + 3, sizeof(char *));

	/* skip any initial whitespace following "#!" */
	for (i = 2; (hashbang[i] == ' ' 
			|| hashbang[i] == '\t') && i < c; i++)
		;

	for (n = 0, j = i; i < c; i++) {
		ch = hashbang[i];
		if (hashbang[i] == 0
			|| hashbang[i] == ' '
			|| hashbang[i] == '\t'
			|| hashbang[i] == '\n') {
			hashbang[i] = 0;
			if (i > j) {
				if (n == 0) {
					ptr = &hashbang[j];
					strcpy(interpreter, ptr);
					new_argv[n++] = strdup(interpreter);
				} else {
					new_argv[n++] = strdup(&hashbang[j]);
					/* this was the one and only
					 * allowed argument for the
					 * interpreter
					 */
					break;
				}
			}
			j = i + 1;
		}
		if (ch == '\n' || ch == 0) break;
	}

	mapped_interpreter = scratchbox_path("execve", interpreter, 
		NULL/*RO-flag addr.*/, 0/*dont_resolve_final_symlink*/);
	SB_LOG(SB_LOGLEVEL_DEBUG, "prepare_hashbang(): interpreter=%s,"
			"mapped_interpreter=%s", interpreter,
			mapped_interpreter);
	new_argv[n++] = strdup(orig_file); /* the unmapped script path */

	for (i = 1; (*argvp)[i] != NULL && i < argc; ) {
		new_argv[n++] = (*argvp)[i++];
	}

	new_argv[n] = NULL;

	/* feed this through prepare_exec to let it deal with
	 * cpu transparency etc.
	 */
	return prepare_exec("run_hashbang", mapped_interpreter,
		new_argv, *envpp, mapped_file, argvp, envpp);
}

static char **duplicate_argv(char *const *argv)
{
	int	argc = elem_count(argv);
	char	**p;
	int	i;
	char	**my_argv;

	my_argv = (char **)calloc(argc + 1, sizeof(char *));
	for (i = 0, p = (char **)argv; *p; p++) {
		my_argv[i++] = strdup(*p);
	}
	my_argv[i] = NULL;

	return(my_argv);
}

static char **prepare_envp_for_do_exec(char *binaryname, char *const *envp)
{
	char	**p;
	int	envc = 0;
	char	**my_envp;
	int	has_ld_preload = 0;
	int	has_ld_library_path = 0;
	int	i;
	char	*new_binaryname_var;
	int	has_sbox_session_dir = 0;
	int	has_sbox_session_mode = 0;
	const int sbox_session_dir_varname_len = strlen("SBOX_SESSION_DIR");
	const int sbox_session_varname_prefix_len = strlen("SBOX_SESSION_");

	/* FIXME: This routine checks that we have LD_PRELOAD and
	 * LD_LIBRARY_PATH, but doesn't check contents of those, so it is
	 * still possible to get out from sb2 by accident if someone modifies
	 * those variables and leaves sb2's settings out. But the most common
	 * problem which was caused by completely missing variables has
	 * been solved now.
	 * 
	 * SBOX_SESSION_* is now preserved properly (these are practically
	 * read-only variables now)
	*/
	
	/* if we have LD_PRELOAD env var set, make sure the new my_envp
	 * has it as well
	 */

	/* count the environment variables and arguments, also check
	 * for LD_PRELOAD, LD_LIBRARY_PATH and SBOX_SESSION_*
	 */
	for (p=(char **)envp, envc=0; *p; p++, envc++) {
		if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) == 0) {
			has_ld_preload = 1;
			continue;
		}
		if (strncmp("LD_LIBRARY_PATH=", *p, strlen("LD_LIBRARY_PATH=")) == 0) {
			has_ld_library_path = 1;
			continue;
		}
		if (strncmp("SBOX_SESSION_DIR=", *p,
		     sbox_session_dir_varname_len+1) == 0) {
			has_sbox_session_dir = 1;
			if (strcmp(*p+sbox_session_dir_varname_len+1,
				sbox_session_dir)) {
					SB_LOG(SB_LOGLEVEL_WARNING, 
						"Detected attempt to set %s,"
						" restored to %s",
						*p, sbox_session_dir);
			}
			continue;
		}
	}
	if (!has_sbox_session_dir) {
		SB_LOG(SB_LOGLEVEL_WARNING, 
			"Detected attempt to clear SBOX_SESSION_DIR, "
				"restored to %s", sbox_session_dir);
	}

	/* allocate new environment. Add 7 extra elements (all may not be
	 * needed always) */
	my_envp = (char **)calloc(envc + 7, sizeof(char *));

	for (i = 0, p=(char **)envp; *p; p++) {
		if (strncmp(*p, "__SB2_BINARYNAME=",
				strlen("__SB2_BINARYNAME=")) == 0) {
			/* this is current process' name, skip it */
			continue;
		}
		if (strncmp(*p, "__SB2_REAL_BINARYNAME=",
				strlen("__SB2_REAL_BINARYNAME=")) == 0) {
			/* skip current process' real binary name */
			continue;
		}
		if (strncmp(*p, "SBOX_SESSION_MODE=",
				sbox_session_varname_prefix_len+5) == 0) {
			/* user-provided SBOX_SESSION_MODE */
			char *requested_mode = *p +
				sbox_session_varname_prefix_len+5;
			char *rulefile = NULL;

			if (sbox_session_mode &&
			    (strcmp(requested_mode, sbox_session_mode) == 0)) {
				/* same as current mode - skip it */
				continue;
			}

			if (asprintf(&rulefile, "%s/rules/%s.lua",
				sbox_session_dir, requested_mode) < 0) {

				SB_LOG(SB_LOGLEVEL_ERROR,
					"asprintf failed to create path to rulefile");
				continue;
			}

			if (access_nomap_nolog(rulefile, R_OK) == 0) {
				SB_LOG(SB_LOGLEVEL_DEBUG,
					"Accepted requested mode change to '%s'",
					requested_mode);
				has_sbox_session_mode = 1;
			}
			free(rulefile);
			if (has_sbox_session_mode == 0) continue;
		} else if (strncmp(*p, "SBOX_SESSION_",
				sbox_session_varname_prefix_len) == 0) {
			/* this is user-provided SBOX_SESSION_*, skip it. */
			continue;
		}
		my_envp[i++] = strdup(*p);
	}

	/* add our session directory */
	if (asprintf(&(my_envp[i]), "SBOX_SESSION_DIR=%s", sbox_session_dir) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"asprintf failed to create SBOX_SESSION_DIR");
	} else {
		i++;
	}

	/* add mode, if not using the default mode */
	if (sbox_session_mode && (has_sbox_session_mode==0)) {
		if (asprintf(&(my_envp[i]), "SBOX_SESSION_MODE=%s",
		     sbox_session_mode) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create SBOX_SESSION_MODE");
		} else {
			i++;
		}
	}

	/* add permission token (optional) */
	if (sbox_session_perm) {
		if (asprintf(&(my_envp[i]), "SBOX_SESSION_PERM=%s",
		     sbox_session_perm) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create SBOX_SESSION_PERM");
		} else {
			i++;
		}
	}

	/* __SB2_BINARYNAME is used to communicate the binary name
	 * to the new process so that it's available even before
	 * its main function is called
	 */
	if (asprintf(&new_binaryname_var, "__SB2_BINARYNAME=%s", binaryname) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"asprintf failed to create __SB2_BINARYNAME");
	}
	my_envp[i++] = new_binaryname_var; /* add the new process' name */

	/* allocate slot for __SB2_REAL_BINARYNAME that is filled later on */
	my_envp[i++] = strdup("__SB2_REAL_BINARYNAME=");

	/* If our environ has LD_PRELOAD, but the given envp doesn't,
	 * add the value that was active when this process was started.
	 */
	if (!has_ld_preload && sbox_orig_ld_preload) {
		char *new_ld_preload_var;

		if (asprintf(&new_ld_preload_var, "LD_PRELOAD=%s",
			sbox_orig_ld_preload) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create LD_PRELOAD");
		}
		if (!new_ld_preload_var)
			exit(1);
		my_envp[i++] = new_ld_preload_var;
	}
	/* If our environ has LD_LIBRARY_PATH, but the given envp doesn't,
	 * add the value that was active when this process was started.
	 */
	if (!has_ld_library_path && sbox_orig_ld_library_path) {
		char *new_ld_library_path;

		if (asprintf(&new_ld_library_path, "LD_LIBRARY_PATH=%s",
			sbox_orig_ld_library_path) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create LD_LIBRARY_PATH");
		}
		if (!new_ld_library_path)
			exit(1);
		my_envp[i++] = new_ld_library_path;
	}
	my_envp[i] = NULL;

	return(my_envp);
}

/* compare vectors of strings and log if there are any changes.
 * TO BE IMPROVED: a more clever algorithm would be nice, something
 * that would log only the modified parts... now this displays 
 * everything if something was changed, which easily creates 
 * a *lot* of noise if SB_LOGLEVEL_NOISE is enabled.
*/
static void compare_and_log_strvec_changes(const char *vecname, 
	char *const *orig_strv, char *const *new_strv) 
{
	char *const *ptr2old = orig_strv;
	char *const *ptr2new = new_strv;
	int	strv_modified = 0;

	while (*ptr2old && *ptr2new) {
		if (strcmp(*ptr2old, *ptr2new)) {
			strv_modified = 1;
			break;
		}
		ptr2old++, ptr2new++;
	}
	if (!strv_modified && (*ptr2old || *ptr2new)) {
		strv_modified = 1;
	}
	if (strv_modified) {
		SB_LOG(SB_LOGLEVEL_DEBUG, "%s[] was modified", vecname); 
		if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_NOISE)) {
			int	i;
			for (i = 0, ptr2new = new_strv; 
			    *ptr2new;
			    i++, ptr2new++) {
				SB_LOG(SB_LOGLEVEL_NOISE, 
					"[%d]='%s'", i,
					*ptr2new);
			}
		}
	} else {
		SB_LOG(SB_LOGLEVEL_NOISE, 
			"%s[] was not modified", vecname); 
	}
}

static int strvec_contains(char *const *strvec, const char *id,
    size_t *index)
{
	int i;

	for (i = 0; strvec[i] != NULL; i++) {
		if (strcmp(strvec[i], id) == 0) {
			if (index != NULL)
				*index = i;
			return (1);
		}
	}
	return (0);
}


static int prepare_exec(const char *exec_fn_name,
	const char *orig_file,
	char *const *orig_argv,
	char *const *orig_envp,
	char **new_file,
	char ***new_argv,
	char ***new_envp)
{
	char **my_envp, **my_argv, *my_file;
	char **my_envp_copy = NULL; /* used only for debug log */
	char *binaryname, *tmp, *mapped_file;
	int err = 0;
	enum binary_type type;
	int postprocess_result = 0;
	int ret = 0; /* 0: ok to exec, ret<0: exec fails */
	size_t idx;

	(void)exec_fn_name; /* not yet used */

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* just run it, don't worry, be happy! */
		*new_file = NULL;
		*new_argv = NULL;
		*new_envp = NULL;
		return(0);
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		char *buf = strvec_to_string(orig_argv);

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"EXEC/Orig.args: %s : %s", orig_file, buf);
		free(buf);
	}
	
	tmp = strdup(orig_file);
	binaryname = strdup(basename(tmp));
	free(tmp);
	
	my_file = strdup(orig_file);

	my_envp = prepare_envp_for_do_exec(binaryname, orig_envp);
	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		/* create a copy of intended environment for logging,
		 * before sb_execve_preprocess() gets control */ 
		my_envp_copy = prepare_envp_for_do_exec(binaryname, orig_envp);
	}

	my_argv = duplicate_argv(orig_argv);

	if ((err = sb_execve_preprocess(&my_file, &my_argv, &my_envp)) != 0) {
		SB_LOG(SB_LOGLEVEL_ERROR, "argvenvp processing error %i", err);
	}

	if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
		/* find out and log if sb_execve_preprocess() did something */
		compare_and_log_strvec_changes("argv", orig_argv, my_argv);
		compare_and_log_strvec_changes("envp", my_envp_copy, my_envp);
	}

	/* test if mapping is enabled during the exec()..
	 * (host-* tools disable it)
	*/
	if (strvec_contains(my_envp, "SBOX_DISABLE_MAPPING=1", NULL)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"do_exec(): mapping disabled, my_file = %s", my_file);
		mapped_file = strdup(my_file);

		/* we won't call scratchbox_path_for_exec() because mapping
		 * is disabled; instead we must push a string to Lua's stack
		 * which explains the situation to the Lua code
		*/
		sb_push_string_to_lua_stack("no mapping rule (mapping disabled)");
		sb_push_string_to_lua_stack("no exec_policy (mapping disabled)");
	} else {
		/* now we have to do path mapping for my_file to find exactly
		 * what is the path we're supposed to deal with
		 */

		mapped_file = scratchbox_path_for_exec("do_exec", my_file,
			NULL/*RO-flag addr.*/, 0/*dont_resolve_final_symlink*/);
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"do_exec(): my_file = %s, mapped_file = %s",
			my_file, mapped_file);

		/* Note: the Lua stack should now have rule and policy objects */
	}

	/*
	 * prepare_envp_for_do_exec() left us placeholder in envp array
	 * that we will fill now with fully mangled binary name.
	 */
	if (strvec_contains(my_envp, "__SB2_REAL_BINARYNAME=", &idx)) {
		char *real_binaryname, *orig_real_binaryname;

		/* release the placeholder */
		orig_real_binaryname = my_envp[idx];
		free(orig_real_binaryname);

		/*
		 * Append fully mangled binary name to exec'd process
		 * environment with name __SB2_REAL_BINARYNAME.
		 */
		if (asprintf(&real_binaryname, "__SB2_REAL_BINARYNAME=%s",
		    mapped_file) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create __SB2_REAL_BINARYNAME");
		} else {
			my_envp[idx] = real_binaryname;
		}

		SB_LOG(SB_LOGLEVEL_DEBUG, "setting %s", real_binaryname);
	}

	/* inspect the completely mangled filename */
	type = inspect_binary(mapped_file);

	switch (type) {
		case BIN_HASHBANG:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/hashbang %s", mapped_file);
			/* prepare_hashbang() will call prepare_exec()
			 * recursively */
			ret = prepare_hashbang(&mapped_file, my_file,
					&my_argv, &my_envp);
			break;

		case BIN_HOST_DYNAMIC:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/host-dynamic %s",
					mapped_file);

			postprocess_result = sb_execve_postprocess("native",
				&mapped_file, &my_file, binaryname,
				&my_argv, &my_envp);

			if (postprocess_result < 0) {
				errno = EINVAL;
				ret = -1;
			}
			break;

		case BIN_HOST_STATIC:
			SB_LOG(SB_LOGLEVEL_WARNING, "Executing statically "
					"linked native binary %s",
					mapped_file);
			break;

		case BIN_TARGET:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/target %s",
					mapped_file);

			postprocess_result = sb_execve_postprocess(
				"cpu_transparency", &mapped_file, &my_file,
				binaryname, &my_argv, &my_envp);

			if (postprocess_result < 0) {
				errno = EINVAL;
				ret = -1;
			}
			break;

		case BIN_INVALID: /* = can't be executed, no X permission */
	 		/* don't even try to exec, errno has been set.*/
			ret = -1;
			break;

		case BIN_NONE:
			/* file does not exist. errno has been set. */
			ret = -1;
			break;

		case BIN_UNKNOWN:
			SB_LOG(SB_LOGLEVEL_ERROR,
				"Unidentified executable detected (%s)",
				mapped_file);
			break;
	}

	*new_file = mapped_file;
	*new_argv = my_argv;
	*new_envp = my_envp;
	return(ret);
}

int do_exec(const char *exec_fn_name, const char *orig_file,
		char *const *orig_argv, char *const *orig_envp)
{
	int	r;
	char *new_file = NULL;
	char **new_argv = NULL;
	char **new_envp = NULL;

	r = prepare_exec(exec_fn_name, orig_file, orig_argv, orig_envp,
		&new_file, &new_argv, &new_envp);

	if (r < 0) return(r); /* exec denied */

	return sb_next_execve(
		(new_file ? new_file : orig_file),
		(new_argv ? new_argv : orig_argv),
		(new_envp ? new_envp : orig_envp));
}

/* ----- EXPORTED from interface.master: ----- */
int sb2show__execve_mods__(
	char *file,
	char *const *orig_argv, char *const *orig_envp,
	char **new_file, char ***new_argv, char ***new_envp)
{
	int	ret = 0;

	if (!sb2_global_vars_initialized__) sb2_initialize_global_variables();

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s '%s'", __func__, orig_argv[0]);

	ret = prepare_exec("sb2show_exec", file, orig_argv, orig_envp,
		new_file, new_argv, new_envp);

	if (!*new_file) *new_file = strdup(file);
	if (!*new_argv) *new_argv = duplicate_argv(orig_argv);
	if (!*new_envp) *new_envp = duplicate_argv(orig_envp);

	return(ret);
}

/* "filename" is the real path to a file!
 * returns an allocated buffer, so that some day we could
 * return more information about the binary in the same
 * buffer.
*/
char *sb2show__binary_type__(const char *filename)
{
	enum binary_type type = inspect_binary(filename);
	char *result = NULL;

	switch (type) {
	case BIN_HASHBANG:
		result = "script"; break;
	case BIN_HOST_DYNAMIC:
		result = "host/dynamic"; break;
	case BIN_HOST_STATIC:
		result = "host/static"; break;
	case BIN_TARGET:
		result = "target"; break;
	case BIN_INVALID: /* = can't be executed, no X permission */
		result = "invalid"; break;
	case BIN_NONE:
		result = "none"; break;
	case BIN_UNKNOWN:
		result = "unknown"; break;
	default:
		/* this should not happen! */
		result = "internal error"; break;
	}
	return(strdup(result));
}

