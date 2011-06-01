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
 *        will be read from the file, mapped by sb_execve_map_script_interpreter()
 *	  (a lua function in argvenvp.lua), and once the interpreter
 *	  location has been found, the parameters will be processed again by 
 *	  prepare_exec().
 *        This solution supports location- and exec policy based mapping
 *	  of the script interpreter.
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

#if defined(__i386__) || defined(__x86_64__)
/*
 * We support exec'ing 64-bit programs from 32-bit programs
 * as tools distribution might be 32-bit even in 64-bit machine.
 */
# define HOST_ELF_MACHINE_32 EM_386
# define HOST_ELF_MACHINE_64 EM_X86_64
#elif defined(__ia64__)
# define HOST_ELF_MACHINE_64 EM_IA_64
#elif defined(__powerpc__)
# define HOST_ELF_MACHINE_32 EM_PPC
#else
# error Unsupported host CPU architecture
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

static int elf_hdr_match(const char *region, uint16_t match, int ei_data);
static enum binary_type inspect_elf_binary(const char *);

static int prepare_exec(const char *exec_fn_name,
	char *exec_policy_name,
	const char *orig_file, int file_has_been_mapped,
	char *const *orig_argv, char *const *orig_envp,
	enum binary_type *typep,
	char **new_file, char ***new_argv, char ***new_envp);

static void change_environment_variable(
	char **my_envp, const char *var_prefix, const char *new_value);

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

static int elf_hdr_match(const char *region, uint16_t match, int ei_data)
{
	/*
	 * It is OK to use Elf32_Ehdr here because fields accessed
	 * in this function are same in both 64-bit and 32-bit ELF formats.
	 */
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)region;
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

static enum binary_type inspect_elf_binary(const char *region)
{
	assert(region != NULL);

	/* check for hashbang */
	if (region[EI_MAG0] == '#' && region[EI_MAG1] == '!')
		return (BIN_HASHBANG);

	/*
	 * We go through ELF program headers one by one and check
	 * whether there is interpreter (PT_INTERP) section.
	 * In that case this is dynamically linked, otherwise
	 * it is statically linked.
	 */
#ifdef HOST_ELF_MACHINE_32
	if (elf_hdr_match(region, HOST_ELF_MACHINE_32, HOST_ELF_DATA)) {
		Elf32_Ehdr *eh = (Elf32_Ehdr *)region;
		Elf32_Phdr *ph;
		size_t ph_entsize = eh->e_phentsize;
		int i;

		for (i = 0; i < eh->e_phnum; i++) {
			ph = (Elf32_Phdr *)
			    (region + eh->e_phoff + (i * ph_entsize));
			if (ph->p_type == PT_INTERP)
				return (BIN_HOST_DYNAMIC);
		}
		return (BIN_HOST_STATIC);
	}
#endif
#ifdef HOST_ELF_MACHINE_64
	if (elf_hdr_match(region, HOST_ELF_MACHINE_64, HOST_ELF_DATA)) {
		Elf64_Ehdr *eh = (Elf64_Ehdr *)region;
		Elf64_Phdr *ph;
		size_t ph_entsize = eh->e_phentsize;
		int i;

		for (i = 0; i < eh->e_phnum; i++) {
			ph = (Elf64_Phdr *)
			    (region + eh->e_phoff + (i * ph_entsize));
			if (ph->p_type == PT_INTERP)
				return (BIN_HOST_DYNAMIC);
		}
		return (BIN_HOST_STATIC);
	}
#endif
	/* could not identify as host binary */
	return (BIN_UNKNOWN);
}

static enum binary_type inspect_binary(const char *filename, int check_x_permission)
{
	static char *target_cpu = NULL;
	enum binary_type retval;
	int fd, j;
	struct stat status;
	char *region;
	unsigned int ei_data;
	uint16_t e_machine;

