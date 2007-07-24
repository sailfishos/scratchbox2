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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <fcntl.h>

#include <sys/syscall.h>
#include <sys/utsname.h>
#include <elf.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <config.h>
#include <sb2.h>
#include <mapping.h>

#include "sb_env.h"

#define RPATH_PREFIX "/scratchbox/"
#define LDPATH       "/scratchbox/lib/ld.so"
#define LIBCPATH     "/scratchbox/lib/libc.so.6"

#if __BYTE_ORDER == __BIG_ENDIAN
# define elf_endianness ELFDATA2MSB
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# define elf_endianness ELFDATA2LSB
#else
# error Invalid __BYTE_ORDER
#endif

enum binary_type {
	BIN_NONE, 
	BIN_UNKNOWN, 
	BIN_FOREIGN, 
	BIN_STATIC, 
	BIN_DYNAMIC, 
	BIN_SCRATCHBOX, 
	BIN_TARGET
};


#define LD_LIB_PATH	"/scratchbox/sarge/lib/:/scratchbox/sarge/lib/tls:/scratchbox/sarge/usr/lib"
#define LD_SO		"/scratchbox/sarge/lib/ld-linux.so.2"

static int endswith(char *haystack, char *needle)
{
	return strcmp(&haystack[strlen(haystack)-strlen(needle)], needle)==0;
}

static int one_startswith(char **data, char *key)
{
	int len = strlen(key);
	if (!data) return 0;

	char **p;
	for (p = data; *p; p++) {
		if (strncmp(*p, key, len)==0) return 1;
	}
	return 0;
}

static int elem_count(char **elems)
{
	int count = 0;
	char **p = elems;
	while (*p) {
		p++; count++;
	}
	return count;
}

#if 0
/* Returns the number of splittable elements in s */
static int substr_count(char *s, char sep)
{
	int count=1;
	char *p;
	if (!s) return 0;

	for (p = s; *p; p++) {
		if (*p==sep) count++;
	}

	return count;
}
#endif

static int is_linker(char *fname)
{
	return endswith(fname, "ld");
}

static char **padded_list(char **list)
{
	if (!list) {
		list = (char **)calloc(1, sizeof(char*));
	}
	return list;
}

#define ENVLEN (2)
char *envs[] = {"LD_LIBRARY_PATH", "LD_RUN_PATH"};
#define LINK_FLAG "-rpath-link"

char **pre_args(char *fname, char **args)
{
	char **ret = NULL;
	if (is_linker(fname) && !one_startswith(args,"-rpath")) {
		int n = 0;
		int i = 0,j = 0;
		// Figure out list size.
		for(; i<ENVLEN; i++) {
				n += (getenv(envs[i]) != NULL);
		}

		ret = (char **)calloc(n*2+1, sizeof(char *));
		ret[n] = NULL;

		// Fill the list
		//
		for(i=0,j=0;i<ENVLEN;i++) {
			char *p = getenv(envs[i]);
			if (!p) continue;
			ret[j++] = strdup(LINK_FLAG);
			ret[j++] = strdup(p);
		}

	}
	return padded_list(ret);
}

char **post_args(char *fname)
{
	char **ret = NULL;
	return padded_list(ret);
}

int run_cputransparency(char *file, char **argv, char *const *envp)
{
	char *cputransp_bin, *target_root;
	char *basec, *bname;

	cputransp_bin = getenv("SBOX_CPUTRANSPARENCY_METHOD");
	if (!cputransp_bin) {
		fprintf(stderr, "SBOX_CPUTRANSPARENCY_METHOD not set, unable to execute the target binary\n");
		return -1;
	}

	target_root = getenv("SBOX_TARGET_ROOT");
	if (!target_root) {
		fprintf(stderr, "SBOX_TARGET_ROOT not set, unable to execute the target binary\n");
		return -1;
	}

	basec = strdup(cputransp_bin);
	bname = basename(basec);

	if (strstr(bname, "qemu")) {
		free(basec);
		return run_qemu(cputransp_bin, target_root, file, argv, envp);
	} else if (strstr(bname, "sbrsh")) {
		free(basec);
		return run_sbrsh(cputransp_bin, target_root, file, argv, envp);
	}

	free(basec);
	fprintf(stderr, "run_cputransparency() error: Unknown cputransparency method: [%s]\n", cputransp_bin);
	return -1;
}

