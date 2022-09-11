#ifndef __SYS_H_
#define __SYS_H_
#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include <string>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/mman.h>



enum ERROR{
    SOCKET_INT_ERROR = -1,
    MALLOC_ERROR = -2,
    READ_ERROR = -3,
    EPOLL_CREATE_ERROR = -4,
    EPOLL_CTL_ERROR = -5,
    ACCEPT_ERROR = -6,
    LISTEN_ERROR = -7,
    LOCK_INIT_ERROR = -8
};


#define MAX_FD  30000           //最大文件描述符
#define MAX_EVENT_NUMBER  10000 //最大事件数
#define TIMESLOT  5             //最小超时单位
#define MAXSIZE 1024
#define SERV_PORT 8000
#define SERV_IP "127.0.0.1"
#define MAX_BUFF_SIZE 2048
#define MAX_WRITE_SIZE 1024

#define DEBUGER


#endif