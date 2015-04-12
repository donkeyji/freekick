#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <fk_util.h>

//interval: millisecond ( 1/1000 sec )
void fk_util_cal_expire(struct timeval *tv, int interval)
{
	struct timeval now, itv;

	gettimeofday(&now, NULL);
	FK_UTIL_MS2TV(interval, &itv);

	FK_UTIL_TMVAL_ADD(&now, &itv, tv);
}
