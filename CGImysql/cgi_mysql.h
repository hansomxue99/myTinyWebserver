#ifndef CGI_MYSQL_H
#define CGI_MYSQL_H

#include "../log/locker.h"
#include "../log/log.h"

#include <mysql/mysql.h>
#include <pthread.h>
#include <string>
#include <list>
using namespace std;

class CGImysql {
public:
    static CGImysql *getInstance();
    void init(string Url, string User, string Password, string Databasename, int Port, int Maxconn, int close_log);

    MYSQL   *getConnection();                   // 获取数据库连接
    bool    releaseConnection(MYSQL *con);      // 释放连接
    int     getFreeConn();                      // 获取当前空闲连接数量
    void    destroyPool();                      // 销毁所有连接

private:
    CGImysql();
    ~CGImysql();

public:
    string vurl;            // 主机地址
    string vport;           // 数据库端口号
    string vuser;           // 登陆数据库的用户名
    string vpasswd;         // 登陆数据库的密码
    string vdatabasename;   // 使用数据库名
    int    log_close;       // 日志开关

private:
    list<MYSQL *>   conn_list;  // 数据池
    MutexLocker     lock;
    Sem             reserve;
    int             vmaxconn;   // 最大连接数量
    int             vcurconn;   // 当前已使用的连接数
    int             vfreeconn;  // 当前空闲的连接数
};


class CGImysqlRAII {
public:
    CGImysqlRAII(MYSQL **con, CGImysql *connPool);
    ~CGImysqlRAII();

private:
    MYSQL *conRAII;
    CGImysql *poolRAII;
};

#endif