/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1
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

#if __BYTE_ORDER == __BIG_ENDIAN
# define elf_endianness ELFDATA2MSB
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define elf_endianness ELFDATA2LSB
#else
# error Invalid __BYTE_ORDER
#endif

static int elf_hdr_match(uint16_t e_machine, uint16_t match,
			 int target_little_endian);
static uint16_t byte_swap(uint16_t a);

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
	BIN_HOST,
	BIN_TARGET,
	BIN_HASHBANG,
};


static int elem_count(char *const *elems)
{
	int count = 0;
	char **p = (char **)elems;
	while (*p) {
		p++; count++;
	}
	return count;
}

/* orig_file is the unmangled filename, file is mangled */
int run_cputransparency(const char *file, const char *unmapped_file,
			char *const *argv, char *const *envp)
{
	char *cputransp_bin, *target_root;
	char *basec, *bname;

	cputransp_bin = getenv("SBOX_CPUTRANSPARENCY_METHOD");
	if (!cputransp_bin) {
		fprintf(stderr, "SBOX_CPUTRANSPARENCY_METHOD not set, "
				"unable to execute the target binary\n");
		return -1;
	}

	target_root = getenv("SBOX_TARGET_ROOT");
	if (!target_root) {
		fprintf(stderr, "SBOX_TARGET_ROOT not set, "
				"unable to execute the target binary\n");
		return -1;
	}

	basec = strdup(cputransp_bin);
	bname = basename(basec);

	if (strstr(bname, "qemu")) {
		free(basec);
		return run_qemu(cputransp_bin, unmapped_file, argv, envp);
	} else if (strstr(bname, "sbrsh")) {
		free(basec);
		return run_sbrsh(cputransp_bin, target_root, file,
		                 argv, envp);
	}

	free(basec);
	fprintf(stderr, "run_cputransparency() error: "
			"Unknown cputransparency method: [%s]\n",
			cputransp_bin);
	return -1;
}

static int is_subdir(const char *root, const char *subdir)
{
	size_t sublen;

	if (strstr(subdir, root) != subdir)
		return 0;

	sublen = strlen(subdir);
	return root[sublen] == '/' || root[sublen] == '\0';
}

int run_sbrsh(const char *sbrsh_bin, const char *target_root,
		const char *orig_file, char *const *argv, char *const *envp)
{
	char *config, *file, *dir, **my_argv, **p;
	int len, i = 0;

	SB_LOG(SB_LOGLEVEL_INFO, "Exec:sbrsh (%s,%s,%s)",
		sbrsh_bin, target_root, orig_file);

	config = getenv("SBRSH_CONFIG");
	if (config && strlen(config) == 0)
		config = NULL;

	len = strlen(target_root);
	if (len > 0 && target_root[len - 1] == '/')
		--len;

	file = strdup(orig_file);
	if (file[0] == '/') {
		if (strstr(file, target_root) != file) {
			fprintf(stderr, "Binary must be under target (%s)"
			        " when using sbrsh\n", target_root);
			errno = ENOTDIR;
			free(file);
			return -1;
		}
		file += len;
	}

	dir = get_current_dir_name();
	if (is_subdir(target_root, dir)) {
		dir += len;
	} else if (is_subdir(getenv("HOME"), dir)) {
		/* no change */
	} else {
		fprintf(stderr, "Warning: Executing binary with bogus working"
		        " directory (/tmp) because sbrsh can only see %s and"
		        " %s\n", target_root, getenv("HOME"));
		dir = "/tmp";
	}

	my_argv = calloc(6 + elem_count(argv) + 1, sizeof (char *));

	my_argv[i++] = strdup(sbrsh_bin);
	if (config) {
		my_argv[i++] = "--config";
		my_argv[i++] = config;
	}
	my_argv[i++] = "--directory";
	my_argv[i++] = dir;
	my_argv[i++] = file;

	for (p = (char **)&argv[1]; *p; p++)
		my_argv[i++] = strdup(*p);

	for (p = (char **) envp; *p; ++p) {
		char *start, *end;

		if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) != 0)
			continue;

		start = strstr(*p, LIBSB2);
		if (!start)
			break;

		end = start + strlen(LIBSB2);

		while (start[-1] != '=' && !isspace(start[-1]))
			start--;

		while (*end != '\0' && isspace(*end))
			end++;

		memmove(start, end, strlen(end) + 1);
	}

	return sb_next_execve(sbrsh_bin, my_argv, envp);
}

