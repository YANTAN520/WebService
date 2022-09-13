#include "lib.h"
#include <iostream>
void* testpritf(void* arg)
{
    printf("测试线程池添加");
    sleep(1);
    return NULL;
}


int main(int argc, const char* argv[])
{
    
    int sockfd;
    struct sockaddr_in addrs;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(0 > sockfd){
        perror("socket");
        return -1;
    }
    addrs.sin_family = AF_INET;
    addrs.sin_port = htons(8002);
    addrs.sin_addr.s_addr = inet_addr(SERV_IP);

    Server MyServer(sockfd, &addrs, 3, 100, 100, 5);

    MyServer.start_server();
    //服务器启动后开始进行查询
    MyServer.server_work();
    // threadpool_t pool(3, 100 ,100);
    // pool.threadpool_create();
    // pool.threadpool_add(testpritf, NULL);
    // // printf("main\n");
    // sleep(3);
    // pool.threadpool_destroy();
    return 1;
}