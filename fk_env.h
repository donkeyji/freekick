#ifndef _FK_ENV_H_
#define _FK_ENV_H_

/* mac features definitions */
#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#endif

#if defined(__linux__)
#include <features.h>
#include <linux/version.h>
#endif

#if defined(__linux__)
#define FK_HAVE_EPOLL
#endif

#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6))
#define FK_HAVE_KQUEUE
#endif

#endif
