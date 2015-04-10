#ifndef _FK_TYPES_H_
#define _FK_TYPES_H_

/* exit code*/
#define FK_EXIT_ERROR 1

/*log level*/
#define FK_LOG_ERROR 	0
#define FK_LOG_WARNING 	1
#define FK_LOG_INFO 	2
#define FK_LOG_DEBUG 	3

/*arg max*/
#define FK_ARG_MAX 4

/*unused var*/
#define FK_UNUSE(var) (void)(var)

/*saved fd*/
#define FK_SAVED_FD 16

#define FK_MAX_CONN 10240
#define FK_LOG_PATH "/tmp/freekick.log"
#define FK_PID_PATH "/tmp/freekick.pid"
#define FK_DB_PATH "/tmp/freekick.db"
#define FK_DEFAULT_PORT 8210
#define FK_DAEMON 1
#define FK_NODAEMON 0
#define FK_SVR_ADDR "0.0.0.0"

/*timeout of conn, unit: second*/
#define FK_CLIENT_TIMEOUT 300

#define FK_DB_CNT 16
#endif
