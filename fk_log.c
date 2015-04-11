#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include <fk_macro.h>
#include <fk_conf.h>
#include <fk_log.h>
#include <fk_mem.h>

#define FK_LOG_BUFF_SIZE 1024

#define FK_LOG_BEGIN	\
	va_list ap;			\
	char log_buff[FK_LOG_BUFF_SIZE]	\
 
#define FK_LOG_END		\
	va_start(ap, fmt);	\
	vsprintf(log_buff, fmt, ap);\
	va_end(ap)	\
 
#define FK_LOG_WRITE(level)	{					\
	va_list ap;									\
	char log_buff[FK_LOG_BUFF_SIZE];			\
	if (level > logger.log_level) {				\
		return;									\
	}											\
	va_start(ap, fmt);							\
	vsprintf(log_buff, fmt, ap);				\
	va_end(ap);									\
	fk_log_fprint_str(level, log_buff);			\
}

static void fk_log_fprint_str(unsigned int level, const char *data);

fk_log logger;

void fk_log_init()
{
	FILE *fp;

	fp = fopen(FK_STR_RAW(setting.log_path), "a+");
	if (fp == NULL) {
		printf("null file\n");
		exit(1);
	}
	logger.log_level = setting.log_level;
	logger.log_file = fp;
}

void fk_log_error(const char *fmt, ...)
{
	FK_LOG_WRITE(FK_LOG_ERROR);
}

void fk_log_warn(const char *fmt, ...)
{
	FK_LOG_WRITE(FK_LOG_WARN);
}

void fk_log_info(const char *fmt, ...)
{
	FK_LOG_WRITE(FK_LOG_INFO);
}

void fk_log_debug(const char *fmt, ...)
{
	FK_LOG_WRITE(FK_LOG_DEBUG);
}

void fk_log_fprint_str(unsigned int level, const char *data)
{
	time_t now;
	char *level_name;
	struct tm *tm_now;
	char date_str[20];

	switch (level) {
	case FK_LOG_ERROR:
		level_name = "ERROR";
		break;
	case FK_LOG_WARN:
		level_name = "WARN";
		break;
	case FK_LOG_INFO:
		level_name = "INFO";
		break;
	case FK_LOG_DEBUG:
		level_name = "DEBUG";
		break;
	}
	now = time(NULL);
	tm_now = localtime(&now);
	if (tm_now == NULL) {
		return;
	}

	sprintf(
	    date_str, "%d-%d-%d %d:%d:%d",
	    tm_now->tm_year + 1900, tm_now->tm_mon + 1,
	    tm_now->tm_mday, tm_now->tm_hour,
	    tm_now->tm_min, tm_now->tm_sec
	);

	//??????why fprintf does not work???????
	fprintf(logger.log_file, "[%s]<%s>%s", date_str, level_name, data);
	printf("[%s]<%s>%s", date_str, level_name, data);
}
