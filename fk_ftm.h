#ifndef _FK_FTM_
#define _FK_FTM_

/*
 * linux feature test macros
 * no other headers should be included before these
 * feature test macros definitons
 * for sigaction/getline
 */
#if defined(__linux__)

/* for sigaction */
#define _POSIX_C_SOURCE 200809L

/* for getline */
#define _GNU_SOURCE

#endif

#endif