int run_qemu(const char *qemu_bin, const char *file,
		char *const *argv, char *const *envp)
{
	char **my_argv, **p;
	int i = 0;

	SB_LOG(SB_LOGLEVEL_INFO, "Exec:qemu (%s,%s)",
		qemu_bin, file);

	my_argv = (char **)calloc(elem_count(argv) + 5 + 1, sizeof(char *));

	my_argv[i++] = strdup(qemu_bin);
	my_argv[i++] = "-drop-ld-preload";
	my_argv[i++] = "-L";
	my_argv[i++] = "/";
	my_argv[i++] = strdup(file); /* we're passing the unmapped file
				      * here, it works because qemu will
				      * do open() on it, and that gets
				      * mapped again.
				      */
	for (p = (char **)&argv[1]; *p; p++) {
		my_argv[i++] = strdup(*p);
	}

	my_argv[i] = NULL;

	return sb_next_execve(qemu_bin, my_argv, envp);
}

int run_app(const char *file, char *const *argv, char *const *envp)
{
	sb_next_execve(file, argv, envp);

	fprintf(stderr, "libsb2.so failed running (%s): %s\n", file,
			strerror(errno));
	return -12;
}

static int elf_hdr_match(uint16_t e_machine, uint16_t match,
			 int target_little_endian)
{
	int swap;

#ifdef WORDS_BIGENDIAN
	swap = target_little_endian;
#else
	swap = !target_little_endian;
#endif

	if (swap)
		return e_machine == byte_swap(match);

	return e_machine == match;
}

static enum binary_type inspect_binary(const char *filename)
{
	enum binary_type retval;
	int fd;
	struct stat status;
	char *region, *target_cpu;
#ifdef __x86_64__
	Elf64_Ehdr *ehdr;
#else
	Elf32_Ehdr *ehdr;
#endif
	retval = BIN_NONE;
	if (access_nomap_nolog(filename, X_OK) < 0) {
		/* can't execute it. Possible errno codes from access() 
		 * are all possible from execve(), too, so there is no
		 * need to convert errno.
		*/
		SB_LOG(SB_LOGLEVEL_DEBUG, "no X permission for '%s'",
			filename);
		retval = BIN_INVALID;
		goto _out;
	}

	fd = open_nomap_nolog(filename, O_RDONLY, 0);
	if (fd < 0) {
		retval = BIN_HOST; /* can't peek in to look, assume host */
		goto _out;
	}

	retval = BIN_UNKNOWN;

	if (fstat(fd, &status) < 0) {
		goto _out_close;
	}

	if (!S_ISREG(status.st_mode) && !S_ISLNK(status.st_mode)) {
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

#ifdef __x86_64__
	ehdr = (Elf64_Ehdr *) region;
#else
	ehdr = (Elf32_Ehdr *) region;
#endif

	target_cpu = getenv("SBOX_CPU");
	if (!target_cpu) target_cpu = "arm";

	if (!strcmp(target_cpu, "arm")
		&& elf_hdr_match(ehdr->e_machine, EM_ARM, 1)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strcmp(target_cpu, "armel")
		&& elf_hdr_match(ehdr->e_machine, EM_ARM, 1)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strcmp(target_cpu, "armeb")
		&& elf_hdr_match(ehdr->e_machine, EM_ARM, 0)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strcmp(target_cpu, "ppc")
		&& elf_hdr_match(ehdr->e_machine, EM_PPC, 0)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strcmp(target_cpu, "mips")
		&& elf_hdr_match(ehdr->e_machine, EM_MIPS, 0)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strcmp(target_cpu, "mipsel")
		&& elf_hdr_match(ehdr->e_machine, EM_MIPS_RS3_LE, 1)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	} else if (!strncmp(target_cpu, "sh", 2)
		&& elf_hdr_match(ehdr->e_machine, EM_SH, 1)) {
		retval = BIN_TARGET;
		goto _out_munmap;
	}

	retval = BIN_HOST;
_out_munmap:
	munmap(region, status.st_size);
_out_close:
	close(fd);
_out:
	return retval;
}

int run_hashbang(const char *file, char *const *argv, char *const *envp)
{
	int argc, fd, c, i, j, n, ret;
	char ch;
	char **p, *ptr, *mapped_interpreter;
	char **new_argv;
	char hashbang[SBOX_MAXPATH]; /* only 60 needed on linux, just be safe */
	char interpreter[SBOX_MAXPATH];

	if ((fd = open(file, O_RDONLY)) < 0) {
		/* unexpected error, just run it */
		return run_app(file, argv, envp);
	}

	if ((c = read(fd, &hashbang[0], SBOX_MAXPATH - 1)) < 2) {
		/* again unexpected error, close fd and run it */
		close(fd);
		return run_app(file, argv, envp);
	}

	for (argc = 0, p = (char **)argv; *p; p++)
		argc++;

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
					new_argv[n++] = strdup(file);
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

	mapped_interpreter = scratchbox_path("execve", interpreter);
	new_argv[n++] = strdup(file); /* the unmapped script path */

	for (i = 1; argv[i] != NULL && i < argc; ) {
		new_argv[n++] = argv[i++];
	}

	new_argv[n] = NULL;

	SB_LOG(SB_LOGLEVEL_DEBUG, "exec script, interp=%s",
		interpreter);

	/* feed this through do_exec to let it deal with
	 * cpu transparency etc.
	 */
	ret = do_exec("run_hashbang", mapped_interpreter, new_argv, envp);

	if (mapped_interpreter) free(mapped_interpreter);
	return ret;
}

int do_exec(const char *exec_fn_name, const char *file,
		char *const *argv, char *const *envp)
{
	char ***my_envp, ***my_argv, **my_file, **p;
	char *binaryname, *tmp, *mapped_file;
	int envc = 0, argc = 0, i, has_ld_preload = 0, err = 0;
	enum binary_type type;

	(void)exec_fn_name; /* not yet used */

	/* if we have LD_PRELOAD env var set, make sure the new my_envp
	 * has it as well
	 */

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* just run it, don't worry, be happy! */
		return sb_next_execve(file, argv, envp);
	}
	

