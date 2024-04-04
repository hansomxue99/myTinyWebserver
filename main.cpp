#include "./webserver/webserver.h"

int main(void) {
    string user = "debian-sys-maint";
    string passwd = "L57oYDftfQiuWAGu";
    string databasename = "webdb";

    int port = 9006;
    int log_write = LOG_ASYN;
    int trig_mode = LT_LT;

    int linger = 0;
    int sql_num = 8;
    int thread_num = 8;
    int log_close = 0;
    int actor_mode = PROACTOR;

    WebServer server;

    server.init(port, user, passwd, databasename, log_write, 
                linger, trig_mode,  sql_num,  thread_num, 
                log_close, actor_mode);

    server.eventListen();
    server.eventLoop();
    return 0;
}