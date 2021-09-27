#!/usr/bin/env python

from Cheetah.Template import Template


curry = lambda func, *args, **kw:\
    lambda *p, **n:\
    func(*args + p, **dict(kw.items() + n.items()))


@curry(curry,curry)
def b_map(key,f):
    f.emit_nomap=True
    f.mapkey=key
    if key not in f.argnames:
        raise "map key missing from args"
    f.argnames[f.argnames.index(key)]='mapped__'+key
    return dict(
    defs="\t$map_defs('%s')"%key,
    pre = "\t$map_pre('%s')"%key,
    post = "\t$map_post('%s')"%key
    )

@curry(curry,curry)
def map_at(fd,key,f):
    entry=b_map(key)(f)
    f.mapfd=fd
    entry['pre']="$mapat_pre($f)"
    return entry

@curry(curry,curry)
def optional_arg_is_create_mode(cond,f):
    if '...' not in f.argnames:
        raise 'Optarg missing...'
    f.optarg_cond=cond
    ind = f.argnames.index('...')
    f.optarg_prev=f.argnames[ind-1]
    f.argnames[ind]='mode'
    f.argnames_orig[ind]='mode'
    return dict(post=' ',defs='$optarg_defs',pre='$optarg_pre($f)',nomap=True)

def create_nomap_nolog_version(f):
    f.emit_nomap_nolog=True

def pass_va_list(f):
    if '...' not in f.argnames:
        raise 'Optarg missing...'
    ind = f.argnames.index('...')
    f.optarg_prev=f.argnames[ind-1]
    del f.argnames[ind]
    del f.argnames_orig[ind]
    return dict(defs='$vaskip_defs',pre='$vaskip_pre($f)',nomap=True, post='$vaskip_post')

class Function(object):
    def __init__(self,ret,name,args,brains=None,argnames=None):
        self.ret=ret
        self.name=name
        self.args=args
        if argnames is None: #TODO: Make a better parser
            self.argnames=[x.split(' ')[-1].split('*')[-1].split('[')[0] for x in args.split(',')]
        else:
            self.argnames=argnames
        self.argnames_orig=self.argnames[:]
        self.extra=[]
        self.emit_nomap=False
        self.emit_nomap_nolog=False
        if brains:
            for b in brains:
                stuff=b(self)
                if stuff:
                    self.extra.append(stuff)

wraps=[]
gates=[]

def F(lst,s,*brains,**kw):
    """Helper to parse ugly syntax"""
    name=s.split('(')[0].split()[-1].split('*')[-1]
    ret=s[:s.index(name)]
    args=s.split('(',1)[-1].rsplit(')',1)[0]
    argnames=None
    if 'argnames' in kw:
        argnames=kw['argnames']
    f = Function(ret,name,args,brains,argnames)
    lst.append(f)
    return f

W=curry(F,wraps)
G=curry(F,gates)