	retval = BIN_NONE; /* assume it doesn't exist, until proven otherwise */
	if (check_x_permission && access_nomap_nolog(filename, X_OK) < 0) {
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
	} else if(!check_x_permission) {
		if (access_nomap_nolog(filename, F_OK) < 0) {
			/* file is missing completely, or can't be accessed
			 * at all.
			 * errno has been set */
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

	retval = inspect_elf_binary(region);
	switch (retval) {
	case BIN_HASHBANG:
	case BIN_HOST_STATIC:
	case BIN_HOST_DYNAMIC:
		/* host binary, lets go out of here */
		goto _out_munmap;

	default:
		break;
	}

	/*
	 * Target binary.  Find out whether it is supported
	 * by scratchbox2.
	 */
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

	if (elf_hdr_match(region, e_machine, ei_data))
		retval = BIN_TARGET;

_out_munmap:
	munmap(region, status.st_size);
_out_close:
	close_nomap_nolog(fd);
_out:
	return retval;
}

static int prepare_hashbang(
	char **mapped_file,	/* In: script, out: mapped script interpreter */
	char *orig_file,
	char ***argvp,
	char ***envpp,
	char *exec_policy_name)
{
	int argc, fd, c, i, j, n;
	char ch;
	char *ptr, *mapped_interpreter;
	char **new_argv;
	char hashbang[SBOX_MAXPATH]; /* only 60 needed on linux, just be safe */
	char interpreter[SBOX_MAXPATH];
	char *interp_arg = NULL;
	char *tmp, *mapped_binaryname;
	int result = 0;
	char *nep;

	if ((fd = open_nomap(*mapped_file, O_RDONLY)) < 0) {
		/* unexpected error, just run it */
		return 0;
	}

	if ((c = read(fd, &hashbang[0], SBOX_MAXPATH - 1)) < 2) {
		/* again unexpected error, close fd and run it */
		close_nomap_nolog(fd);
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
					/* this was the one and only
					 * allowed argument for the
					 * interpreter
					 */
					interp_arg = strdup(&hashbang[j]);
					new_argv[n++] = interp_arg;
					break;
				}
			}
			j = i + 1;
		}
		if (ch == '\n' || ch == 0) break;
	}

	new_argv[n++] = strdup(orig_file); /* the unmapped script path */
	for (i = 1; (*argvp)[i] != NULL && i < argc; ) {
		new_argv[n++] = (*argvp)[i++];
	}
	new_argv[n] = NULL;

	/* Now we need to update __SB2_ORIG_BINARYNAME to point to 
	 * the unmapped script interpreter (sb_execve_map_script_interpreter
	 * may change it again (not currently, but in the future)
	*/
	change_environment_variable(
		*envpp, "__SB2_ORIG_BINARYNAME=", interpreter);

	/* rule & policy are in the stack */
	mapped_interpreter = sb_execve_map_script_interpreter(
		exec_policy_name,
		interpreter, interp_arg, *mapped_file, orig_file,
		&new_argv, envpp, &nep);

	if (!mapped_interpreter) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"failed to map script interpreter=%s", interpreter);
		return(-1);
	}

	exec_policy_name = nep;

	/*
	 * Binaryname (the one expected by the rules) comes still from
	 * the interpreter name so we set it here.  Note that it is now
	 * basename of the mapped interpreter (not the original one)!
	 */
	tmp = strdup(mapped_interpreter);
	mapped_binaryname = strdup(basename(tmp));
	change_environment_variable(*envpp, "__SB2_BINARYNAME=",
	    mapped_binaryname);
	free(mapped_binaryname);
	free(tmp);
	
	SB_LOG(SB_LOGLEVEL_DEBUG, "prepare_hashbang(): interpreter=%s,"
			"mapped_interpreter=%s", interpreter,
			mapped_interpreter);

	/* feed this through prepare_exec to let it deal with
	 * cpu transparency etc.
	 */
	result = prepare_exec("run_hashbang",
		exec_policy_name,
		mapped_interpreter,
		1/*file_has_been_mapped, and rue&policy exist*/,
		new_argv, *envpp,
		(enum binary_type*)NULL,
		mapped_file, argvp, envpp);

	SB_LOG(SB_LOGLEVEL_DEBUG, "prepare_hashbang done: mapped_file='%s'",
			*mapped_file);

	return(result);
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

static int check_envp_has_ld_preload_and_ld_library_path(
	char *const *envp)
{
	int	has_ld_preload = 0;
	int	has_ld_library_path = 0;
	char	**p;

	for (p=(char **)envp; *p; p++) {
		if (**p == 'L') {
			if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) == 0) {
				has_ld_preload = 1;
			} else if (strncmp("LD_LIBRARY_PATH=", *p, strlen("LD_LIBRARY_PATH=")) == 0) {
				has_ld_library_path = 1;
			}
		}
	}
	return (has_ld_preload && has_ld_library_path);
}

