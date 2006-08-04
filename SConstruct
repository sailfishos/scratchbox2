env = Environment(CCFLAGS = "-I./include")

Export("env")


funcs = [
"__lxstat","__lxstat64","__open","__open64","__opendir2","__xmknod","__xstat",
"__xstat64","_xftw","_xftw64","access","acct","canonicalize_file_name","chdir",
"chmod","chown","chroot","creat","creat64","dlmopen","dlopen","euidaccess",
"execl","execle","execlp","execv","execve","execvp","fopen","fopen64",
"freopen","freopen64","fts_open","ftw","ftw64","get_current_dir_name","getcwd",
"getwd","getxattr","glob","glob64","glob_pattern_p","lchmod","lchown",
"lckpwdf","lgetxattr","link","listxattr","llistxattr","lremovexattr",
"lsetxattr","lstat","lstat64","lutimes","mkdir","mkdtemp","mknod","mkfifo",
"mkstemp","mkstemp64","mktemp","nftw","nftw64","open","open64","opendir",
"pathconf","readlink","realpath","remove","removexattr","rename","revoke",
"rmdir","scandir","scandir64","setenv","setxattr","stat","stat64","strchrnul",
"symlink","tempnam","tmpnam","truncate","truncate64","unlink","ulckpwdf",
"utime","utimes",
]

headers= [ ("sys/xattr.h", "HAVE_XATTR"),
           ("fcntl.h", "HAVE_FCNTL_H"),
           ("fts.h", "HAVE_FTS_H"),
           ("ftw.h", "HAVE_FTW_H"),
           ("shadow.h", "HAVE_SHADOW_H"),
           ("stdlib.h", "HAVE_STDLIB_H"),
           ("string.h", "HAVE_STRING_H"),
           ("unistd.h", "HAVE_UNISTD_H"),
           ("utime.h", "HAVE_UTIME_H"),
           ("sys/xattr.h", "HAVE_SYS_XATTR_H")]



def build_configh(target):
    env = Environment()
    conf = Configure(env)
    chdr=open(target,"w")
    chdr.write('#define PACKAGE_VERSION "1.99b"\n')
    chdr.write('#define PACKAGE "SB2"\n')
    chdr.write("#include <config_hardcoded.h>\n")
    for f in funcs:
        if conf.CheckFunc(f):
            chdr.write("#define HAVE_%s 1\n"%f.upper())
        else:
            chdr.write("#undef HAVE_%s\n"%f.upper())
    for h in headers:
        if conf.CheckCHeader(h[0]):
            chdr.write("#define %s 1\n"%h[1])
        else:
            chdr.write("#undef %s\n"%h[1])
    chdr.close()
    env=conf.Finish()

build_configh("include/config.h")

#KLUDGE: Just for cleaning include/config.h
#def nullOp(target,source,env):
#    pass
#bld=Builder(action=nullOp)
#noise_env=Environment(BUILDERS = {'Noise': bld})
#noise_env.Noise("include/config.h",'SConstruct')


SConscript(["lua/SConscript", 
	"preload/SConscript", 
#	"fuse/SConscript", 
	"utils/SConscript"])

