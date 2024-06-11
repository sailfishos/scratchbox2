/*
 *
 * SPDX-FileCopyrightText: 2023, Jolla Ltd.
 * SPDX-License-Identifier: LGPL-2.1
 *
 * syscallgate -- syscall() GATE for the scratchbox2 preload library
 *
 * Copyright (C) 2023 Jolla Ltd.
 */

#include <asm-generic/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "libsb2.h"
#include "exported.h"

#define EXPAND_0(OPERATION)
#define EXPAND_1(OPERATION, X) OPERATION(X)
#define EXPAND_2(OPERATION, X, ...) OPERATION(X)EXPAND_1(OPERATION, __VA_ARGS__)
#define EXPAND_3(OPERATION, X, ...) OPERATION(X)EXPAND_2(OPERATION, __VA_ARGS__)
#define EXPAND_4(OPERATION, X, ...) OPERATION(X)EXPAND_3(OPERATION, __VA_ARGS__)
#define EXPAND_5(OPERATION, X, ...) OPERATION(X)EXPAND_4(OPERATION, __VA_ARGS__)
#define EXPAND_6(OPERATION, X, ...) OPERATION(X)EXPAND_5(OPERATION, __VA_ARGS__)

#define GET_MACRO(_0,_1,_2,_3,_4,_5,_6,NAME,...) NAME
#define FOR_EACH(action,...) \
  GET_MACRO(_0,__VA_ARGS__,EXPAND_6,EXPAND_5,EXPAND_4,EXPAND_3,EXPAND_2,EXPAND_1,EXPAND_0)(action,__VA_ARGS__)

#define DEFINE_ARG(arg) long arg = va_arg(args, long);

#define FORWARD_TO_WRAPPER(sysc, ...) \
		case SYS_ ## sysc: { \
			FOR_EACH(DEFINE_ARG, __VA_ARGS__); \
			ret = sysc(__VA_ARGS__); \
		} break

extern long syscall_gate(int *result_errno_ptr,
	long (*real_syscall_ptr)(long number, ...),
	const char *realfnname, long number, va_list args)
{
	long ret;
	switch (number) {
		FORWARD_TO_WRAPPER(execve, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(openat, arg0, arg1, arg2, arg3);
		// FORWARD_TO_WRAPPER(open_by_handle_at, ...)
		FORWARD_TO_WRAPPER(open, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(renameat, arg0, arg1, arg2, arg3);
		FORWARD_TO_WRAPPER(renameat2, arg0, arg1, arg2, arg3, arg4);
		FORWARD_TO_WRAPPER(connect, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(sendto, arg0, arg1, arg2, arg3, arg4, arg5);
		FORWARD_TO_WRAPPER(sendmsg, arg0, arg1, arg2);
		case SYS_recvfrom: {
			FOR_EACH(DEFINE_ARG, arg0, arg1, arg2, arg3, arg4, arg5);
			// must do a cast...
			ret = recvfrom(arg0, arg1, arg2, arg3, (struct sockaddr*)arg4, arg5);
			break;
		}
		FORWARD_TO_WRAPPER(recvmsg, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(accept, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(accept4, arg0, arg1, arg2, arg3);
		FORWARD_TO_WRAPPER(utimensat, arg0, arg1, arg2, arg3);
		FORWARD_TO_WRAPPER(utime, arg0, arg1);
		FORWARD_TO_WRAPPER(utimes, arg0, arg1);
		FORWARD_TO_WRAPPER(fcntl, arg0, arg1, arg2);
		FORWARD_TO_WRAPPER(futimesat, arg0, arg1, arg2);

#ifdef SYS_wait
		FORWARD_TO_WRAPPER(wait, arg0);
#endif
		FORWARD_TO_WRAPPER(waitpid, arg0, arg1, arg2);

#ifdef SYS_wait3
		case SYS_wait3: {
			FOR_EACH(DEFINE_ARG, arg0, arg1, arg2);
			// different number of arguments (for now we ignore rusage
			// and redirect to waitpid).
			ret = waitpid(-1, arg0, arg1);
			break;
		}
#endif

		case SYS_wait4: {
			FOR_EACH(DEFINE_ARG, arg0, arg1, arg2, arg3);
			// different number of arguments (for now we ignore rusage
			// and redirect to waitpid).
			ret = waitpid(arg0, arg1, arg2);
			break;
		}

		default: {
			FOR_EACH(DEFINE_ARG, arg0, arg1, arg2, arg3, arg4, arg5);
			ret = real_syscall_ptr(number, arg0, arg1, arg2, arg3, arg4, arg5);
			break;
		}
	};
	if (result_errno_ptr) *result_errno_ptr = errno;
	return ret;
}

