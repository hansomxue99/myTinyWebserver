#include "log.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

/*
** Name: Constructor
** Description: 
** Author: wkxue
** Create time: 2023/11/22 18:39
*/
Log::Log() {
    log_numline = 0ll;
    is_async = false;
}

/*
** Name: Destructor
** Description: 
** Author: wkxue
** Create time: 2023/11/22 18:39
*/
Log::~Log() {
    if (log_fp != NULL) {
        fclose(log_fp);
    }
}

/*
** Name: Log initialization
** Description: 
** Author: wkxue
** Create time: 2023/11/22 18:40
*/
bool Log::init(const char *vfile_name, int vlog_close, int vlog_bufsize, int vlog_maxlines, int vque_maxsize) {
    // 设置为异步
    if (vque_maxsize >= 1) {
        is_async = true;
        log_queue = new BlockQueue<string>(vque_maxsize);
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    log_close = vlog_close;
    log_bufsize = vlog_bufsize;
    buf = new char[log_bufsize];
    memset(buf, '\0', log_bufsize);
    log_maxlines = vlog_maxlines;

    // 获取时间
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(vfile_name, '/');
    char log_fullname[256] = {0};
    if (p == NULL) {
        snprintf(log_fullname, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon+1, my_tm.tm_mday, vfile_name);
    } else {
        strcpy(log_name, p+1);
        strncpy(dir_name, vfile_name, p-vfile_name+1);
        snprintf(log_fullname, 255, "%s%d_%02d_%02d_%s", dir_name, my_tm.tm_year + 1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);
    }

    log_day = my_tm.tm_mday;
    log_fp = fopen(log_fullname, "a");
    if (log_fp == NULL) {
        return false;
    }
    return true;
}

/*
** Name: Log write function
** Description: 
** Author: wkxue
** Create time: 2023/11/22 18:41
*/
void Log::write_log(int level, const char *format, ...) {
    // 获取时间
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 解析level
    char s[16] = {0};
    switch (level) {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    // 写入操作
    mutex.lock();
    ++log_numline;

    // 需要创建新日志文件
    if (log_day != my_tm.tm_mday || log_numline % log_maxlines == 0) {
        fflush(log_fp);
        fclose(log_fp);

        char log_newname[256] = {0};
        char tail[16] = {0};

        snprintf(tail, 15, "%d_%02d_%02d_", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday);
        if (log_day != my_tm.tm_mday) {
            snprintf(log_newname, 255, "%s%s%s", dir_name, tail, log_name);
            log_day = my_tm.tm_mday;
            log_numline = 0;
        } else {
            snprintf(log_newname, 255, "%s%s%s.%lld", dir_name, tail, log_name, log_numline / log_maxlines);
        }
        log_fp = fopen(log_newname, "a");
    }
    mutex.unlock();

    va_list valist;
    va_start(valist, format);

    mutex.lock();
    int n = snprintf(buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = vsnprintf(buf+n, log_bufsize-n-1, format, valist);
    va_end(valist);
    buf[m+n] = '\n';
    buf[m+n+1] = '\0';
    string log_str = buf;
    mutex.unlock();

    if (is_async && !log_queue->full()) {
        log_queue->push(log_str);
    } else {
        mutex.lock();
        fputs(log_str.c_str(), log_fp);
        mutex.unlock();
    }
}

/*
** Name: Log flush function
** Description: 
** Author: wkxue
** Create time: 2023/11/22 18:51
*/
void Log::flush(void) {
    mutex.lock();
    fflush(log_fp);
    mutex.unlock();
}
