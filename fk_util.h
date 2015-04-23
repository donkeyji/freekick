#ifndef _FK_UTIL_H_
#define _FK_UTIL_H_

#include <sys/time.h>

void fk_util_cal_expire(struct timeval *tv, int interval);
int fk_util_is_positive_str(char *start, int len);
int fk_util_is_nonminus_str(char *start, int len);
int fk_util_min_power(int n);

#define FK_UTIL_TV2MILLIS(tv) 	((tv)->tv_sec * 1000 + (tv)->tv_usec / 1000)

#define FK_UTIL_MILLIS2TV(ms, tv)	{			\
	(tv)->tv_sec = (ms) / 1000;				\
	(tv)->tv_usec = ((ms) % 1000) * 1000;	\
}

#define fk_util_tv2ts(tv, ts)	TIMEVAL_TO_TIMESPEC((tv), (ts))

#define fk_util_ts2tv(tv, ts)	TIMESPEC_TO_TIMEVAL((tv), (ts))

#define FK_UTIL_TMSPEC_PRINT(ts) {				\
	printf("sec: %lu\t", (ts)->tv_sec);			\
	printf("usec: %lu\n", (ts)->tv_nsec);		\
}

#define FK_UTIL_TMVAL_PRINT(tv) {				\
	printf("sec: %lu\t", (tv)->tv_sec);			\
	printf("usec: %lu\n", (tv)->tv_usec);		\
}

//----------------
// >0: t1>t2
// <0: t1<t2
// 0: t1=t2
//----------------
#define fk_util_tmval_cmp(t1, t2)	\
	(t1)->tv_sec == (t2)->tv_sec ? (t1)->tv_usec - (t2)->tv_usec : (t1)->tv_sec - (t2)->tv_sec

#define FK_UTIL_TMVAL_ADD(a, b, result)						\
	do {                                					\
		(result)->tv_sec = (a)->tv_sec + (b)->tv_sec;      	\
		(result)->tv_usec = (a)->tv_usec + (b)->tv_usec;   	\
		if ((result)->tv_usec >= 1000000) {            		\
			(result)->tv_sec++;                				\
			(result)->tv_usec -= 1000000;          			\
		}                           						\
	} while (0)

#define FK_UTIL_TMVAL_SUB(a, b, result)						\
	do {									      			\
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec;	    \
		(result)->tv_usec = (a)->tv_usec - (b)->tv_usec;    \
		if ((result)->tv_usec < 0) {					    \
			--(result)->tv_sec;						      	\
			(result)->tv_usec += 1000000;				    \
		}									      			\
	} while (0)

#define fk_util_int_len(num, len)	\
	len = 0;						\
	while (num != 0) {				\
		len++;						\
		num /= 10;					\
	}								\

#endif
