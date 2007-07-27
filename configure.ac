# Copyright (C) 2006 Lauri Leukkunen
# Based on the configure.ac script from fakechroot package


AC_PREREQ(2.59)
AC_INIT([sb2], [1.99b], [lle@rahina.org])
#AC_CANONICAL_TARGET
#AM_INIT_AUTOMAKE
#AM_MAINTAINER_MODE
#AC_CONFIG_SRCDIR([src/libfakechroot.c])
AC_CONFIG_HEADER([include/config.h])
AC_CONFIG_FILES([config.mak])
AC_PROG_CC
AC_PROG_CXX

# Checks for programs.
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([dl], [dlsym])

PKG_CHECK_MODULES(LUA51, lua5.1 >= 5.1.0)

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([ \
fcntl.h \
fts.h \
ftw.h \
shadow.h \
stdlib.h \
string.h \
unistd.h \
utime.h \
sys/xattr.h \
])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_CHOWN
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_FUNC_UTIME_NULL
AC_CHECK_FUNCS([ \
__lxstat \
__lxstat64 \
__open \
__open64 \
__opendir2 \
__xmknod \
__xstat \
__xstat64 \
_xftw \
_xftw64 \
access \
accessat \
acct \
canonicalize_file_name \
chdir \
chmod \
chown \
creat \
creat64 \
dlmopen \
dlopen \
euidaccess \
execl \
execle \
execlp \
execv \
execve \
execvp \
faccessat \
fchmodat \
fchownat \
futimesat \
fopen \
fopen64 \
freopen \
freopen64 \
fstatat \
fts_open \
ftw \
ftw64 \
get_current_dir_name \
getcwd \
getwd \
getxattr \
glob \
glob64 \
glob_pattern_p \
lchmod \
lchown \
lckpwdf \
lgetxattr \
link \
linkat \
listxattr \
llistxattr \
lremovexattr \
lsetxattr \
lstat \
lstat64 \
lutimes \
mkdir \
mkdirat \
mkdtemp \
mknod \
mknodat \
mkfifo \
mkfifoat \
mkstemp \
mkstemp64 \
mktemp \
nftw \
nftw64 \
open \
open64 \
openat \
openat64 \
opendir \
pathconf \
readlink \
readlinkat \
realpath \
remove \
removexattr \
rename \
renameat \
revoke \
rmdir \
scandir \
scandir64 \
setenv \
setxattr \
stat \
stat64 \
strchrnul \
symlink \
symlinkat \
tempnam \
tmpnam \
truncate \
truncate64 \
unlink \
unlinkat \
ulckpwdf \
utime \
utimes \
])

AC_EGREP_CPP(EM_386,
[
#include <asm/elf.h>
ELF_ARCH
], elf_arch=EM_386, elf_arch=EM_X86_64
)
AH_TEMPLATE([ELF_ARCH], [ELF architecture])
AC_DEFINE_UNQUOTED(ELF_ARCH, ${elf_arch})

AH_TEMPLATE([READLINK_TYPE], [readlink() return type])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
ssize_t readlink(const char *path, char *buf, size_t bufsize) 
{ return 0; }]])], 
readlink_type="ssize_t", readlink_type="int")
AC_DEFINE_UNQUOTED(READLINK_TYPE, ${readlink_type})
AC_SUBST(PREFIX, ${prefix})
AC_SUBST(CC, ${CC})
AC_SUBST(CXX, ${CXX})
AC_SUBST(LUA51_LIBS, ${LUA51_LIBS})
AC_SUBST(LUA51_CFLAGS, ${LUA51_CFLAGS})

AC_OUTPUT