/* Prepare environment vector for do_exec() and other
 * pieces of exec*() processing: This will add/check SB2's
 * private variables and rename some user-specified
 * variables:
 * - LD_LIBRARY_PATH is expected to be the user's version.
 *   contents of it will be moved to __SB2_LD_LIBRARY_PATH.
 *   the new environment returned by this function does
 *   not have LD_LIBRARY_PATH at all.
 * - LD_PRELOAD gets the same treatment as LD_LIBRARY_PATH
 * - SBOX_SESSION_DIR can not be changed or removed
 *
 * N.B The lua scripts will set LD_LIBRARY_PATH and
 *    LD_PRELOAD to the actual values that should be active
 *    during the real exec; sb2's initialization code will
 *    restore the __SB2__LD... variables to the real variables.
 * N.B2. Proper LD_LIBRARY_PATH and LD_PRELOAD *must* be set
 *    by the Lua-based exec logic (argvenvp.lua), otherwise
 *    prepare_exec() will deny the exec.
*/
static char **prepare_envp_for_do_exec(const char *orig_file,
	const char *binaryname, char *const *envp)
{
	char	**p;
	int	envc = 0;
	char	**my_envp;
	char	*user_ld_preload = NULL;
	char	*user_ld_library_path = NULL;
	int	i;
	char	*new_binaryname_var;
	char	*new_orig_file_var;
	char	*new_exec_file_var;
	int	has_sbox_session_dir = 0;
	int	has_sbox_session_mode = 0;
	int     has_sbox_sigtrap = 0;
	const int sbox_session_dir_varname_len = strlen("SBOX_SESSION_DIR");
	const int sbox_sigtrap_varname_len = strlen("SBOX_SIGTRAP");
	const int sbox_session_varname_prefix_len = strlen("SBOX_SESSION_");

	/* SBOX_SESSION_* is now preserved properly (these are practically
	 * read-only variables now)
	*/
	
	/* if we have LD_PRELOAD env var set, make sure the new my_envp
	 * has it as well
	 */

	/* if the caller sets SBOX_SIGTRAP, then assume that it was
	 * intentional and leave it alone, but if they just call
	 * execve without SBOX_SIGTRAP in the environment, then
	 * SBOX_SIGTRAP should be inherited from us
	 */

	/* count the environment variables and arguments, also check
	 * for LD_PRELOAD, LD_LIBRARY_PATH, SBOX_SESSION_* and
	 * SBOX_SIGTRAP
	 */
	for (p=(char **)envp, envc=0; *p; p++, envc++) {
		if (**p == 'L') {
			if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) == 0) {
				if (asprintf(&user_ld_preload,
					"__SB2_%s", *p) < 0) {
					SB_LOG(SB_LOGLEVEL_ERROR,
						"asprintf failed to create __SB2_%s", *p);
				}
				continue;
			}
			if (strncmp("LD_LIBRARY_PATH=", *p, strlen("LD_LIBRARY_PATH=")) == 0) {
				if (asprintf(&user_ld_library_path,
					"__SB2_%s", *p) < 0) {
					SB_LOG(SB_LOGLEVEL_ERROR,
						"asprintf failed to create __SB2_%s", *p);
				}
				continue;
			}
		} else if (**p == 'S') {
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
			if (strncmp("SBOX_SIGTRAP=", *p,
			     sbox_sigtrap_varname_len+1) == 0) {
				has_sbox_sigtrap = 1;
				continue;
			}
		}
	}
	if (!has_sbox_session_dir) {
		SB_LOG(SB_LOGLEVEL_WARNING, 
			"Detected attempt to clear SBOX_SESSION_DIR, "
				"restored to %s", sbox_session_dir);
	}

	/* allocate new environment. Add 11 extra elements (all may not be
	 * needed always) */
	my_envp = (char **)calloc(envc + 11, sizeof(char *));

	for (i = 0, p=(char **)envp; *p; p++) {
		if (strncmp(*p, "__SB2_", strlen("__SB2_")) == 0) {
			/* __SB2_* are temporary variables that must not
			 * be relayed to the next executable => skip it.
			 * Such variables include: __SB2_BINARYNAME,
			 * __SB2_REAL_BINARYNAME, __SB2_ORIG_BINARYNAME
			*/
			continue;
		}
		if (**p == 'L') {
			/* drop LD_PRELOAD and LD_LIBRARY_PATH */
			if ((strncmp("LD_PRELOAD=", *p,
				strlen("LD_PRELOAD=")) == 0) ||
			    (strncmp("LD_LIBRARY_PATH=", *p,
				strlen("LD_LIBRARY_PATH=")) == 0)) continue;
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
		} else if ((strncmp(*p, "NLSPATH=", 8) == 0) ||
		    		(strncmp(*p, "LOCPATH=", 8) == 0)) {
			/*
			 * We need to drop any previously set locale
			 * paths (set in argvenvp.lua) so that they
			 * won't get inherited accidentally to child
			 * process who don't need them.
			 */
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

	/* add back SBOX_SIGTRAP if it was removed accidentally, so
	 * exec following in GDB will work */
	if (!has_sbox_sigtrap && getenv("SBOX_SIGTRAP")) {
		SB_LOG(SB_LOGLEVEL_WARNING,
		       "Detected attempt to clear SBOX_SIGTRAP, "
		       "restored to %s", getenv("SBOX_SIGTRAP"));
		if (asprintf(&(my_envp[i]), "SBOX_SIGTRAP=%s",
			     getenv("SBOX_SIGTRAP")) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
			       "asprintf failed to create SBOX_SIGTRAP");
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

	if (asprintf(&new_orig_file_var, "__SB2_ORIG_BINARYNAME=%s", orig_file) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"asprintf failed to create __SB2_ORIG_BINARYNAME");
	}
	my_envp[i++] = new_orig_file_var; /* add the new process' name */

	/* __SB2_EXEC_BINARYNAME is the original filename; for scripts,
	 * it is the name of script, otherwise it is same as
	 *  __SB2_ORIG_BINARYNAME
	*/
	if (asprintf(&new_exec_file_var, "__SB2_EXEC_BINARYNAME=%s", orig_file) < 0) {
		SB_LOG(SB_LOGLEVEL_ERROR,
			"asprintf failed to create __SB2_EXEC_BINARYNAME");
	}
	my_envp[i++] = new_exec_file_var; /* add the new process' name */

	/* allocate slot for __SB2_REAL_BINARYNAME that is filled later on */
	my_envp[i++] = strdup("__SB2_REAL_BINARYNAME=");

	/* add user's versions of LD_PRELOAD and LD_LIBRARY_PATH */
	if (user_ld_preload != NULL) {
		my_envp[i++] = user_ld_preload;
		SB_LOG(SB_LOGLEVEL_NOISE, "Added %s", user_ld_preload);
	}
	if (user_ld_library_path != NULL) {
		my_envp[i++] = user_ld_library_path;
		SB_LOG(SB_LOGLEVEL_NOISE, "Added %s", user_ld_library_path);
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

static int strvec_contains_prefix(char *const *strvec,
	const char *prefix, size_t *index)
{
	int i, len;

	if (!strvec || !prefix) return(0);

	len = strlen(prefix);
	for (i = 0; strvec[i] != NULL; i++) {
		SB_LOG(SB_LOGLEVEL_NOISE2,
			"strvec_contains_prefix: try %s", strvec[i]);
		if (strncmp(strvec[i], prefix, len) == 0) {
			if (index != NULL)
				*index = i;
			SB_LOG(SB_LOGLEVEL_NOISE2,
				"strvec_contains_prefix: found");
			return (1);
		}
	}
	SB_LOG(SB_LOGLEVEL_NOISE2, "strvec_contains_prefix: not found");
	return (0);
}

/* "patch" environment = change environment variables.
 * the variable must already exist in the environment;
 * this doesn't do anything if the variable has been
 * removed from environment.
 *
 * - "var_perfix" should contain the variable name + '='
*/
static void change_environment_variable(
	char **my_envp, const char *var_prefix, const char *new_value)
{
	size_t idx;

	if (strvec_contains_prefix(my_envp, var_prefix, &idx)) {
		char *new_value_buf, *orig_value;

		/* release the placeholder */
		orig_value = my_envp[idx];
		free(orig_value);

		if (asprintf(&new_value_buf, "%s%s",
		    var_prefix, new_value) < 0) {
			SB_LOG(SB_LOGLEVEL_ERROR,
				"asprintf failed to create new value %s%s",
				var_prefix, new_value);
		} else {
			my_envp[idx] = new_value_buf;
		}

		SB_LOG(SB_LOGLEVEL_DEBUG, "Changed: %s", new_value_buf);
	} else {
		SB_LOG(SB_LOGLEVEL_DEBUG, "Failed to change %s%s", 
			var_prefix, new_value);
	}
}


static int prepare_exec(const char *exec_fn_name,
	char *exec_policy_name,
	const char *orig_file,
	int file_has_been_mapped,
	char *const *orig_argv,
	char *const *orig_envp,
	enum binary_type *typep,
	char **new_file,  /* return value */
	char ***new_argv,
	char ***new_envp) /* *new_envp must be filled by the caller */
{
	char **my_envp = *new_envp;
	char **my_argv = NULL, *my_file = NULL;
	char *binaryname, *tmp, *mapped_file;
	int err = 0;
	enum binary_type type;
	int postprocess_result = 0;
	int ret = 0; /* 0: ok to exec, ret<0: exec fails */

	(void)exec_fn_name; /* not yet used */
	(void)orig_envp; /* not used */

	SB_LOG(SB_LOGLEVEL_DEBUG,
		"prepare_exec(): orig_file='%s'",
		orig_file);

	tmp = strdup(orig_file);
	binaryname = strdup(basename(tmp)); /* basename may modify *tmp */
	free(tmp);
	
	my_file = strdup(orig_file);

	my_argv = duplicate_argv(orig_argv);

	if (!file_has_been_mapped) {
		if ((err = sb_execve_preprocess(&my_file, &my_argv, &my_envp)) != 0) {
			SB_LOG(SB_LOGLEVEL_ERROR, "argvenvp processing error %i", err);
		}
	}

	/* test if mapping is enabled during the exec()..
	 * (host-* tools disable it)
	*/
	if (file_has_been_mapped) {
		/* (e.g. we came back from run_hashbang()) */
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"prepare_exec(): no double mapping, my_file = %s", my_file);
		mapped_file = strdup(my_file);
	} else if (strvec_contains_prefix(my_envp, "SBOX_DISABLE_MAPPING=1", NULL)) {
		SB_LOG(SB_LOGLEVEL_DEBUG,
			"do_exec(): mapping disabled, my_file = %s", my_file);
		mapped_file = strdup(my_file);

		/* we won't call sbox_map_path_for_exec() because mapping
		 * is disabled.  */
	} else {
		/* now we have to do path mapping for my_file to find exactly
		 * what is the path we're supposed to deal with
		 */
		mapping_results_t	mapping_result;

		clear_mapping_results_struct(&mapping_result);
		sbox_map_path_for_exec("do_exec", my_file, &mapping_result);
		mapped_file = (mapping_result.mres_result_buf ?
			strdup(mapping_result.mres_result_buf) : NULL);
		exec_policy_name = (mapping_result.mres_exec_policy_name ?
			strdup(mapping_result.mres_exec_policy_name) : NULL);
			
		free_mapping_results(&mapping_result);

		SB_LOG(SB_LOGLEVEL_DEBUG,
			"do_exec(): my_file = %s, mapped_file = %s",
			my_file, mapped_file);
	}

	/*
	 * prepare_envp_for_do_exec() left us placeholder in envp array
	 * that we will fill now with fully mangled binary name.
	 */
	change_environment_variable(my_envp,
		"__SB2_REAL_BINARYNAME=", mapped_file);

	/* inspect the completely mangled filename */
	type = inspect_binary(mapped_file, 1/*check_x_permission*/);
	if (typep) *typep = type;

	switch (type) {
		case BIN_HASHBANG:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/hashbang %s", mapped_file);
			/* prepare_hashbang() will call prepare_exec()
			 * recursively */
			ret = prepare_hashbang(&mapped_file, my_file,
					&my_argv, &my_envp, exec_policy_name);
			break;

		case BIN_HOST_DYNAMIC:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/host-dynamic %s",
					mapped_file);

			postprocess_result = sb_execve_postprocess("native",
				exec_policy_name,
				&mapped_file, &my_file, binaryname,
				&my_argv, &my_envp);

			if (postprocess_result < 0) {
				errno = EINVAL;
				ret = -1;
			}
			break;

		case BIN_HOST_STATIC:
			/* don't print warning, if this static binary
			 * has been allowed (see the wrapper for
			 * ldconfig - we don't want to see warnings
			 * every time when someone executes that)
			*/
			{
				const char *allow_static_bin;

				allow_static_bin = getenv(
					"SBOX_ALLOW_STATIC_BINARY");
				if (allow_static_bin &&
				    !strcmp(allow_static_bin, mapped_file)) {
					/* no warnning, just debug */
					SB_LOG(SB_LOGLEVEL_DEBUG,
						"statically linked "
						"native binary %s (allowed)",
						mapped_file);
				}  else {
					SB_LOG(SB_LOGLEVEL_WARNING,
						"Executing statically "
						"linked native binary %s",
						mapped_file);
				}
			}
			/* Call postprocessing.
			 * This adds LD_LIBRARY_PATH and LD_PRELOAD.
			 * the static binary itselft does not need
			 * these, but if it executes another 
			 * program, then there is at least some
			 * hope of getting back to SB2. It won't
			 * be able to start anything that runs
			 * under CPU transparency, but host-compatible
			 * binaries may be able to get back..
			*/
			postprocess_result = sb_execve_postprocess("static",
				exec_policy_name,
				&mapped_file, &my_file, binaryname,
				&my_argv, &my_envp);
			if (postprocess_result < 0) {
				errno = EINVAL;
				ret = -1;
			}
			break;

		case BIN_TARGET:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/target %s",
					mapped_file);

			postprocess_result = sb_execve_postprocess(
				"cpu_transparency",
				exec_policy_name,
				&mapped_file, &my_file,
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
			errno = ENOEXEC;
			ret = -1;
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"Unidentified executable detected (%s) => ENOEXEC",
				mapped_file);
			break;
	}

	*new_file = mapped_file;
	*new_argv = my_argv;
	*new_envp = my_envp;
	return(ret);
}

