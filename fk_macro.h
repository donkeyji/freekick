#ifndef _FK_TYPES_H_
#define _FK_TYPES_H_

/*log level*/
#define FK_LOG_ERROR 	0
#define FK_LOG_WARN 	1
#define FK_LOG_INFO 	2
#define FK_LOG_DEBUG 	3

/*arg max*/
#define FK_ARG_MAX 4

/*unused var*/
#define FK_UNUSE(var) (void)(var)

/*saved fd*/
#define FK_SAVED_FDS 16

/*default setting*/
#define FK_MAX_CONN 10240
#define FK_LOG_PATH "/tmp/freekick.log"
#define FK_PID_PATH "/tmp/freekick.pid"
#define FK_DB_PATH "/tmp/freekick.db"
#define FK_DEFAULT_PORT 8210
#define FK_DAEMON 1
#define FK_NODAEMON 0
#define FK_SVR_ADDR "0.0.0.0"
#define FK_DB_CNT 8

/*timeout of conn, unit: second*/
#define FK_CONN_TIMEOUT 300

#define FK_MAXCONN_2_MAXFILES(max_conn)	(max_conn) + 1 + FK_SAVED_FDS
#define FK_MAXFILES_2_MAXCONN(max_files)	(max_files) - 1 - FK_SAVED_FDS

#endif
