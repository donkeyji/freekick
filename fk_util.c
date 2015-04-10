#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <fk_util.h>

//interval: millisecond ( 1/1000 sec )
void fk_util_cal_expire(struct timeval *tv, int interval)
{
	int sec, usec;
	struct timeval now;

	gettimeofday(&now, NULL);

	tv->tv_usec = now.tv_usec + interval * FK_THOUSAND;

	sec = tv->tv_usec / FK_MILLION;
	usec = tv->tv_usec - FK_MILLION * sec;

	tv->tv_sec = now.tv_sec + sec;
	tv->tv_usec = usec;
}