int run_sbrsh(char *sbrsh_bin, char *target_root, char *file, char **argv, char *const *envp)
{
	return -1;
}

int run_qemu(char *qemu_bin, char *target_root, char *file, char **argv, char *const *envp)
{
	char **my_argv, **p;
	char **my_envp;
	int i = 0;
	//DBGOUT("about to run qemu: %s %s %s %s\n", qemu_bin, target_root, file, argv[0]);
	my_argv = (char **)calloc(elem_count(argv) + 5 + 1, sizeof(char *));
	my_envp = (char **)calloc(elem_count((char **)envp) + 1, sizeof(char *));

	my_argv[i++] = qemu_bin;
	my_argv[i++] = "-drop-ld-preload";
	my_argv[i++] = "-L";

	my_argv[i++] = "/";
	my_argv[i++] = file;
	for (p=&argv[1]; *p; p++) {
		//DBGOUT("processing args: [%s]\n", *p);
		my_argv[i++] = *p;
	}

	my_argv[i] = NULL;
	i = 0;
	for (p=(char **)envp; *p; p++) {
		//DBGOUT("ENV: [%s]\n", *p);
#if 0
		if (strncmp(*p, "LD_PRELOAD=", strlen("LD_PRELOAD="))==0) {
			//DBGOUT("skipping LD_PRELOAD\n");
			continue;
		}
#endif
		my_envp[i++] = *p;
	}
	my_envp[i] = NULL;

	//DBGOUT("just before running it [%s][%s]\n", my_argv[0], my_argv[1]);
	return sb_next_execve(my_argv[0], my_argv, my_envp);
	//DBGOUT("after running it\n");
	return -1;
}

int run_app(char *file, char **argv, char *const *envp)
{
	char *binaryname, **my_argv;
	char **pre,**post;
	char **p;
	int argc=0, i=0;
	
	binaryname = basename(strdup(file));

	/* DBGOUT("[%s][%s]\n", file, argv[0]); */
	argc = elem_count(argv);

	pre = pre_args(binaryname, &argv[1]);
	post = post_args(binaryname);

	/* DBGOUT("allocating: %i\n", elem_count(pre) + argc + elem_count(post) + 1); */
	my_argv = (char **)calloc(elem_count(pre) + argc + elem_count(post) + 1, sizeof (char *));
	my_argv[i++] = argv[0];

	for (p=pre; *p; p++)
		my_argv[i++]=*p;
	
	for (p=argv+1; *p; p++)
		my_argv[i++]=*p;
	
	for (p=post; *p; p++)
		my_argv[i++]=*p;

	my_argv[i] = NULL;

	/* DBGOUT("about to execute: %s\n", my_argv[0]); */

#if 0
	DBGOUT("**** CRAP starts here ****\n");
	for (p=my_argv; *p; p++) {
		DBGOUT("[%s]\n", *p);
	}
	DBGOUT("**** CRAP ENDS HERE *****\n");
#endif
	
	sb_next_execve(file, my_argv, envp);

	fprintf(stderr, "libsb2.so failed running (%s): %s\n", file, strerror(errno));
	return -12;
}