int do_exec(int *result_errno_ptr,
	const char *exec_fn_name, const char *orig_file,
	char *const *orig_argv, char *const *orig_envp)
{
	char *new_file = NULL;
	char **new_argv = NULL;
	char **new_envp = NULL;
	int  result;

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* just run it, don't worry, be happy! */
	} else {
		int	r;
		char	**my_envp_copy = NULL; /* used only for debug log */
		char	*tmp, *binaryname;
		enum binary_type type;

		tmp = strdup(orig_file);
		binaryname = strdup(basename(tmp)); /* basename may modify *tmp */
		free(tmp);

		if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
			char *buf = strvec_to_string(orig_argv);

			SB_LOG(SB_LOGLEVEL_DEBUG,
				"EXEC/Orig.args: %s : %s", orig_file, buf);
			free(buf);
		
			/* create a copy of intended environment for logging,
			 * before sb_execve_preprocess() gets control */ 
			my_envp_copy = prepare_envp_for_do_exec(orig_file,
				binaryname, orig_envp);
		}
		
		new_envp = prepare_envp_for_do_exec(orig_file, binaryname, orig_envp);

		r = prepare_exec(exec_fn_name, NULL/*exec_policy_name: not yet known*/,
			orig_file, 0, orig_argv, orig_envp,
			&type, &new_file, &new_argv, &new_envp);

		if (SB_LOG_IS_ACTIVE(SB_LOGLEVEL_DEBUG)) {
			/* find out and log if sb_execve_preprocess() did something */
			compare_and_log_strvec_changes("argv", orig_argv, new_argv);
			compare_and_log_strvec_changes("envp", my_envp_copy, new_envp);
		}

		if (r < 0) {
			SB_LOG(SB_LOGLEVEL_DEBUG,
				"EXEC denied by prepare_exec(), %s", orig_file);
			*result_errno_ptr = errno;
			return(r); /* exec denied */
		}

		if (check_envp_has_ld_preload_and_ld_library_path(
			new_envp ? new_envp : orig_envp) == 0) {

			SB_LOG(SB_LOGLEVEL_ERROR,
				"exec(%s) failed, internal configuration error: "
				"LD_LIBRARY_PATH and/or LD_PRELOAD were not set "
				"by exec mapping logic", orig_file);
			*result_errno_ptr = EINVAL;
			return(-1);
		}
	}

	errno = *result_errno_ptr; /* restore to orig.value */
	result = sb_next_execve(
		(new_file ? new_file : orig_file),
		(new_argv ? new_argv : orig_argv),
		(new_envp ? new_envp : orig_envp));
	*result_errno_ptr = errno;
	SB_LOG(SB_LOGLEVEL_DEBUG,
		"EXEC failed (%s), errno=%d", orig_file, *result_errno_ptr);
	return(result);
}

