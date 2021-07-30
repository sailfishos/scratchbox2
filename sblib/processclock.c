/* Copyright (C) 2012 Nokia Corporation.
 * Author: Lauri T. Aarnio
*/

#ifdef USE_PROCESSCLOCK

#include "processclock.h"

void processclock_finalize(processclock_t *pclk)
{
	long long start;
	long long stop;

	start = (pclk->pclk_start_time.tv_sec * 1000000000) +
		(pclk->pclk_start_time.tv_nsec);
	stop =  (pclk->pclk_stop_time.tv_sec * 1000000000) +
		(pclk->pclk_stop_time.tv_nsec);
	pclk->pclk_ns = stop-start;
}

#endif