W('char* tmpnam(char *s)')
W('char *mktemp (char *template)')
W('int __open(const char *pathname, int flags, ...)', b_map("pathname"),optional_arg_is_create_mode('flags&O_CREAT'))
W('int __open64(const char *pathname, int flags, ...)', b_map("pathname"),optional_arg_is_create_mode('flags&O_CREAT'))
W('int open(const char *pathname, int flags, ...)', b_map("pathname"),optional_arg_is_create_mode('flags&O_CREAT'),create_nomap_nolog_version)
W('int open64(const char *pathname, int flags, ...)', b_map("pathname"),optional_arg_is_create_mode('flags&O_CREAT'))
W('int openat(int dirfd, const char *pathname, int flags, ...)',map_at("dirfd","pathname"),optional_arg_is_create_mode('flags&O_CREAT'))
W('int openat64(int dirfd, const char *pathname, int flags, ...)', map_at("dirfd","pathname"),optional_arg_is_create_mode('flags&O_CREAT'))
W('int symlinkat(const char *oldpath, int newdirfd, const char *newpath)', b_map("oldpath"),map_at("newdirfd","newpath"))
W('int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)', map_at("olddirfd","oldpath"),map_at("newdirfd","newpath"))
W('int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)',map_at("olddirfd","oldpath"),map_at("newdirfd","newpath"))
W('int __lxstat(int ver, const char *filename, struct stat *buf)', b_map("filename"))
W('int __lxstat64(int ver, const char *filename, struct stat64 *buf)', b_map("filename"))
W('DIR *__opendir2(const char *name, int flags)', b_map("name"))
W('int __xmknod(int ver, const char *path, mode_t mode, dev_t *dev)', b_map("path"))
W('int __xstat(int ver, const char *filename, struct stat *buf)', b_map("filename"))
W('int __xstat64(int ver, const char *filename, struct stat64 *buf)', b_map("filename"))
W('int _xftw(int mode, const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)', b_map("dir"))
W('int _xftw64(int mode, const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)', b_map("dir"))
W('int access(const char *pathname, int mode)', b_map("pathname"), create_nomap_nolog_version)
W('int acct(const char *filename)', b_map("filename"))
W('char *canonicalize_file_name(const char *name)', b_map("name"))
W('int chdir(const char *path)', b_map("path"))
W('int chmod(const char *path, mode_t mode)', b_map("path"))
W('int chown(const char *path, uid_t owner, gid_t group)', b_map("path"))
W('int creat(const char *pathname, mode_t mode)', b_map("pathname"))
W('int creat64(const char *pathname, mode_t mode)', b_map("pathname"))
W('void *dlmopen(Lmid_t nsid, const char *filename, int flag)', b_map("filename"))
W('void *dlopen(const char *filename, int flag)', b_map("filename"))
W('int euidaccess(const char *pathname, int mode)', b_map("pathname"))
W('int faccessat(int dirfd, const char *pathname, int mode, int flags)', map_at("dirfd","pathname"))
W('int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags)', map_at("dirfd","pathname"))
W('int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)', map_at("dirfd","pathname"))
W('FILE *fopen(const char *path, const char *mode)', b_map("path"))
W('FILE *fopen64(const char *path, const char *mode)', b_map("path"))
W('FILE *freopen(const char *path, const char *mode, FILE *stream)', b_map("path"))
W('FILE *freopen64(const char *path, const char *mode, FILE *stream)', b_map("path"))
W('int fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)', map_at("dirfd","pathname"))
W('int ftw(const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag), int nopenfd)', b_map("dir"))
W('int ftw64(const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag), int nopenfd)', b_map("dir"))
W('int futimesat(int dirfd, const char *pathname, const struct timeval times[2])', map_at("dirfd","pathname"))
W('ssize_t getxattr(const char *path, const char *name, void *value, size_t size)', b_map("path"))
W('int glob_pattern_p(const char *pattern, int quote)', b_map("pattern"))
W('int lchmod(const char *path, mode_t mode)', b_map("path"))
W('int lchown(const char *path, uid_t owner, gid_t group)', b_map("path"))
W('ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size)', b_map("path"))
W('int link(const char *oldpath, const char *newpath)', b_map("oldpath"), b_map("newpath"))
W('ssize_t listxattr(const char *path, char *list, size_t size)', b_map("path"))
W('ssize_t llistxattr(const char *path, char *list, size_t size)', b_map("path"))
W('int lremovexattr(const char *path, const char *name)', b_map("path"))
W('int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags)', b_map("path"))
W('int lstat(const char *file_name, struct stat *buf)', b_map("file_name"))
W('int lstat64(const char *file_name, struct stat64 *buf)', b_map("file_name"))
W('int lutimes(const char *filename, const struct timeval tv[2])', b_map("filename"),argnames=['filename','tv'])
W('int mkdir(const char *pathname, mode_t mode)', b_map("pathname"))
W('int mkdirat(int dirfd, const char *pathname, mode_t mode)', map_at("dirfd","pathname"))
W('int mkfifo(const char *pathname, mode_t mode)', b_map("pathname"))
W('int mkfifoat(int dirfd, const char *pathname, mode_t mode)', map_at("dirfd","pathname"))
W('int mknod(const char *pathname, mode_t mode, dev_t dev)', b_map("pathname"))
W('int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev)', map_at("dirfd","pathname"))
W('int mkstemp(char *template)', b_map("template"))
W('int mkstemps(char *template, int suffixlen)', b_map("template"))
W('int mkstemp64(char *template)', b_map("template"))
W('int mkstemps64(char *template, int suffixlen)', b_map("template"))
W('int nftw(const char *dir, int (*fn)(const char *file, const struct stat *sb, int flag, struct FTW *s), int nopenfd, int flags)', b_map("dir"),argnames=['dir','fn','flag','s'])
W('int nftw64(const char *dir, int (*fn)(const char *file, const struct stat64 *sb, int flag, struct FTW *s), int nopenfd, int flags)', b_map("dir"))
W('DIR *opendir(const char *name)', b_map("name"))
W('long pathconf(const char *path, int name)', b_map("path"))
W('READLINK_TYPE readlink(const char *path, char *buf, size_t bufsize)', b_map("path"))
W('READLINK_TYPE readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize)', map_at("dirfd","pathname"))
W('char *realpath(const char *name, char *resolved)', b_map("name"))
W('int remove(const char *pathname)', b_map("pathname"))
W('int removexattr(const char *path, const char *name)', b_map("path"))
W('int rename(const char *oldpath, const char *newpath)', b_map("oldpath"), b_map("newpath"))
W('int revoke(const char *file)', b_map("file"))
W('int rmdir(const char *pathname)', b_map("pathname"))
W('int scandir(const char *dir, struct dirent ***namelist, int(*filter)(const struct dirent *), int(*compar)(scandir_arg_t *, scandir_arg_t *))', b_map("dir"), argnames=['dir','namelist','filter','compar'])
W('int scandir64(const char *dir, struct dirent64 ***namelist, int(*filter)(const struct dirent64 *), int(*compar)(scandir64_arg_t *, scandir64_arg_t *))', b_map("dir"),argnames=['dir','namelist','filter','compar'])
W('int setxattr(const char *path, const char *name, const void *value, size_t size, int flags)', b_map("path"))
W('int stat(const char *file_name, struct stat *buf)', b_map("file_name"))
W('int stat64(const char *file_name, struct stat64 *buf)', b_map("file_name"))
W('int symlink(const char *oldpath, const char *newpath)', b_map("oldpath"),b_map("newpath"))
W('char *tempnam(const char *dir, const char *pfx)', b_map("dir"))
W('int truncate(const char *path, off_t length)', b_map("path"))
W('int truncate64(const char *path, off64_t length)', b_map("path"))
W('int unlink(const char *pathname)', b_map("pathname"))
W('int unlinkat(int dirfd, const char *pathname, int flags)', map_at("dirfd","pathname"))
W('int utime(const char *filename, const struct utimbuf *buf)', b_map("filename"))
W('int utimes(const char *filename, const struct timeval tv[2])', b_map("filename"),argnames=['filename','timeval'])

