/*
 * Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
 *
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
*/

/* Rule tree server: Library support routines.
 *
 * the daemon is using the very same logger etc. routines 
 * as what are used by the preload library (libsb2),
 * but is executed without libsb2 library (outside of
 * the session)
 *
 * This file provides required, compatible global variables
 * and routines for direct linking to sb2d.
*/

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "sb2_server.h"
#include "libsb2.h"
#include "exported.h"

int sb2_global_vars_initialized__ = 1;

void sb2_initialize_global_variables(void)
{
	sb2_global_vars_initialized__ = 1;
}

char *sbox_binary_name = "[sb2d]";
char *sbox_exec_name = "[sb2d]";
char *sbox_real_binary_name = "[sb2d]";
char *sbox_orig_binary_name = "[sb2d]";

char *sbox_active_exec_policy_name = "[sb2d]";
char *sbox_mapping_method = "";

int pthread_library_is_available = 0;
pthread_t (*pthread_self_fnptr)(void) = NULL;
int (*pthread_mutex_lock_fnptr)(pthread_mutex_t *mutex) = NULL;
int (*pthread_mutex_unlock_fnptr)(pthread_mutex_t *mutex) = NULL;


int open_nomap_nolog(const char *pathname, int flags, ...)
{
        int mode = 0;
        if(flags&O_CREAT) {
                va_list arg;
                va_start (arg, flags);
                mode = va_arg (arg, int);
                va_end (arg);
        }
	return(open(pathname, flags, mode));
}

int close_nomap_nolog(int fd)
{
	return(close(fd));
}

FILE *fopen_nomap(const char *path, const char *mode)
{
	return(fopen(path, mode));
}