int ld_so_run_app(char *file, char **argv, char *const *envp)
{
	char *binaryname, **my_argv;
	char *host_libs, *ld_so;
	char **pre,**post;
	char **p;
	char *tmp;
	char ld_so_buf[PATH_MAX + 1];
	char ld_so_basename[PATH_MAX + 1];
	int argc;
	int i = 0;
	
	DBGOUT("__YAYYYY___\n");
	tmp = getenv("REDIR_LD_LIBRARY_PATH");

	if (!tmp) {
		host_libs = LD_LIB_PATH;
	} else {
		host_libs = strdup(tmp);
	}

	tmp = getenv("REDIR_LD_SO");
	if (!tmp) {
		ld_so = LD_SO;
	} else {
		ld_so = strdup(tmp);
	}

	memset(ld_so_buf, '\0', PATH_MAX + 1);
	memset(ld_so_basename, '\0', PATH_MAX + 1);

	if (readlink(ld_so, ld_so_buf, PATH_MAX) < 0) {
		if (errno == EINVAL) {
			/* it's not a symbolic link, so use it directly */
			strcpy(ld_so_basename, basename(ld_so));

		} else {
			/* something strange, bail out */
			perror("readlink(ld_so) failed badly. aborting\n");
			return -1;
		}
	} else {
		strcpy(ld_so_basename, basename(ld_so_buf));
	}

	binaryname = basename(strdup(file));
	
	/* if the file to be run is the dynamic loader itself, 
	 * run it straight
	 */

	if (strcmp(binaryname, ld_so_basename) == 0) {
		sb_next_execve(file, argv, envp);
		perror("failed to directly run the dynamic linker!\n");
		return -1;
	}

	argc = elem_count(argv);

	pre = pre_args(binaryname, &argv[1]);
	post = post_args(binaryname);

	my_argv = (char **)calloc(4 + elem_count(pre) + argc - 1  + elem_count(post) + 1, sizeof (char *));
	my_argv[i++] = ld_so;
	my_argv[i++] = "--library-path";
	my_argv[i++] = host_libs;
	my_argv[i++] = file;

	for (p=pre; *p; p++)
		my_argv[i++]=*p;

	for (p=argv+1; *p; p++)
		my_argv[i++]=*p;

	for (p=post; *p; p++)
		my_argv[i++]=*p;

	//printf("about to execute: %s, %s, %s, %s\n", my_argv[0], my_argv[1], my_argv[2], my_argv[3]);
	
	sb_next_execve(my_argv[0], my_argv, envp);

	fprintf(stderr, "sb_alien (running %s): %s\n", file, strerror(errno));
	return -11;
}


static enum binary_type inspect_binary(const char *filename)
{
	enum binary_type retval;
	int fd, phnum, j;
	struct stat status;
	char *region;
	unsigned int ph_base, ph_frag;
#ifdef __x86_64__
	int64_t reloc0;
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
#else
	int reloc0;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
#endif
	retval = BIN_NONE;

	fd = syscall(__NR_open, filename, O_RDONLY, 0);
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

	switch (ehdr->e_machine) {
		case EM_ARM:
		case EM_PPC:
			retval = BIN_TARGET;
			goto _out_munmap;
	}

	if (strncmp((char *) ehdr, ELFMAG, SELFMAG) != 0) {
		goto _out_munmap;
	}

	retval = BIN_FOREIGN;

	if (ehdr->e_ident[EI_DATA] != elf_endianness ||
			ehdr->e_ident[EI_VERSION] != EV_CURRENT ||
			ehdr->e_machine != ELF_ARCH) {
		goto _out_munmap;
	}

	retval = BIN_STATIC;

	phnum = ehdr->e_phnum;
	reloc0 = ~0;
	ph_base = ehdr->e_phoff & PAGE_MASK;
	ph_frag = ehdr->e_phoff - ph_base;

#ifdef __x86_64__
	phdr = (Elf64_Phdr *) (region + ph_base + ph_frag);
#else
	phdr = (Elf32_Phdr *) (region + ph_base + ph_frag);
#endif

	for (j = phnum; --j >= 0; ++phdr) {
		if (PT_LOAD == phdr->p_type && ~0 == reloc0) {
			reloc0 = phdr->p_vaddr - phdr->p_offset;
		}
	}

	phdr -= phnum;

