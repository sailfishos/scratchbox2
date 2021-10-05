#ifndef LIBSB2CALLERS_H__
#define LIBSB2CALLERS_H__

/* libsb2callers: Macros for creating functions for calling
 * routines from libsb2, which may not be available.
 *
 * Copyright (c) 2008 Nokia Corporation. All rights reserved.
 * Author: Lauri T. Aarnio
 * License: LGPL-2.1
*/

extern void *libsb2_handle;

/* create a wrapper to a function returning void */
#define LIBSB2_VOID_CALLER(funct_name, param_list, param_names) \
	static void call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && libsb2_handle) \
			fnptr = dlsym(libsb2_handle, #funct_name); \
		if (fnptr) { \
			((void(*)param_list)fnptr)param_names; \
			return; \
		} \
	} \
	extern void funct_name param_list; /* ensure that we got the prototype right */

/* create a wrapper to a function with returns something */
#define LIBSB2_CALLER(return_type, funct_name, param_list, param_names, errorvalue) \
	static return_type call_ ## funct_name param_list \
	{ \
		static	void *fnptr = NULL; \
		if (!fnptr && libsb2_handle) \
			fnptr = dlsym(libsb2_handle, #funct_name); \
		if (fnptr) { \
			return(((return_type(*)param_list)fnptr)param_names); \
		} \
		return(errorvalue); \
	} \
	extern return_type funct_name param_list; /* ensure that we got the prototype right */

#endif /* LIBSB2CALLERS_H__ */
