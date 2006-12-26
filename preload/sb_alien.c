#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>

#include <sb2.h>


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

char *searchdir_flags[] = { "-L/usr/local/lib", "-L/lib", "-L/usr/lib",NULL };
#define SD_FLAGLEN (3)

char **post_args(char *fname)
{
	char **ret = NULL;
	if (is_linker(fname)) {
		ret=searchdir_flags;
	}
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
	my_argv = (char **)calloc(elem_count(argv) + 4 + 1, sizeof(char *));
	my_envp = (char **)calloc(elem_count((char **)envp) + 1, sizeof(char *));

	my_argv[i++] = qemu_bin;
	my_argv[i++] = "-L";

	my_argv[i++] = target_root;
	my_argv[i++] = file;
	for (p=&argv[1]; *p; p++) {
		//DBGOUT("processing args: [%s]\n", *p);
		my_argv[i++] = *p;
	}

	my_argv[i] = NULL;
	i = 0;
	for (p=(char **)envp; *p; p++) {
		//DBGOUT("ENV: [%s]\n", *p);
		my_envp[i++] = *p;
	}
	my_envp[i] = NULL;

	//DBGOUT("just before running it [%s][%s]\n", my_argv[0], my_argv[1]);
	return next_execve(my_argv[0], my_argv, envp);
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
	
	next_execve(file, my_argv, envp);

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
		next_execve(file, argv, envp);
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
	
	next_execve(my_argv[0], my_argv, envp);

	fprintf(stderr, "sb_alien (running %s): %s\n", file, strerror(errno));
	return -11;
}
