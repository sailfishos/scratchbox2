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

#ifndef FUSE_HPP_
#define FUSE_HPP_

#define FUSE_USE_VERSION 25 // TODO (It is compatible with FUSE 2.5 and 2.6.0-pre1)

#include <fuse/fuse.h>

#include <errno.h>

#include <string>

#include <sys/stat.h>		// needed?

namespace FUSE
{

/** @defgroup FUSE FUSE++ - FUSE library C++ wrapper
 *
 * This is an advanced (thick) <a href="http://fuse.sourceforge.net">FUSE</a>
 * (Filesystem in Userspace) C++ wrapper.
 *
 * Use this library for creation of new filesystems with C++.
 */
/*@{*/

    class BaseFilesystem;

// Internal
  extern "C"
    int Filesystem_process_options_wrapper (void *data, const char *arg, int key, struct fuse_args *outargs);

// Internal
  struct FilesystemOptionsProcessor
  {
    BaseFilesystem &_fs;
    FilesystemOptionsProcessor (BaseFilesystem &fs) : _fs (fs) { }
    virtual int process_options (BaseFilesystem &fs, const char *arg,
				 int key, struct fuse_args &outargs) const = 0;
  };

// Normally there are no more than one such object in an executable.
  class BaseFilesystem
  {
    int bad ()
    {
      throw "Undefined BaseFilesystem method call"; // I do not bother to include <exception>
      return -EOPNOTSUPP;	/*-ENOTSUP*/// to prevent compiler warnings
    }
  public:
    virtual ~BaseFilesystem() { }

    // not stable API, currently calls exit()
    void pre_usage    (const char *prog, struct fuse_args *outargs = 0);
    void post_usage   (const char *prog, struct fuse_args *outargs = 0);
    virtual void usage(const char *prog, struct fuse_args *outargs = 0) { };
    void print_version(const char *prog, struct fuse_args *outargs = 0);

    // Derived classes should NOT call base class versions of this function.
    int process_options (const char *arg, int key, struct fuse_args *outargs)
    {
      switch (key)
	{
	case FUSE_OPT_KEY_OPT:
	  if (!std::strcmp (arg, "-h") || !std::strcmp (arg, "--help"))
	    {
              pre_usage  (outargs->argv[0], outargs);
              usage      (outargs->argv[0], outargs);
              post_usage (outargs->argv[0], outargs);
              return 0; // really no-op after exit()
	    }
	  if (!std::strcmp (arg, "-V") || !std::strcmp (arg, "--version"))
	    {
              print_version(outargs->argv[0], outargs);
              return 0; // really no-op after exit()
	    }
	  break;
	}
      return 1;
    }
//    int opt_parse (struct fuse_args &args)
//    {
//      return BaseFilesystem::opt_parse_tmpl<Base, BaseFilesystem> (args);
//    }

    // For derived classes
    template <class OurBase, class Derived>
      int opt_parse_tmpl (struct fuse_args &args)
    {
      bool ok = !BaseFilesystem::do_opt_parse <Derived> (args, 0) &&
	!static_cast<OurBase &>(*this).opt_parse (args);
        return ok ? 0 : -1;
    }

    int opt_parse (struct fuse_args &args)
    {
      return BaseFilesystem::do_opt_parse <BaseFilesystem> (args, 0);
    }

    template <class Filesystem>
      int do_opt_parse (struct fuse_args &args, const struct fuse_opt opts[] = 0)
      {
        struct processor_t: FilesystemOptionsProcessor
        {
          processor_t (BaseFilesystem &fs) : FilesystemOptionsProcessor (fs) { }
          // Override:
          int process_options (BaseFilesystem &fs, const char *arg, int key, struct fuse_args &outargs) const
          {
            return static_cast<Filesystem &>(fs).process_options (arg, key, &outargs);
          }
        };
        processor_t proc (*this);
        return ::fuse_opt_parse (&args, (void*)&proc, opts, &Filesystem_process_options_wrapper);
      }

    int before_init() { return 0; } // my extension to FUSE API

