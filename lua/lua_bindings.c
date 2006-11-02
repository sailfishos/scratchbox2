/*
 * Copyright (C) 2006 Lauri Leukkunen <lleukkun@cc.hut.fi>
 *
 * Licensed under LGPL version 2, see top level LICENSE file for details.
 */




/*
 * In the path translation functions we must use syscalls directly to 
 * avoid creating recursive loops of function calls due to wrapping
 */

#define _GNU_SOURCE

#include <sys/syscall.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/param.h>
#include <assert.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lua_bindings.h"

#define __set_errno(e) errno = e

pidfunction *sb_getpid=getpid;

void bind_set_getpid(pidfunction *func) {
	sb_getpid=func;
}


static int lua_bind_sb_functions(lua_State *l);
char *__sb2_realpath (const char *name, char *resolved);

/* Lua interpreter */
lua_State *l;

char *rsdir = NULL;
char *main_lua = NULL;



static int sb_realpath(lua_State *l)
{
	char *path;
	char *resolved_path = NULL;
	int n;

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, "sb_realpath(path) - invalid number of parameters");
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	//printf("sb_realpath: [%s]\n", path);

	/* Here we rely on glibc specific feature of passing NULL as the second
	 * parameter to realpath, thus forcing it to allocate a sufficient buffer.
	 * This might not work at all on other systems.
	 */

	resolved_path = __sb2_realpath(path, NULL);
	if (resolved_path == NULL)  {
		lua_pushstring(l, "no such file");
		return 1;
	}

	//printf("sb_realpath: resolved_path = [%s]\n", resolved_path);

	lua_pushstring(l, resolved_path);
	return 1;
}


/*
 * check if the path is a symlink, if yes then return it resolved,
 * if not, return the path intact
 */

static int sb_followsymlink(lua_State *l)
{
	char *path;
	struct stat64 s;
	char link_path[PATH_MAX + 1];
	int n;

	/* printf("in sb_followsymlink\n"); */
	memset(&s, '\0', sizeof(struct stat64));
	memset(link_path, '\0', PATH_MAX + 1);

	n = lua_gettop(l);
	if (n != 1) {
		lua_pushstring(l, "sb_followsymlink(path) - invalid number of parameters");
		return 1;
	}

	path = strdup(lua_tostring(l, 1));

	//printf("C thinks path is: %s\n", path);
	
	if (syscall(__NR_lstat64, path, &s) < 0) {
		/* didn't work
		 * TODO: error handling 
		 */
		//perror("stat failed\n");
		lua_pushstring(l, path);
		goto getout;
	}
	//printf("about to test for symlink: %i\n", s.st_mode);
	if (S_ISLNK(s.st_mode)) {
		/* we have a symlink, read it and return */
		//printf("WE HAVE A SYMLINK!!!\n");
		syscall(__NR_readlink, path, link_path, PATH_MAX);
		lua_pushstring(l, link_path);

	} else {
		//printf("not a symlink! %s\n", path);
		/* not a symlink, return path */
		lua_pushstring(l, path);
		//printf("after pushing\n");
	}
getout:	
	//printf("about to free!\n");
	free(path);
	return 1;
}

/*
 * This function should ONLY look at things from rsdir
 * any other path leads to loops
 */

static int sb_getdirlisting(lua_State *l)
{
	DIR *d;
	struct dirent *de;
	char *path;
	int count;
	
	int n = lua_gettop(l);
	
	if (n != 1) {
		lua_pushstring(l, NULL);
		return 1;
	}

	path = strdup(lua_tostring(l, 1));
	if (strncmp(path, rsdir, strlen(rsdir)) != 0) {
		/* invalid path used */
		lua_pushstring(l, NULL);
		return 1;
	}
	
	if ( (d = opendir(path)) == NULL ) {
		lua_pushstring(l, NULL);
		return 1;
	}
	count = 0;
	lua_newtable(l); /* create a new table on the stack */
	
	while ( (de = readdir(d)) != NULL) { /* get one dirent at a time */
		lua_pushnumber(l, count);
		lua_pushstring(l, de->d_name); /* push the entries to lua stack */
		lua_settable(l, -3);
		count++;
	}
	closedir(d);

	/* make sure lua knows about the table size */
	lua_pushliteral(l, "n");
	lua_pushnumber(l, count - 1);
	lua_rawset(l, -3);
	free(path);
	return 1;
}


