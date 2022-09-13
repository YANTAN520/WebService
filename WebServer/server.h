#ifndef __SERVER_H__
#define __SERVER_H__

#include "../sys.h"
#include "../http/myhttp.h"
#include "../timer/timer.h"


class Server{
public:
    Server(int sockfd, struct sockaddr_in* a, 
                int min_thr_num, int max_thr_num, int queue_max_size,
                int timeout);
    int epoll_init(int listed_fd);
    int start_server(void);
    int server_work(void);
    int deal_connection(void);
    int deal_read_cli(int readfd);
    int deal_write_cli(int writefd);
    ~Server();
private:
    //服务器套接字配置socket配置
    int sockfd;                 //监听套接字
    struct sockaddr_in addr;    //服务器信息
    char* m_root;
    //IO复用实现部分
    int ep_fd;
    struct epoll_event ev;
    struct epoll_event events[MAX_EVENT_NUMBER];
    //线程池
    threadpool_t* pool;
    int time_out_step; /*秒为单位*/
    //HTTP实现
    HTTP* user;
};


int SetNonBlock(int iSock);
int SetFlags(int iSock, int flags);
int ReadNonBlock(int fd, char* request, size_t len);
int WriteNonBlock(int fd, const char* response, size_t len);
void* threadpool_time(void* arg);
// void readTask(HTTP* http);
// void writeTask(HTTP* http);
#endif