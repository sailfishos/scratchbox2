/*
 * Copyright (C) 2006,2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL version 2.1
 */

#define _GNU_SOURCE

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

#include <sys/utsname.h>
#include <elf.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <config.h>
#include <sb2.h>
#include <mapping.h>

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
	BIN_HOST,
	BIN_TARGET
};


static int elem_count(char **elems)
{
	int count = 0;
	char **p = elems;
	while (*p) {
		p++; count++;
	}
	return count;
}

int run_cputransparency(char *orig_file, char *file, char **argv,
			char *const *envp)
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
		return run_qemu(cputransp_bin, orig_file, file, argv, envp);
	} else if (strstr(bname, "sbrsh")) {
		free(basec);
		return run_sbrsh(cputransp_bin, target_root, orig_file,
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

int run_sbrsh(char *sbrsh_bin, char *target_root, char *orig_file,
              char **argv, char *const *envp)
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

	file = orig_file;
	if (file[0] == '/') {
		if (strstr(file, target_root) != file) {
			fprintf(stderr, "Binary must be under target (%s)"
			        " when using sbrsh\n", target_root);
			errno = ENOTDIR;
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

	my_argv[i++] = sbrsh_bin;
	if (config) {
		my_argv[i++] = "--config";
		my_argv[i++] = config;
	}
	my_argv[i++] = "--directory";
	my_argv[i++] = dir;
	my_argv[i++] = file;

	for (p = &argv[1]; *p; p++)
		my_argv[i++] = *p;

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

int run_qemu(char *qemu_bin, char *orig_file,char *file,
		char **argv, char *const *envp)
{
	char **my_argv, **p;
	int i = 0;

	SB_LOG(SB_LOGLEVEL_INFO, "Exec:qemu (%s,%s,%s)",
		qemu_bin, orig_file, file);

	my_argv = (char **)calloc(elem_count(argv) + 5 + 1, sizeof(char *));

	my_argv[i++] = qemu_bin;
	my_argv[i++] = "-drop-ld-preload";
	my_argv[i++] = "-L";

	my_argv[i++] = "/";
	my_argv[i++] = orig_file; /* we're passing the unmapped file
				   * here, it works because qemu will
				   * do open() on it, and that gets
				   * mapped again.
				   */
	for (p=&argv[1]; *p; p++) {
		my_argv[i++] = *p;
	}

	my_argv[i] = NULL;

	return sb_next_execve(qemu_bin, my_argv, envp);
}

int run_app(char *file, char **argv, char *const *envp)
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

	fd = open_nomap_nolog(filename, O_RDONLY, 0);
	if (fd < 0) {
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
		&& elf_hdr_match(ehdr->e_machine, EM_SH, 0)) {
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

static int is_gcc_tool(char *fname)
{
	unsigned int i, index, start, c, len;
	char *t;
	char **gcc_prefixes;
	char **p;
	char *gcc_tools[] = {
		"addr2line",
		"ar",
		"as",
		"cc",
		"c++",
		"c++filt",
		"cpp",
		"g++",
		"gcc",
		"gcov",
		"gdb",
		"gdbtui",
		"gprof",
		"ld",
		"nm",
		"objcopy",
		"objdump",
		"ranlib",
		"rdi-stub",
		"readelf",
		"run",
		"size",
		"strings",
		"strip",
		NULL
	};
	char **tmp;
	if (!getenv("SBOX_CROSS_GCC_PREFIX_LIST")
		||!getenv("SBOX_HOST_GCC_PREFIX_LIST")) {
		return 0;
	}

	len = strlen(getenv("SBOX_CROSS_GCC_PREFIX_LIST"));
	len += 1 + strlen(getenv("SBOX_HOST_GCC_PREFIX_LIST"));
	t = malloc((len + 1) * sizeof(char));

	strcpy(t, getenv("SBOX_CROSS_GCC_PREFIX_LIST"));
	strcat(t, ":");
	strcat(t, getenv("SBOX_HOST_GCC_PREFIX_LIST"));

	if (!t) {
		return 0;
	}
	t = strdup(t);

	for (i = 0, c = 0; i < strlen(t); i++) {
		if (t[i] == ':') {
			c++;
		}
	}

	gcc_prefixes = (char **)calloc(c + 2, sizeof(char *));
	p = gcc_prefixes;

	for (start = 0, index = 0; index < strlen(t); index++) {
		if (t[index] == ':') {
			*p = malloc((index - start + 1) * sizeof(char));
			strncpy(*p, &t[start], index - start);
			(*p)[index - start] = '\0';
			p++;
			start = index + 1;
		}
	}
	p = NULL;

	for (tmp = gcc_tools; *tmp; tmp++) {
		for (p = gcc_prefixes; *p; p++) {
			char s[PATH_MAX];
			strcpy(s, *p);
			strcat(s, *tmp);
			if (!strcmp(*tmp, fname) || (!strcmp(s, fname))) {
				return 1;
			}
		}
	}
	return 0;
}


int do_exec(const char *exec_fn_name, const char *orig_file, const char *file,
		char *const *argv, char *const *envp)
{
	char **my_envp, **my_argv, **p;
	char *binaryname, *tmp, *my_file;
	int envc=0, argc=0, i, has_ld_preload=0;

	(void)exec_fn_name; /* not yet used */

	/* if we have LD_PRELOAD env var set, make sure the new my_envp
	 * has it as well
	 */

	if (getenv("SBOX_DISABLE_MAPPING")) {
		/* just run it, don't worry, be happy! */
		return sb_next_execve(file, argv, envp);
	}
	enum binary_type type = inspect_binary(file);

	if (type == BIN_TARGET) {
		binaryname = strdup(getenv("SBOX_CPUTRANSPARENCY_METHOD"));
	} else {
		tmp = strdup(file);
		binaryname = strdup(basename(tmp));
		free(tmp);
	}

	/* count the environment variables and arguments, also check
	 * for LD_PRELOAD
	 */
	for (p=(char **)envp; *p; p++, envc++) {
		if (strncmp("LD_PRELOAD=", *p, strlen("LD_PRELOAD=")) == 0)
			has_ld_preload = 1;
	}

	for (p=(char **)argv; *p; p++, argc++)
		;

	if (has_ld_preload || !getenv("LD_PRELOAD")) {
		my_envp = (char **)calloc(envc + 2, sizeof(char *));
	} else {
		my_envp = (char **)calloc(envc + 3, sizeof(char *));
	}

	i = strlen(binaryname) + strlen("__SB2_BINARYNAME=") + 1;
	tmp = malloc(i * sizeof(char));
	strcpy(tmp, "__SB2_BINARYNAME=");
	strcat(tmp, binaryname);

	i = 0;
	for (p=(char **)envp; *p; p++) {
		if (strncmp(*p, "__SB2_BINARYNAME=",
				strlen("__SB2_BINARYNAME=")) == 0) {
			/* already set, skip it */
			continue;
		}

		if (strncmp(*p, "__SBOX_GCCWRAPPER_RUN=",
				strlen("__SBOX_GCCWRAPPER_RUN")) == 0) {
			/* don't pass this onwards */
			continue;
		}
		my_envp[i++] = *p;
	}

	my_envp[i++] = strdup(tmp);
	free(tmp);
	if (!has_ld_preload && getenv("LD_PRELOAD")) {
		tmp = malloc(strlen(getenv("LD_PRELOAD")) 
				+ strlen("LD_PRELOAD=") + 1);
		if (!tmp)
			exit(1);
		strcpy(tmp, "LD_PRELOAD=");
		strcat(tmp, getenv("LD_PRELOAD"));
		my_envp[i++] = strdup(tmp);
		free(tmp);
	}

	my_envp[i] = NULL;

	char const* const* new_env=(char const* const*)my_envp;

	my_argv = (char **)calloc(argc + 1, sizeof(char *));
	i = 0;

	my_file = strdup(file);

	if (!getenv("__SBOX_GCCWRAPPER_RUN") && is_gcc_tool(binaryname)) {
		/* unset the env variable */
		char *sb_gcc_wrapper;
		sb_gcc_wrapper = getenv("SBOX_GCCWRAPPER");
		if (!sb_gcc_wrapper) {
			my_file = "/usr/bin/sb_gcc_wrapper";
		} else {
			my_file = strdup(sb_gcc_wrapper);
		}
	}

	for (p = (char **)argv; *p; p++) {
		my_argv[i++] = *p;
	}
	my_argv[i] = NULL;

	switch (type) {
		case BIN_HOST:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/host %s",
				(char *)my_file);

			return run_app((char *)my_file, (char **)my_argv,
					my_envp);
		case BIN_TARGET:
			SB_LOG(SB_LOGLEVEL_DEBUG, "Exec/target %s",
				(char *)my_file);

			return run_cputransparency((char *)orig_file, my_file,
					(char **)my_argv, my_envp);
		case BIN_NONE:
		case BIN_UNKNOWN:
			SB_LOG(SB_LOGLEVEL_ERROR,
					"Unidentified executable detected");
			break;
	}

	return sb_next_execve(my_file, my_argv, (char *const*)new_env);
}