    // TODO: Think, shouldn't we use std::string for file names?
    int getattr (const char *name, struct stat *st) { return bad (); }
    int readlink (const char *name, char *buf, size_t size) { return bad (); }
    int getdir (const char *name, fuse_dirh_t dh, fuse_dirfil_t fill) { return bad (); }
    int mknod (const char *name, mode_t mode, dev_t dev) { return bad (); }
    int mkdir (const char *name, mode_t mode) { return bad (); }
    int unlink (const char *name) { return bad (); }
    int rmdir (const char *name) { return bad (); }
    int symlink (const char *from, const char *to) { return bad (); }
    int rename (const char *from, const char *to) { return bad (); }
    int link (const char *from, const char *to) { return bad (); }
    int chmod (const char *name, mode_t mode) { return bad (); }
    int chown (const char *name, uid_t uid, gid_t gid) { return bad (); }
    int truncate (const char *name, off_t off) { return bad (); }
    int utime (const char *name, struct utimbuf *time) { return bad (); }
    int open (const char *name, struct fuse_file_info *info) { return bad (); }
    int read (const char *name, char *buf, size_t size, off_t off, struct fuse_file_info *info) { return bad (); }
    int write (const char *name, const char *buf, size_t size, off_t off, struct fuse_file_info *info) { return bad (); }
    int statfs (const char *name, struct statvfs *buf) { return bad (); }
    int flush (const char *name, struct fuse_file_info *info) { return bad (); }
    int release (const char *name, struct fuse_file_info *info) { return bad (); }
    int fsync (const char *name, int datasync, struct fuse_file_info *info) { return bad (); }
    int setxattr (const char *name, const char *attr, const char *value, size_t size, int flags) { return bad (); }
    int getxattr (const char *name, const char *attr, char *value, size_t size) { return bad (); }
    int listxattr (const char *name, char *attr, size_t size) { return bad (); }
    int removexattr (const char *name, const char *attr) { return bad (); }
    int opendir (const char *name, struct fuse_file_info *info) { return bad (); }
    int readdir (const char *name, void *buf, fuse_fill_dir_t fill, off_t off, struct fuse_file_info *info) { return bad (); }
    int releasedir (const char *name, struct fuse_file_info *info) { return bad (); }
    int fsyncdir (const char *name, int datasync, struct fuse_file_info *info) { return bad (); }
    void *init () { return (void *) 0; }
    void destroy (void *) { }
    int access (const char *name, int mode) { return bad (); }
    int create (const char *name, mode_t mode, struct fuse_file_info *info) { return bad (); }
    int ftruncate (const char *name, off_t off, struct fuse_file_info *info) { return bad (); }
    int fgetattr (const char *name, struct stat *st, struct fuse_file_info *info) { return bad (); }

    typedef int (*getattr_func_t) (const char *, struct stat *);
    typedef int (*readlink_func_t) (const char *, char *, size_t);
    typedef int (*getdir_func_t) (const char *, fuse_dirh_t, fuse_dirfil_t);
    typedef int (*mknod_func_t) (const char *, mode_t, dev_t);
    typedef int (*mkdir_func_t) (const char *, mode_t);
    typedef int (*unlink_func_t) (const char *);
    typedef int (*rmdir_func_t) (const char *);
    typedef int (*symlink_func_t) (const char *, const char *);
    typedef int (*rename_func_t) (const char *, const char *);
    typedef int (*link_func_t) (const char *, const char *);
    typedef int (*chmod_func_t) (const char *, mode_t);
    typedef int (*chown_func_t) (const char *, uid_t, gid_t);
    typedef int (*truncate_func_t) (const char *, off_t);
    typedef int (*utime_func_t) (const char *, struct utimbuf *);
    typedef int (*open_func_t) (const char *, struct fuse_file_info *);
    typedef int (*read_func_t) (const char *, char *, size_t, off_t, struct fuse_file_info *);
    typedef int (*write_func_t) (const char *, const char *, size_t, off_t, struct fuse_file_info *);
    typedef int (*statfs_func_t) (const char *, struct statvfs *);
    typedef int (*flush_func_t) (const char *, struct fuse_file_info *);
    typedef int (*release_func_t) (const char *, struct fuse_file_info *);
    typedef int (*fsync_func_t) (const char *, int, struct fuse_file_info *);
    typedef int (*setxattr_func_t) (const char *, const char *, const char *, size_t, int);
    typedef int (*getxattr_func_t) (const char *, const char *, char *, size_t);
    typedef int (*listxattr_func_t) (const char *, char *, size_t);
    typedef int (*removexattr_func_t) (const char *, const char *);
    typedef int (*opendir_func_t) (const char *, struct fuse_file_info *);
    typedef int (*readdir_func_t) (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *);
    typedef int (*releasedir_func_t) (const char *, struct fuse_file_info *);
    typedef int (*fsyncdir_func_t) (const char *, int, struct fuse_file_info *);
    typedef void *(*init_func_t) ();
    typedef void (*destroy_func_t) (void *);
    typedef int (*access_func_t) (const char *, int);
    typedef int (*create_func_t) (const char *, mode_t, struct fuse_file_info *);
    typedef int (*ftruncate_func_t) (const char *, off_t, struct fuse_file_info *);
    typedef int (*fgetattr_func_t) (const char *, struct stat *, struct fuse_file_info *);
  }; // BaseFilesystem