char *scratchbox_path(const char *func_name, const char *path)
{	
	char binary_name[PATH_MAX+1];
	char work_dir[PATH_MAX+1];
	char *tmp;
	char pidlink[17]; /* /proc/2^8/exe */
	
	if (!rsdir) {
		rsdir = getenv("SBOX_REDIR_SCRIPTS");
		if (!rsdir) {
			rsdir = "/scratchbox/redir_scripts";
		} else {
			rsdir = strdup(rsdir);
		}
		
		main_lua = calloc(strlen(rsdir) + strlen("/main.lua") + 1, sizeof(char));

		strcpy(main_lua, rsdir);
		strcat(main_lua, "/main.lua");
	}
	
	memset(binary_name, '\0', PATH_MAX+1);
	tmp = getenv("__SB2_BINARYNAME");
	if (tmp) {
		strcpy(binary_name, tmp);
	} else {
		strcpy(binary_name, "DUMMY");
	}
	memset(work_dir, '\0', PATH_MAX+1);
	snprintf(pidlink,16,"/proc/%i/exe",sb_getpid());
//	if (syscall(__NR_readlink, pidlink, binary_name, PATH_MAX) < 0) {
//		perror("__NR_readlink() error, check that /proc is mounted!");
//	}
	syscall(__NR_getcwd, work_dir, PATH_MAX);


	/* RECURSIVE CALL BREAK */
	if (strncmp(path, rsdir, strlen(rsdir)) == 0) {
		return (char *)path;
	}
	
	if (!l) {
				
		l = luaL_newstate();
		
		luaL_openlibs(l);
		lua_bind_sb_functions(l); /* register our sb_ functions */
		switch(luaL_loadfile(l, main_lua)) {
		case LUA_ERRFILE:
			fprintf(stderr, "Error loading %s\n", main_lua);
			exit(1);
		case LUA_ERRSYNTAX:
			fprintf(stderr, "Syntax error in %s\n", main_lua);
			exit(1);
		case LUA_ERRMEM:
			fprintf(stderr, "Memory allocation error while loading %s\n", main_lua);
			exit(1);
		default:
			;
		}
		
		lua_call(l, 0, 0);
	}


	lua_getfield(l, LUA_GLOBALSINDEX, "sbox_translate_path");
	lua_pushstring(l, binary_name);
	lua_pushstring(l, func_name);
	lua_pushstring(l, work_dir);
	lua_pushstring(l, path);
	lua_call(l, 4, 1); /* four arguments, one result */
	tmp = strdup(lua_tostring(l, -1));
	lua_pop(l, 1);
	
	return tmp;
}


/* mappings from c to lua */
static const luaL_reg reg[] =
{
	{"sb_getdirlisting",		sb_getdirlisting},
	{"sb_followsymlink",		sb_followsymlink},
	{"sb_realpath",			sb_realpath},
	{NULL,				NULL}
};


static int lua_bind_sb_functions(lua_State *l)
{
	luaL_register(l, "sb", reg);
	lua_pushliteral(l,"version");
	lua_pushstring(l, "2.0" );
	lua_settable(l,-3);

	return 0;
}


/*
 * __sb_realpath is based directly on the __realpath in glibc-2.4.
 * The only difference is that this uses syscalls directly and 
 * replaces all __functions with their public names.
 */

/* Return the canonical absolute name of file NAME.  A canonical name
   does not contain any `.', `..' components nor any repeated path
   separators ('/') or symlinks.  All path components must exist.  If
   RESOLVED is null, the result is malloc'd; otherwise, if the
   canonical name is PATH_MAX chars or more, returns null with `errno'
   set to ENAMETOOLONG; if the name fits in fewer than PATH_MAX chars,
   returns the name in RESOLVED.  If the name cannot be resolved and
   RESOLVED is non-NULL, it contains the path of the first component
   that cannot be resolved.  If the path can be resolved, RESOLVED
   holds the same value as the value returned.  */

