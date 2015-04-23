#ifndef _FK_UTIL_H_
#define _FK_UTIL_H_

#include <sys/time.h>

void fk_util_cal_expire(struct timeval *tv, int interval);
int fk_util_is_positive_seq(char *start, int len);
int fk_util_is_nonminus_seq(char *start, int len);
int fk_util_is_digit_seq(char *start, int len);
int fk_util_min_power(int n);

#define fk_util_tv2millis(tv) 	((tv)->tv_sec * 1000 + (tv)->tv_usec / 1000)

#define fk_util_millis2tv(ms, tv)	{			\
	(tv)->tv_sec = (ms) / 1000;				\
	(tv)->tv_usec = ((ms) % 1000) * 1000;	\
}

#define fk_util_tv2ts(tv, ts)	TIMEVAL_TO_TIMESPEC((tv), (ts))

#define fk_util_ts2tv(tv, ts)	TIMESPEC_TO_TIMEVAL((tv), (ts))

/*
 * >0: t1>t2 
 * <0: t1<t2 
 * =0: t1=t2
 */
#define fk_util_tmval_cmp(t1, t2)	\
	(t1)->tv_sec == (t2)->tv_sec ? 	\
	(t1)->tv_usec - (t2)->tv_usec : (t1)->tv_sec - (t2)->tv_sec

#define fk_util_tmval_add(a, b, result)						\
	do {                                					\
		(result)->tv_sec = (a)->tv_sec + (b)->tv_sec;      	\
		(result)->tv_usec = (a)->tv_usec + (b)->tv_usec;   	\
		if ((result)->tv_usec >= 1000000) {            		\
			(result)->tv_sec++;                				\
			(result)->tv_usec -= 1000000;          			\
		}                           						\
	} while (0)

#define fk_util_tmval_sub(a, b, result)						\
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