  template <class Base> class SubdirectoryFilesystem: public Base
  {
    std::string _base_dir;	// ends with slash
    class ZC
    {
      const std::string _str;
    public:
      ZC (const std::string & str) : _str (str) { }
      operator const char *() { return _str.c_str (); }
    };
    ZC z (const char *name) { return ZC (_base_dir + name); }
  public:
    SubdirectoryFilesystem () { }
    SubdirectoryFilesystem (const std::string &base_dir)
    {
      set_base_dir (base_dir);
    }

    void usage(const char *prog, struct fuse_args *outargs = 0); // override
    int process_options (const char *arg, int key, struct fuse_args *outargs)
    {
      switch (key)
        {
        case FUSE_OPT_KEY_OPT:
          if (!std::strncmp (arg, "--", 2)) arg += 2;
          if (!std::strncmp (arg, "path=", 5))
            {
              set_base_dir (arg + 5);
              return 0;
            }
          break;
        }
      return 1;
    }

    int opt_parse (struct fuse_args &args)
    {
      return BaseFilesystem::opt_parse_tmpl<Base, SubdirectoryFilesystem> (args);
    }

    std::string get_base_dir () { return _base_dir; }

    // Must be not called after any file operations.
    int set_base_dir (const std::string &base_dir);
    int set_base_dir (const char *base_dir)
    {
      return set_base_dir (std::string (base_dir));
    }

    int before_init()
    {
      /* block */ {
        const int res = Base::before_init();
        if (res) return res;
      }
      if(!_base_dir.empty())
        {
          struct stat st;
          // _base_dir has trailing slash... OK when not directory?
          // On Linux 2.6.15.4 it produces ENOTDIR (as it should).
          const int res = Base::getattr(_base_dir.c_str(), &st);
          if (res) return res;
          if(!S_ISDIR(st.st_mode)) return -ENOTDIR;
        }
      return 0;
    }

    int getattr (const char *name, struct stat *st)
    {
      return Base::getattr (z (name), st);
    }
    int readlink (const char *name, char *buf, size_t size)
    {
      return Base::readlink (z (name), buf, size);
    }
    int getdir (const char *name, fuse_dirh_t dh, fuse_dirfil_t fill)
    {
      return Base::getdir (z (name), dh, fill);
    }
    int mknod (const char *name, mode_t mode, dev_t dev)
    {
      return Base::mknod (z (name), mode, dev);
    }
    int mkdir (const char *name, mode_t mode)
    {
      return Base::mkdir (z (name), mode);
    }
    int unlink (const char *name)
    {
      return Base::unlink (z (name));
    }
    int rmdir (const char *name)
    {
      return Base::rmdir (z (name));
    }
    int symlink (const char *from, const char *to) // to may be relative // TODO: Should add prefix if 'to' starts with '/'?
    {
      return Base::symlink (from, z(to));
    }
    int rename (const char *from, const char *to)
    {
      return Base::rename (z (from), z (to));
    }
    int link (const char *from, const char *to)
    {
      return Base::link (z (from), z (to));
    }
    int chmod (const char *name, mode_t mode)
    {
      return Base::chmod (z (name), mode);
    }
    int chown (const char *name, uid_t uid, gid_t gid)
    {
      return Base::chown (z (name), uid, gid);
    }
    int truncate (const char *name, off_t off)
    {
      return Base::truncate (z (name), off);
    }
    int utime (const char *name, struct utimbuf *time)
    {
      return Base::utime (z (name), time);
    }
    int open (const char *name, struct fuse_file_info *info)
    {
      return Base::open (z (name), info);
    }
    int read (const char *name, char *buf, size_t size, off_t off, struct fuse_file_info *info)
    {
      return Base::read (z (name), buf, size, off, info);
    }
    int write (const char *name, const char *buf, size_t size, off_t off, struct fuse_file_info *info)
    {
      return Base::write (z (name), buf, size, off, info);
    }
    int statfs (const char *name, struct statvfs *buf)
    {
      return Base::statfs (z (name), buf);
    }
    int flush (const char *name, struct fuse_file_info *info)
    {
      return Base::flush (z (name), info);
    }
    int release (const char *name, struct fuse_file_info *info)
    {
      return Base::release (z (name), info);
    }
    int fsync (const char *name, int datasync, struct fuse_file_info *info)
    {
      return Base::fsync (z (name), datasync, info);
    }
    int setxattr (const char *name, const char *attr, const char *value, size_t size, int flags)
    {
      return Base::setxattr (z (name), attr, value, size, flags);
    }
    int getxattr (const char *name, const char *attr, char *value, size_t size)
    {
      return Base::getxattr (z (name), attr, value, size);
    }
    int listxattr (const char *name, char *attr, size_t size)
    {
      return Base::listxattr (z (name), attr, size);
    }
    int removexattr (const char *name, const char *attr)
    {
      return Base::removexattr (z (name), attr);
    }
    int opendir (const char *name, struct fuse_file_info *info)
    {
      return Base::opendir (z (name), info);
    }
    int readdir (const char *name, void *buf, fuse_fill_dir_t fill, off_t off, struct fuse_file_info *info)
    {
      return Base::readdir (z (name), buf, fill, off, info);
    }
    int releasedir (const char *name, struct fuse_file_info *info)
    {
      return Base::releasedir (z (name), info);
    }
    int fsyncdir (const char *name, int datasync, struct fuse_file_info *info)
    {
      return Base::fsyncdir (z (name), datasync, info);
    }
    void *init ()
    {
      return Base::init ();
    }
    //void destroy(void *ptr) { destroy(ptr); }
    int access (const char *name, int mode)
    {
      return Base::access (z (name), mode);
    }
    int create (const char *name, mode_t mode, struct fuse_file_info *info)
    {
      return Base::create (z (name), mode, info);
    }
    int ftruncate (const char *name, off_t off, struct fuse_file_info *info)
    {
      return Base::ftruncate (z (name), off, info);
    }
    int
      fgetattr (const char *name, struct stat *st, struct fuse_file_info *info)
    {
      return Base::fgetattr (z (name), st, info);
    }
  };

  class FilesystemMisc
  {
  public:
    static inline void mtime_to_atime_ctime (struct stat *st)
    {
      st->st_atime = st->st_ctime = st->st_mtime;
    }
  };

#if 0
  class FUSEApp
  {
    BaseFilesystem &_fs;
  public:
    FUSEApp (BaseFilesystem &fs) : _fs(fs) { }
    bool init()
    {
      ::umask (0); // correct?
      const int res = _fs.before_init ();
      if (res)
        {
          errno = -res;
          ::perror ("Filesystem initialization");
          return false;
        }
      return true;
    }
  };
#endif // 0

/*@}*/

} // namespace FUSE

#include "fuse.cc"		// TODO

#endif // FUSE_HPP_