	tmp = strdup(file);
	binaryname = strdup(basename(tmp));
	free(tmp);
	
	my_file = malloc(sizeof(char *));
	*my_file = strdup(file);

	/* count the environment variables and arguments, also check
	 * for LD_PRELOAD
	 */
	for (p=(char **)envp; *p; p++, envc++) {
		if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) == 0)
			has_ld_preload = 1;
	}

	for (p=(char **)argv; *p; p++, argc++)
		;

	my_envp = malloc(sizeof(char **));
	my_argv = malloc(sizeof(char **));

	if (has_ld_preload || !getenv("LD_PRELOAD")) {
		*my_envp = (char **)calloc(envc + 2, sizeof(char *));
	} else {
		*my_envp = (char **)calloc(envc + 3, sizeof(char *));
	}

	/* __SB2_BINARYNAME is used to communicate the binary name
	 * to the new process so that it's available even before
	 * its main function is called
	 */
	i = strlen(binaryname) + strlen("__SB2_BINARYNAME=") + 1;
	tmp = malloc(i * sizeof(char));
	strcpy(tmp, "__SB2_BINARYNAME=");
	strcat(tmp, binaryname);

	for (i = 0, p=(char **)envp; *p; p++) {
		if (strncmp(*p, "__SB2_BINARYNAME=",
				strlen("__SB2_BINARYNAME=")) == 0) {
			/* this is current process' name, skip it */
			continue;
		}

		(*my_envp)[i++] = strdup(*p);
	}
	(*my_envp)[i++] = tmp; /* add the new process' name */

	/* If our environ has LD_PRELOAD, but the given envp doesn't,
	 * add it.
	 */
	if (!has_ld_preload && getenv("LD_PRELOAD")) {
		tmp = malloc(strlen("LD_PRELOAD=")
				+ strlen(getenv("LD_PRELOAD")) + 1);
		if (!tmp)
			exit(1);
		strcpy(tmp, "LD_PRELOAD=");
		strcat(tmp, getenv("LD_PRELOAD"));
		(*my_envp)[i++] = strdup(tmp);
		free(tmp);
	}
	(*my_envp)[i] = NULL;

	*my_argv = (char **)calloc(argc + 1, sizeof(char *));
	for (i = 0, p = (char **)argv; *p; p++) {
		(*my_argv)[i++] = strdup(*p);
	}
	(*my_argv)[i] = NULL;

	if (!(err = sb_execve_mod(my_file, my_argv, my_envp))) {
		SB_LOG(SB_LOGLEVEL_ERROR, "argvenvp processing error %i", err);
	}

	/* now we have to do path mapping for *my_file to find exactly
	 * what is the path we're supposed to deal with
	 */

	mapped_file = scratchbox_path("do_exec", *my_file);

	type = inspect_binary(mapped_file); /* inspect the completely mangled 
					     * filename */

	switch (type) {
		case BIN_HASHBANG:
			return run_hashbang(mapped_file, *my_argv,
					*my_envp);
		case BIN_HOST:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/host %s",
					mapped_file);

			return run_app(mapped_file, *my_argv, *my_envp);
		case BIN_TARGET:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/target %s",
					mapped_file);

			return run_cputransparency(mapped_file, *my_file,
					*my_argv, *my_envp);
		case BIN_NONE:
		case BIN_INVALID:
		case BIN_UNKNOWN:
			SB_LOG(SB_LOGLEVEL_ERROR,
					"Unidentified executable detected");
			break;
	}

	return sb_next_execve(mapped_file, *my_argv, *my_envp);
}

