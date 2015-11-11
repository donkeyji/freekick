#ifndef _FK_DEF_H_
#define _FK_DEF_H_

#if defined(__linux__)
#define FK_HAVE_EPOLL
#endif

//#if defined(__FreeBSD__) || (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6))
#if defined(__FreeBSD__) || defined(__APPLE__)
#define FK_HAVE_KQUEUE
#endif

#endif
