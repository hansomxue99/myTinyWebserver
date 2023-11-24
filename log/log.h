#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#include <string>

#include "locker.h"
#include "block_queue.h"

using namespace std;

/*
** Name: Log
** Description: 
** Author: wkxue
** Create time: 2023/11/22 16:15
*/
class Log{
public:
    static Log *get_instance() {
        static Log instance;
        return &instance;
    }

    static void *flush_log_thread(void *args) {
        Log::get_instance()->async_write_log();
    }

    bool init(const char *vfile_name, int vlog_close, int vlog_bufsize, int vlog_maxlines, int vque_maxsize);
    void write_log(int level, const char *format, ...);
    void flush(void);
private:
    Log();
    virtual ~Log();
    void *async_write_log() {
        string single_log;
        while (log_queue->pop(single_log)) {
            mutex.lock();
            fputs(single_log.c_str(), log_fp);
            mutex.unlock();
        }
    }
private:
    char            dir_name[128];      // 路径名
    char            log_name[128];      // log文件名称
    int             log_maxlines;       // 日志最大行数
    int             log_bufsize;        // 日志缓冲区大小
    long long       log_numline;        // 日志记录的行数
    int             log_day;            // 日志当前记录的天
    FILE            *log_fp;            // 日志文件指针
    char            *buf;
    bool            is_async;           // 是否同步标志位
    int             log_close;          // 关闭日志
    MutexLocker     mutex;
    BlockQueue<string> *log_queue;      // 阻塞队列
};

#define LOG_DEBUG(format, ...) if(0 == log_close) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == log_close) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == log_close) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == log_close) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

#endif
