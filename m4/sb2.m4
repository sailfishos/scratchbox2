dnl -*- autoconf -*-
dnl
dnl Scratchbox2 autoconf macros
dnl
dnl SPDX-FileCopyrightText: Copyright (c) 2026 Jolla Mobile Ltd.
dnl Author: Björn Kettunen
dnl
dnl SPDX-License-Identifier: GPL-3.0-or-later


dnl LIBSB2_CONFIG
dnl -------------
dnl
dnl Configure preloader name for the target system.
dnl
dnl The preloader name is based of the target systems ELF interpreter
dnl i.e. LD name.
AC_DEFUN([LIBSB2_CONFIG],
[
  AC_MSG_CHECKING([preloader name for target system])
  AC_LINK_IFELSE([
    AC_LANG_PROGRAM(
      dnl Any targets why are not mentioned above don't have
      dnl a suffix in their LD name.
      dnl
      dnl To expand the macro for a new platform check it's
      dnl glibc shlib-versions file in the sysdeps directory.
      [[#include <stdio.h>

       #if defined (__x86_64__)
           #define HOST_LD_PLATFORM "-x86-64"

       #elif defined(__arm__) && defined(__ARM_PCS_VFP)
             #define HOST_LD_PLATFORM "-armhf"

       #elif defined(__aarch)
             #if defined(__AARCH64EB__)
                 #define HOST_LD_PLATFORM "-aarch64_be"
             #else
                  #define HOST_LD_PLATFORM "-aarch64"
             #endif

       #elif defined(__powerpc__)
             #if _CALL_ELF == 2
                 #define HOST_LD_PLATFORM "64_2"
             #else
                 #define HOST_LD_PLATFORM "64"
             #endif

       #elif defined(__mips__) && defined(__mips_nan2008)
             #define HOST_LD_PLATFORM "-mipsn8"
       #endif
      ]],
      [[fputs (HOST_LD_PLATFORM, stdout)]])
    ],
    [
      libsb2_suffix=$(./conftest$ac_exeext 2>&1)
    ])
    AC_SUBST([LIBSB2_NAME_SUFFIX], [$libsb2_suffix])

    # For during installation
    AC_SUBST([LIBSB2_SONAME_WEXT], [${LIBSB2_NAME}${LIBSB2_NAME_SUFFIX}])

    # full soname of libsb2 for building and during target initialization
    AC_SUBST([LIBSB2_SONAME], [${LIBSB2_SONAME_WEXT}${DYNAMIC_LIB_SUFFIX}.${LIBSB2_SOVER}])
    AC_DEFINE_UNQUOTED([LIBSB2_SONAME], ["${LIBSB2_SONAME}"], AC_PACKAGE_NAME preloader soname)

    AC_MSG_RESULT([${LIBSB2_SONAME}])
])