/* ----- EXPORTED from interface.master: ----- */
int sb2show__execve_mods__(
	char *file,
	char *const *orig_argv, char *const *orig_envp,
	char **new_file, char ***new_argv, char ***new_envp)
{
	int	ret = 0;
	char	*tmp, *binaryname;

	if (!sb2_global_vars_initialized__) sb2_initialize_global_variables();

	SB_LOG(SB_LOGLEVEL_DEBUG, "%s '%s'", __func__, orig_argv[0]);

	if (!file) return(ret);

	tmp = strdup(file);
	binaryname = strdup(basename(tmp)); /* basename may modify *tmp */
	free(tmp);

	*new_envp = prepare_envp_for_do_exec(file, binaryname, orig_envp);

	ret = prepare_exec("sb2show_exec", NULL/*exec_policy_name*/,
		file, 0, orig_argv, orig_envp,
		NULL, new_file, new_argv, new_envp);

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
	enum binary_type type = inspect_binary(filename, 0/*check_x_permission*/);
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

/* popen():
 * Unfortunately we can't use same stragegy for popen() as what
 * was used for system(), because popen() needs to use some fields
 * of the FILE structure (otherwise pclose() would not work).
 * The solution is to set up the environment (LD_PRELOAD and
 * LD_LIBRARY_PATH) for the host, and then we'll use /bin/sh
 * of the host as a trampoline the get the process up & running.
 * This is not completely correct as location of /bin/sh should
 * be taken determined by the mapping rules, but in practise it
 * produces correct results.
*/
FILE *popen_gate(int *result_errno_ptr,
	FILE *(*real_popen_ptr)(const char *command, const char *type),
        const char *realfnname, const char *command, const char *type)
{
	char	*user_ld_lib_path = NULL;
	char	*user_ld_preload = NULL;
	char	*cp;
	char	*popen_ld_preload = NULL;
	char	*popen_ld_lib_path = NULL;
	FILE	*res;

	(void)realfnname;
	SB_LOG(SB_LOGLEVEL_DEBUG, "popen(%s,%s)", command, type);

	/* popen() uses our 'environ', so we'll have to make
	 * temporary changes and restore the values after
	 * popen() has created the process:
	*/
	cp = getenv("LD_LIBRARY_PATH");
	if (cp) user_ld_lib_path = strdup(cp);
	cp  = getenv("LD_PRELOAD");
	if (cp) user_ld_preload  = strdup(cp);

	sb_get_host_policy_ld_params(&popen_ld_preload, &popen_ld_lib_path);

	if (popen_ld_lib_path) setenv("LD_LIBRARY_PATH", popen_ld_lib_path, 1);
	else unsetenv("LD_LIBRARY_PATH");
	if (popen_ld_preload) setenv("LD_PRELOAD", popen_ld_preload, 1);
	else unsetenv("LD_PRELOAD");

	SB_LOG(SB_LOGLEVEL_DEBUG, "popen: LD_LIBRARY_PATH=%s", popen_ld_lib_path);
	SB_LOG(SB_LOGLEVEL_DEBUG, "popen: LD_PRELOAD=%s", popen_ld_preload);

	errno = *result_errno_ptr; /* restore to orig.value */
	res = (*real_popen_ptr)(command, type);
	*result_errno_ptr = errno;

	SB_LOG(SB_LOGLEVEL_DEBUG, "popen: restoring LD_PRELOAD and LD_LIBRARY_PATH");

	if (user_ld_lib_path) setenv("LD_LIBRARY_PATH", user_ld_lib_path, 1);
	else unsetenv("LD_LIBRARY_PATH");
	if (user_ld_preload) setenv("LD_PRELOAD", user_ld_preload, 1);
	else unsetenv("LD_PRELOAD");

	if (popen_ld_lib_path) free(popen_ld_lib_path);
	if (popen_ld_preload) free(popen_ld_preload);
	if (user_ld_lib_path) free(user_ld_lib_path);
	if (user_ld_preload) free(user_ld_preload);

	return(res);
}