	for (j = phnum; --j >= 0; ++phdr) {
		int szDyn;
		unsigned int pt_base, pt_frag;
#ifdef __x86_64__
		Elf64_Dyn *dp_rpath, *dp_strsz, *dp_strtab, *dp;
#else
		Elf32_Dyn *dp_rpath, *dp_strsz, *dp_strtab, *dp;
#endif

		if (PT_DYNAMIC != phdr->p_type) {
			continue;
		}

		retval = BIN_DYNAMIC;

		pt_base = phdr->p_offset & PAGE_MASK;
		pt_frag = phdr->p_offset - pt_base;
		szDyn = phdr->p_filesz;

		dp_rpath = NULL;
		dp_strsz = NULL;
		dp_strtab = NULL;

#ifdef __x86_64__
		dp = (Elf64_Dyn *) ((char *) region + pt_base + pt_frag);
#else
		dp = (Elf32_Dyn *) ((char *) region + pt_base + pt_frag);
#endif

		for (; 0 <= (szDyn -= sizeof (*dp)); ++dp) {
			if (DT_RPATH == dp->d_tag) {
				dp_rpath = dp;
			} else if (DT_STRTAB == dp->d_tag) {
				dp_strtab = dp;
			} else if (DT_STRSZ == dp->d_tag) {
				dp_strsz = dp;
			}
		}

		if (dp_rpath && dp_strtab && dp_strsz) {
			char *rpathstr, *strtab, *match;
			unsigned int unloc, str_base, str_frag;

			unloc = dp_strtab->d_un.d_val - reloc0;
			str_base = unloc & PAGE_MASK;
			str_frag = unloc - str_base;

			strtab = (char *) (region + str_base + str_frag);
			rpathstr = dp_rpath->d_un.d_val + strtab;

			match = strstr(rpathstr, RPATH_PREFIX);
			if (match && (match == rpathstr || match[-1] == ':')) {
				retval = BIN_SCRATCHBOX;
				break;
			}
		}
	}
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
	//DBGOUT("here we are: [%s]\n", t);
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


static char const* const*drop_preload(char *const oldenv[])
{
	char const* *newenv;
	int i, j;

	for (i = 0; oldenv[i]; ++i)
		;

	newenv = (char const**) calloc(i + 1, sizeof (char *));
	if (!newenv) {
		fprintf(stderr, "libsb: %s\n", strerror(errno));
		return (char const*const*)oldenv;
	}

	for (i = 0, j = 0; oldenv[i]; ++i) {
		if (strncmp(oldenv[i], "LD_PRELOAD=", 11) != 0) {
			newenv[j++] = oldenv[i];
		}
	}

	return newenv;
}


int do_exec(const char *file, char *const *argv, char *const *envp)
{
	char **my_envp, **my_argv, **p;
	char *binaryname, *tmp, *my_file;
	int envc=0, argc=0, i, has_ld_preload=0;


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
		binaryname = strdup(basename(file));
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

	i = strlen(binaryname) + strlen("__SB2_BINARYNAME") + 1;
	tmp = malloc(i * sizeof(char *));
	strcpy(tmp, "__SB2_BINARYNAME=");
	strcat(tmp, binaryname);

	i = 0;
	for (p=(char **)envp; *p; p++) {
		/* DBGOUT("ENV: [%s]\n", *p); */
		if (strncmp(*p, "__SB2_BINARYNAME=", strlen("__SB2_BINARYNAME=")) == 0) {
			/* already set, skip it */
			continue;
		}

		if (strncmp(*p, "__SBOX_GCCWRAPPER_RUN=", strlen("__SBOX_GCCWRAPPER_RUN")) == 0) {
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
		case BIN_FOREIGN:
			new_env = drop_preload(my_envp);
			break;

		case BIN_STATIC:
			new_env = override_sbox_env(drop_preload(my_envp));
			break;

		case BIN_DYNAMIC:
			{
				if (getenv("SBOX_TOOLS_ROOT")) {
					return ld_so_run_app((char *)my_file, (char **)my_argv, my_envp);
				} else {
					return run_app((char *)my_file, (char **)my_argv, my_envp);
				}
			}
		case BIN_TARGET:
			return run_cputransparency(my_file, (char **)my_argv, my_envp);

		case BIN_NONE:
		case BIN_UNKNOWN:
		case BIN_SCRATCHBOX:
			DBGOUT("unknown\n");
			break;
	}

	return sb_next_execve(my_file, my_argv, (char *const*)new_env);
}