G('int execl (const char *path, const char *arg, ...)', pass_va_list)
G('int execle (const char *path, const char *arg, ...)', pass_va_list)
G('int execlp (const char *file, const char *arg, ...)', pass_va_list)
G('int execv (const char *path, char *const argv[])')
G('int execve (const char *filename, char *const argv[], char *const envp[])')
G('int execvp (const char *file, char *const argv[])')
G('char * getcwd (char *buf, size_t size)')
G('char * get_current_dir_name (void)',argnames=[])
G('char * getwd (char *buf)')
G('char *mkdtemp (char *template)')
G('int uname(struct utsname *buf)')
G('FTS * fts_open (char * const *path_argv, int options, int (*compar)(const FTSENT **, const FTSENT **))',
  argnames=['path_argv','options','compar'])
G('int glob (const char *pattern, int flags, int (*errfunc) (const char *, int), glob_t *pglob)', b_map("pattern"),argnames=['pattern','flags','errfunc','pglob'])
G('int glob64 (const char *pattern, int flags, int (*errfunc) (const char *, int), glob64_t *pglob)', b_map("pattern"),
  argnames=['pattern','flags','errfunc','pglob'])


wrappers=Template(file='wrappers_c.tmpl',searchList=[{'funcs':wraps,'gates':gates}])
exported=Template(file='exported_h.tmpl',searchList=[{'funcs':wraps,'gates':gates}])


if __name__ == "__main__":
    
    print >>open('wrappers.c','w'),wrappers
    print >>open('exported.h','w'),exported
