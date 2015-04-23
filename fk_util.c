#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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

int fk_util_is_positive_str(char *start, int len)
{
	char *p;

	if (len <= 0) {
		return 0;
	}
	for (p = start; p <= start + len -1; p++) {
		if (p == start) {
			if (*p <= '0' || *p > '9') {
				return 0;
			}
		} else {
			if (*p < '0' || *p > '9') {
				return 0;
			}
		}
	}
	return 1;
}

int fk_util_is_nonminus_str(char *start, int len)
{
	char *p;

	if (len <= 0) {
		return 0;
	}

	for (p = start; p <= start + len - 1; p++) {
		if (p == start) {
			if (*p < '0' || *p > '9') {
				return 0;
			}
			if (*p == '0') {
				if (len == 1) {
					return 1;
				} else {
					return 0;
				}
			}
		} else {
			if (*p < '0' || *p > '9') {
				return 0;
			}
		}
	}
	return 1;
}

int fk_util_min_power(int n)
{
	uint32_t q;

	q = 0;
	while (n > 0) {
		q++;
		n >>= 1;
	}
	return 1 << q;
}
