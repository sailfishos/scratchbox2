#include <sys/types.h>
#include <unistd.h>

class uidFS: public FUSE::LocalFS {

  void pre() {
    struct fuse_context *fc=fuse_get_context();
    setgid(fc->gid);
    setuid(fc->uid);
  }
public:
  int getattr (const char *path, struct stat *stbuf){
    pre();
    return FUSE::LocalFS::getattr (path, stbuf);
  }
  int access (const char *path, int mask) {
    pre();
    return FUSE::LocalFS::access (path, mask);
  }
  int readlink (const char *path, char *buf, size_t size) {
    pre();
    return FUSE::LocalFS::readlink(path,buf,size);
  }
  int readdir (const char *path, void *buf, fuse_fill_dir_t filler,
		 off_t offset, struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::readdir(path,buf,filler,offset,fi);
  }

  int mknod (const char *path, mode_t mode, dev_t rdev) {
    pre();
    return FUSE::LocalFS::mknod(path,mode,rdev);
  }
  
  int mkdir (const char *path, mode_t mode) {
    pre();
      return FUSE::LocalFS::mkdir (path, mode);
  }
   
  int unlink (const char *path) {
    pre();
    return FUSE::LocalFS::unlink (path);;
  }
  
  int rmdir (const char *path) {
    pre();
    return FUSE::LocalFS::rmdir (path);;
  }

  int symlink (const char *from, const char *to) {
    pre();
    return FUSE::LocalFS::symlink (from, to);
  }

  int rename (const char *from, const char *to) {
    pre();
    return FUSE::LocalFS::rename (from, to);
  }
  int link (const char *from, const char *to) {
    pre();
    return FUSE::LocalFS::link (from, to);
  }
  int chmod (const char *path, mode_t mode) {
    pre();
    return FUSE::LocalFS::chmod (path, mode);
  }

  int chown (const char *path, uid_t uid, gid_t gid) {
    pre();
    return FUSE::LocalFS::chown (path, uid, gid) ? -errno : 0;
  }

  int truncate (const char *path, off_t size) {
    pre();
    return FUSE::LocalFS::truncate (path, size);
  }
  int utime (const char *path, struct utimbuf *buf) {
    pre();
    return FUSE::LocalFS::utime (path, buf);
  }
  
  int open (const char *path, struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::open(path,fi);
  }
  int read (const char *path, char *buf, size_t size, off_t offset,
	      struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::read(path,buf,size,offset,fi);
  }
  int write (const char *path, const char *buf, size_t size,
	     off_t offset, struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::write(path,buf,size,offset,fi);
  }
  int statfs (const char *path, struct statvfs *stbuf) {
    pre();
    return FUSE::LocalFS::statfs (path, stbuf);
  }

  int release (const char *path, struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::release(path,fi);
  }

  int fsync (const char *path, int isdatasync, struct fuse_file_info *fi) {
    pre();
    return FUSE::LocalFS::fsync(path,isdatasync,fi);
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
		  size_t size, int flags) {
      pre();
      return FUSE::LocalFS::setxattr(path,name,value,size,flags);
    }
    int getxattr (const char *path, const char *name, char *value, size_t size) {
      pre();
      return FUSE::LocalFS::getxattr(path,name,value,size);
    }
    int listxattr (const char *path, char *list, size_t size) {
      pre();
      return FUSE::LocalFS::listxattr(path,list,size);
    }
    int removexattr (const char *path, const char *name) {
      pre();
      return FUSE::LocalFS::removexattr(path,name);
    }
#endif // HAVE_XATTR
};
