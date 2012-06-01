/* Test chroot simulation:
 * (inside a session)
 *    gcc -o Chroottests Chroottests.c
 *    ./Chroottests
 * ..then examine the output.
 * Afterwards,
 *    rm -r ./Chroottest-dir
 *
 * (Author: Lauri T. Aarnio)
*/

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

static void write_file(const char *filename, const char *contents)
{
	FILE *f;

	f = fopen(filename, "w");
	if (!f) {
		fprintf(stderr, "ERROR: Failed to open '%s' for writing!\n", filename);
		exit(1);
	}
	fprintf(f, "%s", contents);
	fclose(f);
}

static void print_file(const char *filename)
{
	FILE *f;
	char buf[1024];
	int s;

	f = fopen(filename, "r");
	if (!f) {
		fprintf(stderr, "Warning: Failed to open '%s' for reading!\n", filename);
		return;
	}
	s = fread(buf, sizeof(char), sizeof(buf), f);
	fclose(f);
	if (s >= sizeof(buf)) {
		fprintf(stderr, "Warning: Size of '%s' is too big?!\n", filename);
	} else {
		buf[s] = '\0';
		printf("\t'%s'\n", buf);

	}
}

static void print_file_2(const char *filename, const char *prfcmd)
{
	pid_t p;

	print_file(filename);

	if (p = fork()) {
		/* parent */
		wait(NULL);
	} else {
		/* child */
		printf("Child -> exec %s\n", prfcmd);
		execl(prfcmd, prfcmd, filename, NULL);
		printf("Child -> exec FAILED\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	char orig_workdir[4096];
	char *abs_path_to_d1;
	char binpath[4096];

	if (argc == 2) {
		/* subprocess: Just print the file and exit. */
		print_file(argv[1]);
		exit(0);
	}

	mkdir("./Chroottest-dir",0755);
	chdir("./Chroottest-dir");

	getcwd(orig_workdir, sizeof(orig_workdir));
	printf("orig_workdir='%s'\n", orig_workdir);
	
	mkdir("./d1",0755);
	mkdir("./d1/d2",0755);

	printf("argv[0]='%s'\n", argv[0]);
	snprintf(binpath, sizeof(binpath), "../%s", argv[0]);
	link(binpath, "./prf");
	link(binpath, "./d1/prf");
	link(binpath, "./d1/d2/prf");

	write_file("./a", "a in workdir");
	write_file("./d1/a", "a in d1");
	write_file("./d1/d2/a", "a in d2");

	printf("Not yet chrooted:\n");
	printf("Here file 'a' should contain 'a in workdir':\n");
	print_file_2("./a","./prf");

	printf("chdir(/):\n");
	if (chdir("/") < 0) {
		printf("Error: chdir(/) failed");
	} else {
		printf("chdir OK\n");
	}

	asprintf(&abs_path_to_d1, "%s/d1", orig_workdir);
	printf("\nchroot(orig_workdir/d1) (%s):\n", abs_path_to_d1);
	
	if (chroot(abs_path_to_d1) < 0) {
		printf("Error: chroot(%s) failed", abs_path_to_d1);
	} else {
		printf("chroot OK\n");
	}

	printf("Here file '/a' should contain 'a in d1':\n");
	print_file_2("/a","/prf");
	printf("And file 'd2/a' should contain 'a in d2':\n");
	print_file_2("/d2/a","/prf");

	printf("\nSecond chroot, chroot(/d2):\n");
	if (chroot("/d2") < 0) {
		printf("Error: chroot(/d2) failed");
	} else {
		printf("chroot OK\n");
	}
	printf("Here file '/a' should contain 'a in d2':\n");
	print_file_2("/a","/prf");
	
	printf("\nThird chroot, chroot(/d3) (which doesn't exist):\n");
	if (chroot("/d3") < 0) {
		printf("chroot failed, and that is OK\n");
		perror("errno=");
	} else {
		printf("Error: chroot(/d3) didn't fail!\n");
	}
	printf("\nYet another failing chroot, chroot(/a) (a is a file):\n");
	if (chroot("/a") < 0) {
		printf("chroot failed, and that is OK\n");
		perror("errno=");
	} else {
		printf("Error: chroot(/a) didn't fail!\n");
	}
	
	printf("chroot(.), should deactivate the effect:\n");
	if (chroot(".") < 0) {
		printf("Error: chroot(.) failed");
	} else {
		printf("chroot OK\n");
	}

	printf("chdir(orig_workdir)\n");
	if (chdir(orig_workdir) < 0) {
		printf("Error: chdir(orig_workdir) failed");
	} else {
		printf("chdir OK\n");
	}

	printf("Again, file 'a' should contain 'a in workdir':\n");
	print_file_2("a","./prf");

	printf("\n2nd time: chroot(./d1):\n");
	if (chroot("./d1") < 0) {
		printf("Error: chroot(./d1) failed");
	} else {
		printf("chroot OK\n");
	}

	printf("Here file '/a' should contain 'a in d1':\n");
	print_file_2("/a","/prf");

	printf("Now chroot(.) should take to orig_workdir\n");
	if (chroot(".") < 0) {
		printf("Error: chroot(.) failed");
	} else {
		printf("chroot OK\n");
	}

	printf("Again, file 'a' should contain 'a in workdir':\n");
	print_file_2("a","./prf");

	printf("chdir(/d1/d2)\n");
	if (chdir("/d1/d2") < 0) {
		printf("Error: chdir(/d1/d2) failed");
	} else {
		printf("chdir OK\n");
	}

	printf("Here file 'a' should contain 'a in d2':\n");
	print_file_2("a","./prf");
}
 
