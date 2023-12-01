#include "../log/log.h"
#include "../localepoll/localepoll.h"
#include "../http/http.h"
#include "../CGImysql/cgi_mysql.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <assert.h>

#include <string>
#include <map>
#include <iostream>

using namespace std;

int main()
{
    Log::get_instance()->init("/home/ubuntu/myTinyWebserver/log_txt/ServerLog", 0, 2000, 800000, 0);

    //初始化数据库连接池
    CGImysql *m_connPool = CGImysql::getInstance();
    m_connPool->init("localhost", "debian-sys-maint", "L57oYDftfQiuWAGu", "webdb", 3306, 8, 0);


    const char* ip = "0.0.0.0";
    int port = 9006;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd > 0);
    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
	assert(ret != -1);
	ret = listen(listenfd, 5);
	assert(ret != -1);
    map<int, sockaddr_in> client_map;
	
    //epoll创建内核事件表
    LocalEpoll lepoll;
    epoll_event events[1024];
    int m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    lepoll.add_epollfd(m_epollfd, listenfd, false, 0);
    //简单测试，只有一个用户连接，所以只定义了一个http_conn对象
    //没有初始化mysql连接，先不涉及mysql部分功能
    HTTP http_tmp;
    http_tmp.initmysql_result(m_connPool);
    HTTP::vepoll_fd = m_epollfd;
    while(1)
    {
        ret = epoll_wait(m_epollfd, events, 1024, -1);
        if(ret < 0)
        {
            printf("epoll failure\n");
            break;
        }
        for(int i = 0; i < ret; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                printf("有新连接发生\n");
                sockaddr_in client_address;
	            socklen_t client_addrLen = sizeof(client_address);
                int connfd = accept(listenfd, (sockaddr*)&client_address, &client_addrLen);
                lepoll.add_epollfd(m_epollfd, connfd, false, 0);
                
                client_map[connfd] = client_address; 
            }
            else if(events[i].events & EPOLLIN)
            {
                printf("有可读事件发生\n");
                //root文件夹路径
                char server_path[200];
                //获取当前工作路径
                getcwd(server_path, 200);
                char root[6] = "/root";
                char *m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
                strcpy(m_root, server_path);
                strcat(m_root, root);

                string user = "root";
                string passwd = "ubuntu";
                string databasename = "webdb";
                //LT模式
                http_tmp.http_init(sockfd, client_map[sockfd], m_root, 0, 0, user, passwd, databasename);
                
                //读取请求
                if(http_tmp.http_read())
                {
                    printf("读取成功\n");
                }
                else{
                    //已读完
                    close(sockfd);
                    continue;
                }
                //处理请求，生成响应报文
                http_tmp.http_process();
                //发送响应报文
                if(http_tmp.http_write())
                {
                    printf("发送成功\n");
                }
            }
            else{
                printf("something else happened\n");
            }
        }
    }
    http_tmp.~HTTP();
 
    struct linger tmp = {0, 1};
    setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    close(listenfd);
    close(m_epollfd);
    return 0;
}
