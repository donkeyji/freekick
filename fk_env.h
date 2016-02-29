#ifndef _FK_ENV_H_
#define _FK_ENV_H_

/*
 * linux feature test macros
 * no other headers should be included before these 
 * feature test macros definitons
 */
#if defined(__linux__)

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#endif

/* mac features definitions */
#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

#if defined(__linux__)
#define FK_HAVE_EPOLL
#endif

#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6))
#define FK_HAVE_KQUEUE
#endif

#endif
