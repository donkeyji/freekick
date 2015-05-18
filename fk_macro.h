#ifndef _FK_TYPES_H_
#define _FK_TYPES_H_

/* log level */
#define FK_LOG_ERROR 	0
#define FK_LOG_WARN 	1
#define FK_LOG_INFO 	2
#define FK_LOG_DEBUG 	3

/* default setting */
#define FK_DEFAULT_MAX_CONN 		10240
#define FK_DEFAULT_LOG_PATH 		"/tmp/freekick.log"
#define FK_DEFAULT_LOG_LEVEL 		(FK_LOG_DEBUG)
#define FK_DEFAULT_PID_PATH 		"/tmp/freekick.pid"
#define FK_DEFAULT_DB_PATH 			"/tmp/freekick.db"
#define FK_DEFAULT_PORT 			6379
#define FK_DEFAULT_DAEMON 			0
#define FK_DEFAULT_SVR_ADDR	 		"0.0.0.0"
#define FK_DEFAULT_DB_CNT 			8
#define FK_DEFAULT_CONN_TIMEOUT 	300 /* timeout of conn, unit: second */
#define FK_DEFAULT_DIR				"/tmp/"

/* high water */
#define FK_BUF_HIGHWAT		512
#define FK_ARG_HIGHWAT 		(FK_BUF_HIGHWAT - 2)
#define FK_STR_HIGHWAT 		(FK_ARG_HIGHWAT + 1)
#define FK_ARG_CNT_HIGHWAT 	128

#endif
