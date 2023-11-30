#include "cgi_mysql.h"

/*
** Name: 构造和析构函数
** Description: 
** Author: wkxue
** Create time: 2023/11/30 11:49
*/
CGImysql::CGImysql() {
    vcurconn = 0;
    vfreeconn = 0;
}

CGImysql::~CGImysql() {
    destroyPool();
}

/*
** Name: 数据库线程池获取实例
** Description: 单例懒汉模式，static保证线程安全
** Author: wkxue
** Create time: 2023/11/30 11:48
*/
CGImysql* CGImysql::getInstance() {
    static CGImysql connPool;
    return &connPool;
}

/*
** Name: 初始化
** Description: 
** Author: wkxue
** Create time: 2023/11/30 11:50
*/
void CGImysql::init(string Url, string User, string Password, string Databasename, int Port, int Maxconn, int close_log) {
    vurl = Url;
    vport = Port;
    vuser = User;
    vpasswd = Password;
    vdatabasename = Databasename;
    log_close = close_log;

    for (int i=0; i<Maxconn; ++i) {
        MYSQL *con = NULL;
        con = mysql_init(con);

        if (con == NULL) {
            LOG_ERROR("MySQL Init Error");
            exit(1);
        }
        con = mysql_real_connect(con, Url.c_str(), User.c_str(), Password.c_str(), Databasename.c_str(), Port, NULL, 0);
        if (con == NULL) {
            LOG_ERROR("MySQL Connection Error");
            exit(1);
        }
        conn_list.push_back(con);
        ++vfreeconn;
    }
    reserve = Sem(vfreeconn);
    vmaxconn = vfreeconn;
}

/*
** Name: 当出现请求时，获取数据库连接
** Description: 
** Author: wkxue
** Create time: 2023/11/30 12:04
*/
MYSQL* CGImysql::getConnection() {
    MYSQL *con = NULL;
    if (0 == conn_list.size())  return NULL;

    reserve.wait();
    lock.lock();
    con = conn_list.front();
    conn_list.pop_front();
    --vfreeconn;
    ++vcurconn;
    lock.unlock();

    return con;
}

/*
** Name: 释放当前使用的MYSQL连接
** Description: 
** Author: wkxue
** Create time: 2023/11/30 12:07
*/
bool CGImysql::releaseConnection(MYSQL *con) {
    if (con == NULL)    return false;

    lock.lock();
    conn_list.push_back(con);
    ++vfreeconn;
    --vcurconn;
    lock.unlock();
    reserve.post();

    return true;
}

/*
** Name: 获取当前空闲的连接数量
** Description: 
** Author: wkxue
** Create time: 2023/11/30 12:10
*/
int CGImysql::getFreeConn() {
    return this->vfreeconn;
}

/*
** Name: 销毁数据库连接池
** Description: 
** Author: wkxue
** Create time: 2023/11/30 12:10
*/
void CGImysql::destroyPool() {
    lock.lock();
    if (conn_list.size() > 0) {
        list<MYSQL *>::iterator it;
        for (it = conn_list.begin(); it != conn_list.end(); ++it) {
            MYSQL *con = *it;
            mysql_close(con);
        }
        vcurconn = 0;
        vfreeconn = 0;
        conn_list.clear();
    }
    lock.unlock();
}

CGImysqlRAII::CGImysqlRAII(MYSQL **con, CGImysql *connPool) {
    *con = connPool->getConnection();
    conRAII = *con;
    poolRAII = connPool;
}

CGImysqlRAII::~CGImysqlRAII() {
    poolRAII->releaseConnection(conRAII);
}
