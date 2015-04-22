#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <fk_util.h>

//interval: millisecond ( 1/1000 sec )
void fk_util_cal_expire(struct timeval *tv, int interval)
{
	struct timeval now, itv;

	gettimeofday(&now, NULL);
	FK_UTIL_MILLIS2TV(interval, &itv);

	FK_UTIL_TMVAL_ADD(&now, &itv, tv);
}

int fk_util_positive_check(char *start, char *end)
{
	char *p;

	if (end < start) {
		return -1;
	}
	for (p = start; p <= end; p++) {
		if (*p <= '0' || *p > '9') {
			return -1;
		}
	}
	return 0;
}

int fk_util_nonminus_check(char *start, char *end)
{
	char *p;

	if (end < start) {
		return -1;
	}
	for (p = start; p <= end; p++) {
		if (p == start) {
			if (*p < '0' || *p > '9') {
				return -1;
			}
			if (*p == '0') {
				if (start == end) {
					return 0;
				} else {
					return -1;
				}
			}
		} else {
			if (*p < '0' || *p > '9') {
				return -1;
			}
		}
	}
	return 0;
}
