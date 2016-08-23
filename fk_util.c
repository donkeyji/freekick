/* feature test macros */
#include <fk_ftm.h>

/* c standard headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* unix headers */
#include <sys/time.h>

/* local headers */
#include <fk_env.h>
#include <fk_util.h>

/*
 * description: a wrapper of gettimeofday() / clock_gettime()
 * purpose: to obtain a relative time for Timer checking, not a
 * calendar time
 * clock_gettime() provides better precise control over the time obtaining, so
 * clock_gettime() precedes gettimeofday() when choosing the interface to get
 * time of the system
 * We must use fk_util_get_time() to replace all the orignal calls to gettimeofday()
 * in those sources of freekick
 */
int
fk_util_get_time(struct timeval *tv)
{
    int rt;

#ifdef FK_HAVE_CLOCK_GETTIME
    struct timespec ts;

    /*
     * clock_gettime() is a POSIX clocks API providing nanosecond precision,
     * by contrast, UNIX traditional APIs do not.
     * this clock_id CLOCK_MONOTONIC is conforming to POSIX
     * on linux, the returned time is since the system reboots
     */
    rt = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rt < 0) {
        return -1;
    }
    fk_util_ts2tv(tv, &ts);
#else
    /*
     * gettimeofday() was marked as obsolete by SYSv4, so clock_gettime() is
     * recommended instead.
     */
    rt = gettimeofday(tv, NULL);
    if (rt < 0) {
        return -1;
    }
#endif

    rt = 0; /* unnecessary, could be ommited */
    return rt;
}

/* interval: millisecond ( 1/1000 sec ) */
void
fk_util_cal_expire(struct timeval *tv, uint32_t interval)
{
    struct timeval  now, itv;

    //gettimeofday(&now, NULL);
    fk_util_get_time(&now);
    fk_util_millis2tv(interval, &itv);

    fk_util_tmval_add(&now, &itv, tv);
}

int
fk_util_is_positive_seq(const char *start, size_t len)
{
    const char  *p;

    if (len <= 0) {
        return 0;
    }
    for (p = start; p <= start + len - 1; p++) {
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

int
fk_util_is_nonminus_seq(const char *start, size_t len)
{
    const char  *p;

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

int
fk_util_is_digit_seq(const char *start, size_t len)
{
    int   i;
    char  c;

    if (len <= 0) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        c = *(start + i);
        if (i == 0) {
            if (c != '-' && c != '+' && (c <= '0' || c > '9')) {
                return 0;
            } else {
                if (c == '-' || c == '+') {
                    if (len <= 1) {
                        return 0;
                    }
                }
            }
        } else {
            if (c < '0' || c > '9') {
                return 0;
            }
        }
    }
    return 1;
}

/* if n < 0, return 1 */
int
fk_util_min_power(int n)
{
    uint32_t  q;

    q = 0;
    while (n > 0) {
        q++;
        n >>= 1;
    }
    return 1 << q;
}

size_t
fk_util_decimal_digit(int num)
{
    size_t  len;

    len = 0;
    if (num < 0) {
        len++;
    }

    do {
        len++;
        num /= 10;
    } while (num != 0);

    return len;
}
