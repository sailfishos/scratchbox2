#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>


#define LD_LIB_PATH	"/scratchbox/sarge/lib/:/scratchbox/sarge/lib/tls:/scratchbox/sarge/usr/lib"
#define LD_SO		"/scratchbox/sarge/lib/ld-linux.so.2"

extern int     (*next_execve) (const char *filename, char *const argv [], char *const envp[]);


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

char **pre_args(char *fname,char **args)
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

int run_app(char *file, char **argv, char *const *envp)
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

	memset(ld_so_buf, '\0', PATH_MAX + 1);
	memset(ld_so_basename, '\0', PATH_MAX + 1);
	if (readlink(LD_SO, ld_so_buf, PATH_MAX) < 0) {
		if (errno == EINVAL) {
			printf("buu!\n");
			/* it's not a symbolic link, so use it directly */
			strcpy(ld_so_basename, basename(LD_SO));

		} else {
			/* something strange, bail out */
			perror("readlink(LD_SO) failed badly. aborting\n");
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
		perror("failed to run the dynamic linker!\n");
		return -1;
	}

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

	//printf("seuraavaksi ajetaan... %s, %s, %s, %s\n", my_argv[0], my_argv[1], my_argv[2], my_argv[3]);
	next_execve(my_argv[0], my_argv, envp);

	fprintf(stderr, "sb_alien (running %s): %s\n", file, strerror(errno));
	return -11;
}
