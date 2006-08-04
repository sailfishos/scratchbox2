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

#include "config.h"

#include "../common/fuse.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_XATTR
#  include <sys/xattr.h>
#endif

namespace FUSE
{
    
/** @addtogroup FUSE */
/*@{*/

  class LocalFS : public BaseFilesystem
  {
  public:
    int getattr (const char *path, struct stat *stbuf)
    {
      return ::lstat (path, stbuf) ? -errno : 0;
    }
    int access (const char *path, int mask)
    {
      return ::access (path, mask) ? -errno : 0;
    }
    int readlink (const char *path, char *buf, size_t size)
    {
      int res =::readlink (path, buf, size - 1);
      if (res == -1) return -errno;
      buf[res] = '\0';
      return 0;
    }
    int readdir (const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi)
    {
      // TODO: Not the most efficient.
      (void) offset;
      (void) fi;
      DIR *dp = ::opendir (path);
      if (!dp) return -errno;
      while (struct dirent *de = ::readdir (dp))
	{
	  struct stat st;
	  std::memset (&st, 0, sizeof (st));
	  st.st_ino = de->d_ino;
	  st.st_mode = de->d_type << 12;
	  if (filler (buf, de->d_name, &st, 0)) break;
	}
      ::closedir (dp);
      return 0;
    }
    int mknod (const char *path, mode_t mode, dev_t rdev)
    {
      /* On Linux this could just be 'mknod(path, mode, rdev)' but this
         is more portable */
      int res;
      if (S_ISREG (mode))
	{
	  res = ::open (path, O_CREAT | O_EXCL | O_WRONLY, mode);
	  if (res >= 0) res = ::close (res);
	}
      else
	  res = S_ISFIFO (mode) ? ::mkfifo (path, mode)
                                : ::mknod  (path, mode, rdev);
      return res == -1 ? -errno : 0;
    }
    int mkdir (const char *path, mode_t mode)
    {
      return ::mkdir (path, mode) ? -errno : 0;
    }
    int unlink (const char *path)
    {
      return ::unlink (path) ? -errno : 0;
    }
    int rmdir (const char *path)
    {
      return ::rmdir (path) ? -errno : 0;
    }
    int symlink (const char *from, const char *to)
    {
      return ::symlink (from, to) ? -errno : 0;
    }
    int rename (const char *from, const char *to)
    {
      return ::rename (from, to) ? -errno : 0;
    }
    int link (const char *from, const char *to)
    {
      return ::link (from, to) ? -errno : 0;
    }
    int chmod (const char *path, mode_t mode)
    {
      return ::chmod (path, mode) ? -errno : 0;
    }
    int chown (const char *path, uid_t uid, gid_t gid)
    {
      return ::chown (path, uid, gid) ? -errno : 0;
    }
    int truncate (const char *path, off_t size)
    {
      return ::truncate (path, size) ? -errno : 0;
    }
    int utime (const char *path, struct utimbuf *buf)
    {
      return ::utime (path, buf) ? -errno : 0;
    }
    int open (const char *path, struct fuse_file_info *fi)
    {
      int res = ::open (path, fi->flags);
      if (res == -1) return -errno;
      // TODO: Remember FD (Should not close (also it does not check close() return))
      ::close (res);
      return 0;
    }
    int read (const char *path, char *buf, size_t size, off_t offset,
	      struct fuse_file_info *fi)
    {
      // TODO: Not the most efficient
      (void) fi;
      int fd = ::open (path, O_RDONLY);
      if (fd == -1) return -errno;
      int res = ::pread (fd, buf, size, offset);
      if (res == -1) res = -errno;
      ::close (fd);		// FIXME: return value
      return res;
    }
    int write (const char *path, const char *buf, size_t size,
	       off_t offset, struct fuse_file_info *fi)
    {
      // TODO: Not the most efficient
      (void) fi;
      int fd = ::open (path, O_WRONLY);
      if (fd == -1) return -errno;
      int res = ::pwrite (fd, buf, size, offset);
      if (res == -1) res = -errno;
      ::close (fd);		// FIXME: return value
      return res;
    }
    int statfs (const char *path, struct statvfs *stbuf)
    {
      return ::statvfs (path, stbuf) == -1 ? -errno : 0;
    }
    int release (const char *path, struct fuse_file_info *fi)
    {
      /* Just a stub.  This method is optional and can safely be left
         unimplemented */
      // TODO, however
      (void) path;
      (void) fi;
      return 0;
    }
    int fsync (const char *path, int isdatasync, struct fuse_file_info *fi)
    {
      /* Just a stub.  This method is optional and can safely be left
         unimplemented */
      // TODO, however
      (void) path;
      (void) isdatasync;
      (void) fi;
      return 0;
    }
    // TODO
    //int flush(const char *, struct fuse_file_info *);
    //int releasedir(const char *, struct fuse_file_info *);
    //int fsyncdir(const char *, int, struct fuse_file_info *);
    //int create(const char *, mode_t, struct fuse_file_info *);
    //int ftruncate(const char *, off_t, struct fuse_file_info *);
    //int fgetattr(const char *, struct stat *, struct fuse_file_info *);
#ifdef HAVE_XATTR
    int setxattr (const char *path, const char *name, const char *value,
		  size_t size, int flags)
    {
      int res = ::lsetxattr (path, name, value, size, flags);
      return res == -1 ? -errno : 0;
    }
    int getxattr (const char *path, const char *name, char *value, size_t size)
    {
      int res = ::lgetxattr (path, name, value, size);
      return res == -1 ? -errno : 0;
    }
    int listxattr (const char *path, char *list, size_t size)
    {
      int res = ::llistxattr (path, list, size);
      return res == -1 ? -errno : res;
    }
    int removexattr (const char *path, const char *name)
    {
      return ::lremovexattr (path, name) == -1 ? -errno : 0;
    }
#endif // HAVE_XATTR
  };

/*@}*/

} // namespace FUSE
