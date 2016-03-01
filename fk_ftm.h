#ifndef _FK_FTM_
#define _FK_FTM_

/*
 * linux feature test macros
 * no other headers should be included before these 
 * feature test macros definitons
 * for sigaction/getline
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

#endif
