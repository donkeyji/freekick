#ifndef _FK_UTIL_H_
#define _FK_UTIL_H_

#include <sys/time.h>

#define FK_MILLION 	1000000
#define FK_THOUSAND 	1000

void fk_util_cal_expire(struct timeval *tv, int interval);

#define FK_UTIL_TV2MS(tv) 						\
	((tv)->tv_sec * 1000 + (tv)->tv_usec / 1000)

#define FK_UTIL_TV2TS(tv, ts)	TIMEVAL_TO_TIMESPEC((tv), (ts))

#define FK_UTIL_TS2TV(tv, ts)	TIMESPEC_TO_TIMEVAL((tv), (ts))

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
#define FK_UTIL_TMVAL_CMP(t1, t2)	\
	(t1)->tv_sec == (t2)->tv_sec ? (t1)->tv_usec - (t2)->tv_usec : (t1)->tv_sec - (t2)->tv_sec

	////((t1)->tv_sec * FK_MILLION + (t1)->tv_usec) - ((t2)->tv_sec * FK_MILLION + (t2)->tv_usec)

#define FK_UTIL_TMVAL_SUB(a, b, result)				\
  do {									      			\
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;	    \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;    \
    if ((result)->tv_usec < 0) {					    \
      --(result)->tv_sec;						      	\
      (result)->tv_usec += 1000000;					    \
    }									      			\
  } while (0)

#define FK_UTIL_INT_LEN(num, len)	\
	len = 0;						\
	while (num != 0) {				\
		len++;						\
		num /= 10;					\
	}								\

#endif
