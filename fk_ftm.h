#ifndef _FK_FTM_H_
#define _FK_FTM_H_

/*
 * linux feature test macros
 * no other headers should be included before these
 * feature test macros definitons
 * for sigaction/getline
 */
#if defined(__linux__)

/* how to distinguish the version of glibc??? */

/* for getline() before glibc 2.1 */
//#define _GNU_SOURCE

/* for getline() since glibc 2.1 */
/*
 * better using "#define _XOPEN_SOURCE 700" instead of 
 * using "#define _POSIX_C_SOURCE 200809L"
 * since _POSIX_C_SOURCE is encompassed by _XOPEN_SOURCE
 * in particular, when compiled under RHEL 5.8, of which the version of 
 * glibc is 2.5, if #define _POSIX_C_SOURCE 200809L is used, the flag 
 * SA_RESTART will be undeclared when using sigaction()
 */
//#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

/* for sigaction() */
//#define _XOPEN_SOURCE
//#define _POSIX_C_SOURCE 200809L
//#define _POSIX_SOURCE

#endif

#if defined(__FreeBSD__)
/* for getline() */
#define _WITH_GETLINE
#endif

#endif