char *
__sb2_realpath (const char *name, char *resolved)
{
  char *rpath, *dest, *extra_buf = NULL;
  const char *start, *end, *rpath_limit;
  long int path_max;
  int num_links = 0;

  if (name == NULL)
    {
      /* As per Single Unix Specification V2 we must return an error if
	 either parameter is a null pointer.  We extend this to allow
	 the RESOLVED parameter to be NULL in case the we are expected to
	 allocate the room for the return value.  */
      __set_errno (EINVAL);
      return NULL;
    }

  if (name[0] == '\0')
    {
      /* As per Single Unix Specification V2 we must return an error if
	 the name argument points to an empty string.  */
      __set_errno (ENOENT);
      return NULL;
    }

#ifdef PATH_MAX
  path_max = PATH_MAX;
#else
  path_max = pathconf (name, _PC_PATH_MAX);
  if (path_max <= 0)
    path_max = 1024;
#endif

  if (resolved == NULL)
    {
      rpath = malloc (path_max);
      if (rpath == NULL)
	return NULL;
    }
  else
    rpath = resolved;
  rpath_limit = rpath + path_max;

  if (name[0] != '/')
    {
      if (!syscall(__NR_getcwd, rpath, path_max))
	{
	  rpath[0] = '\0';
	  goto error;
	}
      dest = strchr (rpath, '\0');
    }
  else
    {
      rpath[0] = '/';
      dest = rpath + 1;
    }

  for (start = end = name; *start; start = end)
    {
      struct stat64 st;
      int n;

      /* Skip sequence of multiple path-separators.  */
      while (*start == '/')
	++start;

      /* Find end of path component.  */
      for (end = start; *end && *end != '/'; ++end)
	/* Nothing.  */;

      if (end - start == 0)
	break;
      else if (end - start == 1 && start[0] == '.')
	/* nothing */;
      else if (end - start == 2 && start[0] == '.' && start[1] == '.')
	{
	  /* Back up to previous component, ignore if at root already.  */
	  if (dest > rpath + 1)
	    while ((--dest)[-1] != '/');
	}
      else
	{
	  size_t new_size;

	  if (dest[-1] != '/')
	    *dest++ = '/';

	  if (dest + (end - start) >= rpath_limit)
	    {
	      ptrdiff_t dest_offset = dest - rpath;
	      char *new_rpath;

	      if (resolved)
		{
		  __set_errno (ENAMETOOLONG);
		  if (dest > rpath + 1)
		    dest--;
		  *dest = '\0';
		  goto error;
		}
	      new_size = rpath_limit - rpath;
	      if (end - start + 1 > path_max)
		new_size += end - start + 1;
	      else
		new_size += path_max;
	      new_rpath = (char *) realloc (rpath, new_size);
	      if (new_rpath == NULL)
		goto error;
	      rpath = new_rpath;
	      rpath_limit = rpath + new_size;

	      dest = rpath + dest_offset;
	    }

	  dest = mempcpy (dest, start, end - start);
	  *dest = '\0';

	  if (syscall(__NR_lstat64, rpath, &st) < 0)
	    goto error;

	  if (S_ISLNK (st.st_mode))
	    {
	      char *buf = alloca (path_max);
	      size_t len;

	      if (++num_links > MAXSYMLINKS)
		{
		  __set_errno (ELOOP);
		  goto error;
		}

	      n = syscall(__NR_readlink, rpath, buf, path_max);
	      if (n < 0)
		goto error;
	      buf[n] = '\0';

	      if (!extra_buf)
		extra_buf = alloca (path_max);

	      len = strlen (end);
	      if ((long int) (n + len) >= path_max)
		{
		  __set_errno (ENAMETOOLONG);
		  goto error;
		}

	      /* Careful here, end may be a pointer into extra_buf... */
	      memmove (&extra_buf[n], end, len + 1);
	      name = end = memcpy (extra_buf, buf, n);

	      if (buf[0] == '/')
		dest = rpath + 1;	/* It's an absolute symlink */
	      else
		/* Back up to previous component, ignore if at root already: */
		if (dest > rpath + 1)
		  while ((--dest)[-1] != '/');
	    }
	  else if (!S_ISDIR (st.st_mode) && *end != '\0')
	    {
	      __set_errno (ENOTDIR);
	      goto error;
	    }
	}
    }
  if (dest > rpath + 1 && dest[-1] == '/')
    --dest;
  *dest = '\0';

  assert (resolved == NULL || resolved == rpath);
  return rpath;

error:
  assert (resolved == NULL || resolved == rpath);
  if (resolved == NULL)
    free (rpath);
  return NULL;
}

