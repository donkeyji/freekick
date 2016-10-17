#ifndef _FK_UTIL_H_
#define _FK_UTIL_H_

/* c standard headers */
#include <stdint.h>
#include <stdlib.h>

/* unix headers */
#include <sys/time.h>

/*
 * no unified status code for this module
 */

int fk_util_get_time(struct timeval *tv);
void fk_util_cal_expire(struct timeval *tv, uint32_t interval);
int fk_util_is_positive_seq(const char *start, size_t len);
int fk_util_is_nonminus_seq(const char *start, size_t len);
int fk_util_is_digit_seq(const char *start, size_t len);
int fk_util_min_power(int n);
size_t fk_util_decimal_digit(int num);
void fk_util_backtrace(int level);

#define fk_util_tv2millis(tv)   ((tv)->tv_sec * 1000 + (tv)->tv_usec / 1000)

#define fk_util_millis2tv(ms, tv)                               \
    do {                                                        \
        (tv)->tv_sec = (time_t)((ms) / 1000);                   \
        (tv)->tv_usec = (suseconds_t)(((ms) % 1000) * 1000);    \
    } while (0)

#define fk_util_tv2ts(tv, ts)   TIMEVAL_TO_TIMESPEC((tv), (ts))

#define fk_util_ts2tv(tv, ts)   TIMESPEC_TO_TIMEVAL((tv), (ts))

/*
 * >0: t1>t2
 * <0: t1<t2
 * =0: t1=t2
 */
#define fk_util_tmval_cmp(t1, t2)   (   \
    (t1)->tv_sec == (t2)->tv_sec  ?     \
    (t1)->tv_usec - (t2)->tv_usec :     \
    (t1)->tv_sec - (t2)->tv_sec         \
)

#define fk_util_tmval_add(a, b, result)                     \
    do {                                                    \
        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;       \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;    \
        if ((result)->tv_usec >= 1000000) {                 \
            (result)->tv_sec++;                             \
            (result)->tv_usec -= 1000000;                   \
        }                                                   \
    } while (0)

#define fk_util_tmval_sub(a, b, result)                     \
    do {                                                    \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;       \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;    \
        if ((result)->tv_usec < 0) {                        \
            --(result)->tv_sec;                             \
            (result)->tv_usec += 1000000;                   \
        }                                                   \
    } while (0)


#define fk_util_conns_to_files(max_conns)       ((max_conns) + 1 + 16)

#define fk_util_files_to_conns(max_files)       ((max_files) - 1 - 16)

/* unused var */
#define fk_util_unuse(var)                      (void)(var)

#define fk_util_smaller(a, b)                   ((a) <= (b) ? (a) : (b))
#define fk_util_bigger(a, b)                    ((a) >= (b) ? (a) : (b))

/*
 * we implement our own version of assert macro to offer more detailed
 * information than assert() provided by standard c library
 */

#ifdef FK_DEBUG
#define fk_util_assert(exp)    do {                                         \
    if (!(exp)) {                                                           \
        /* passing 1 is to exclude the frame of fk_util_backtrace() */      \
        fk_util_backtrace(1);                                               \
        abort();                                                            \
    }                                                                       \
} while (0)
#else
#define fk_util_assert(exp)
#endif

#endif
