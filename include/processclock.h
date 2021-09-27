/* Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
*/

/* Process clocks: Count elapsed process CPU time,
 * print results to log.
 *
 * clock_gettime() requires librt, which isn't linked in
 * by default => this feature is active only if 
 * USE_PROCESSCLOCK has been defined in the top-level
 * Makefile.
*/

#ifndef SB2_PROCESSCLOCK_H__
#define SB2_PROCESSCLOCK_H__

#ifdef USE_PROCESSCLOCK

#include <time.h>
#include "sb2.h"

typedef struct {
	struct timespec	pclk_start_time;
	struct timespec	pclk_stop_time;
	long long pclk_ns;
	const char *pclk_name;
} processclock_t;

#define PROCESSCLOCK(v) processclock_t v;
#define START_PROCESSCLOCK(debuglevel,pclk,name) do { \
		if (SB_LOG_IS_ACTIVE((debuglevel))) { \
			(pclk)->pclk_name = (name); \
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &(pclk)->pclk_start_time); \
		} \
	} while(0)

#define STOP_AND_REPORT_PROCESSCLOCK(debuglevel,pclk,str_param) do { \
		if (SB_LOG_IS_ACTIVE((debuglevel))) { \
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &(pclk)->pclk_stop_time); \
			processclock_finalize((pclk)); \
			SB_LOG((debuglevel),"PCLOCK: %09lldns <%s> %s", (pclk)->pclk_ns, \
				(pclk)->pclk_name, (str_param)); \
		} \
	} while(0)


extern void processclock_finalize(processclock_t *pclk);

#else /* USE_PROCESSCLOCK not active */

typedef char processclock_t;

#define PROCESSCLOCK(v)
#define START_PROCESSCLOCK(debuglevel,pclk,name)
#define STOP_AND_REPORT_PROCESSCLOCK(debuglevel,pclk,str_param)

#endif /* USE_PROCESSCLOCK */

#endif /* SB2_PROCESSCLOCK_H__ */
