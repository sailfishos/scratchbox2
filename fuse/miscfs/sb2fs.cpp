/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _FILE_OFFSET_BITS 64
#include "../common/localfs.hpp"
#include "uidfs.hpp"

#include <cstdlib> // for EXIT_FAILURE

// For umask()
#include <sys/types.h>
#include <sys/stat.h>

extern "C" {
#include <lua_bindings.h>
}

FUSE::SubdirectoryFilesystem <uidFS> fs;

extern "C"
{

// The below functions are short but there are no reason to make them inline
// because they are used only as function pointer values.

  static int
  my_getattr (const char *path, struct stat *stbuf)
  {
    return fs.getattr (scratchbox_path("getattr",path), stbuf);
  }
  static int
  my_access (const char *path, int mask)
  {
    return fs.access (scratchbox_path("access",path), mask);
  }
  static int
  my_readlink (const char *path, char *buf, size_t size)
  {
    return fs.readlink (scratchbox_path("readlink",path), buf, size);
  }
  static int
  my_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
	      off_t offset, struct fuse_file_info *fi)
  {
    return fs.readdir (scratchbox_path("readdir",path), buf, filler, offset, fi);
  }
  static int
  my_mknod (const char *path, mode_t mode, dev_t rdev)
  {
    return fs.mknod (path, mode, rdev);
  }
  static int
  my_mkdir (const char *path, mode_t mode)
  {
    return fs.mkdir (path, mode);
  }
  static int
  my_unlink (const char *path)
  {
    return fs.unlink (path);
  }
  static int
  my_rmdir (const char *path)
  {
    return fs.rmdir (path);
  }
  static int
  my_symlink (const char *from, const char *to)
  {
    return fs.symlink (scratchbox_path("symlink_from",from), scratchbox_path("symlink_to",to));
  }
  static int
  my_rename (const char *from, const char *to)
  {
    return fs.rename (from, to);
  }
  static int
  my_link (const char *from, const char *to)
  {
    return fs.link (from, to);
  }
  static int
  my_chmod (const char *path, mode_t mode)
  {
    return fs.chmod (path, mode);
  }
  static int
  my_chown (const char *path, uid_t uid, gid_t gid)
  {
    return fs.chown (path, uid, gid);
  }
  static int
  my_truncate (const char *path, off_t size)
  {
    return fs.truncate (path, size);
  }
  static int
  my_utime (const char *path, struct utimbuf *buf)
  {
    return fs.utime (path, buf);
  }
  static int
  my_open (const char *path, struct fuse_file_info *fi)
  {
    return fs.open (scratchbox_path("open",path), fi);
  }
  static int
  my_read (const char *path, char *buf, size_t size, off_t offset,
	   struct fuse_file_info *fi)
  {
    return fs.read (scratchbox_path("read",path), buf, size, offset, fi);
  }
  static int
  my_write (const char *path, const char *buf, size_t size,
	    off_t offset, struct fuse_file_info *fi)
  {
    return fs.write (scratchbox_path("write",path), buf, size, offset, fi);
  }
  static int
  my_statfs (const char *path, struct statvfs *stbuf)
  {
    return fs.statfs (scratchbox_path("stat",path), stbuf);
  }
  static int
  my_release (const char *path, struct fuse_file_info *fi)
  {
    return fs.release (path, fi);
  }
  static int
  my_fsync (const char *path, int isdatasync, struct fuse_file_info *fi)
  {
    return fs.fsync (path, isdatasync, fi);
  }
#ifdef HAVE_SETXATTR
  static int
  my_setxattr (const char *path, const char *name, const char *value,
	       size_t size, int flags)
  {
    return fs.setxattr (path, name, value, size, flags);
  }
  static int
  my_getxattr (const char *path, const char *name, char *value, size_t size)
  {
    return fs.getxattr (path, name, value, size);
  }
  static int
  my_listxattr (const char *path, char *list, size_t size)
  {
    return fs.listxattr (path, list, size);
  }
  static int
  my_removexattr (const char *path, const char *name)
  {
    return fs.removexattr (path, name);
  }
#endif				/* HAVE_SETXATTR */

// I suppose that modern GCC well optimizes it
  struct my_fuse_operations: fuse_operations
  {
    my_fuse_operations ()
    {
      getattr = my_getattr;
      access = my_access;
      readlink = my_readlink;
      readdir = my_readdir;
      mknod = my_mknod;
      mkdir = my_mkdir;
      unlink = my_unlink;
      rmdir = my_rmdir;
      symlink = my_symlink;
      rename = my_rename;
      link = my_link;
      chmod = my_chmod;
      chown = my_chown;
      truncate = my_truncate;
      utime = my_utime;
      open = my_open;
      read = my_read;
      write = my_write;
      statfs = my_statfs;
      release = my_release;
      fsync = my_fsync;
#ifdef HAVE_SETXATTR
      setxattr = my_setxattr;
      getxattr = my_getxattr;
      listxattr = my_listxattr;
      removexattr = my_removexattr;
#endif
    }
  };

  static struct my_fuse_operations oper;

} // extern "C"

pid_t fuse_getpid(void) {
	return fuse_get_context()->pid;
}

int
main (int argc, char *argv[])
{
  struct fuse_args args = FUSE_ARGS_INIT (argc, argv);
  if (fs.opt_parse (args)) return EXIT_FAILURE;
  ::umask (0);
  const int res = fs.before_init ();
  if (res)
    {
      errno = -res;
      ::perror ("Filesystem initialization");
      return EXIT_FAILURE;
    }
  bind_set_getpid(fuse_getpid);
  return ::fuse_main (args.argc, args.argv, &oper);
}
