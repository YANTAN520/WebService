#include "../lib.h"

/*当读入数据时*/
void* run(void* arg)
{
#ifdef DEBUGER
    printf("进入到run函数");
#endif
    if(arg)
    {
        HTTP* http = (HTTP*)arg;
        if(http->r_mode == 0)
        {
            //读取文件
#ifdef DEBUGER
            printf("开始读取数据");
#endif
            if(http->ReadNonBlock())
            {
#ifdef DEBUGER
            printf("开始解析数据");
#endif
                http->process(); //开始读取报文和解析报文
#ifdef DEBUGER
            printf("解析数据完成");
#endif
            }
        }
        else
        {
#ifdef DEBUGER
            printf("开始写数据");
#endif
            http->WriteNonBlock();
            //写文件
#ifdef DEBUGER
            printf("写入完成");
#endif
        }
        
    }
    return NULL;
}
//监听fd
static int Listen(int fd, int backlog)
{
	char	*ptr;

		/*4can override 2nd argument with environment variable */
	if ( (ptr = getenv("LISTENQ")) != NULL)
		backlog = atoi(ptr);

	if (listen(fd, backlog) < 0)
    {
        perror("listen");
        return -1;
    }
    return 1;
}


 //设置非阻塞模式
int SetNonBlock(int iSock)
{
    int iFlags;
 
    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= O_NONBLOCK;
    iFlags |= O_NDELAY;
    int ret = fcntl(iSock, F_SETFL, iFlags);
    return ret;
}
//设置flags
int SetFlags(int iSock, int flags)
{
    int iFlags;

    iFlags = fcntl(iSock, F_GETFL, 0);
    iFlags |= flags;
    return fcntl(iSock, F_SETFL, flags);
}

/*非阻塞accept*/
int AcceptNonBlock(int fd, struct sockaddr_in* cli, socklen_t* clen)
{
    int ret;
    while(1){
        ret = accept(fd, (struct sockaddr*)cli, clen);
        if(ret < 0)
        {
            if(errno == EWOULDBLOCK) continue;
            perror("Accept");
            return -1;
        }else
        {
            return ret;
        }
    }
}

int Server::epoll_init(int listed_fd)
{
    //初始化epoll相关信息
    this->ep_fd = epoll_create(MAX_FD);
    if(ep_fd < 0)
    {
        perror("epoll create\n");
        return EPOLL_CREATE_ERROR;
    }
    HTTP::m_epollfd = ep_fd;
    ev.data.fd = listed_fd;
    ev.events  = EPOLLIN; /*监听字符设置为LT*/
    if((epoll_ctl(this->ep_fd, EPOLL_CTL_ADD, listed_fd, &ev)) < 0)
    {
        perror("epoll ctl");
        return EPOLL_CTL_ERROR;
    }
    user = new HTTP[MAX_FD];
    // user = (HTTP*)malloc(sizeof(HTTP) * MAX_FD);
    if(!user)
    {
        perror("http user malloc");
        return MALLOC_ERROR;
    }
    return -1;
}

int Server::start_server(void)
{
    if(0 > sockfd )
    {
        perror("请初始化套接字和地址\n");
        return SOCKET_INT_ERROR;
    }

    if((bind(this->sockfd, (struct sockaddr *)&addr, sizeof(addr))) < 0)
    {
        perror("bind: ");
        return SOCKET_INT_ERROR;
    }

    if(Listen(this->sockfd, 1024) < 0)
    {
        perror("Listen: ");
        return LISTEN_ERROR;
    }
    pool->threadpool_create();
// #ifdef DEBUGER
//     printf("pool init success");
// #endif
    return epoll_init(this->sockfd);
}

//处理有连接发送时任务，
int Server::deal_connection(void)
{
    /*调用一个accept生成新的套接字，
    在epoll中声明，并生成HTTP文件放入HTTP文件中*/
    struct sockaddr_in cli;
    socklen_t clen;
    int connfd = AcceptNonBlock(sockfd, &cli, &clen);
    if(connfd < 0)
    {
        return ACCEPT_ERROR;
    }
    //创建HTTP结构体，并存入
    user[connfd].init(connfd, cli, m_root, 1, 1, "123", "123", "suibian"); 
    return connfd;
}

int Server::deal_read_cli(int readfd)
{
    // user[readfd].set_mode(0);
#ifdef DEBUGER
    printf("添加成功");
#endif
    // sleep(1);
    user[readfd].set_mode(0);
    pool->threadpool_add(run, (void*)(user + readfd));
    //将任务放入任务池
    return 1;
}
/*发送数据*/
int Server::deal_write_cli(int writefd)
{
    /*将处理函数放入线程池，让线程池调用处理任务*/
    user[writefd].set_mode(1);
    pool->threadpool_add(run, (void*)(user + writefd));
    return 1;
}
int Server::server_work(void){

    int nfds, ret;
    for(;;){
// #ifdef DEBUGER
//         printf("循环中\n");
// #endif
        nfds = epoll_wait(ep_fd, events, MAX_EVENT_NUMBER, TIMESLOT);
        if (nfds < 0 && errno != EINTR)
        {
            // LOG_ERROR("%s", "epoll failure");
            break;
        }
        for(int i = 0; i < nfds; i++){
            if(events[i].data.fd == sockfd)
            {
                //此时表示有连接建立
#ifdef DEBUGER
                printf("监听到连接\n");
#endif
                if((ret = deal_connection()) < 0)
                {

                    continue;
                } 
#ifdef DEBUGER
                    printf("套接字为%d\n", ret);
#endif
                /*处理客户端上数据*/
            }
            else if(events[i].events & EPOLLIN)
            {
                //此时连接有数据输入
#ifdef DEBUGER
                printf("监听到数据输入\n");
#endif
                deal_read_cli(events[i].data.fd);
            }
            else if(events[i].events & EPOLLOUT)
            {
                deal_write_cli(events[i].data.fd);
            }
        }
    }
    return 0;
}

Server::Server(int sockfd, struct sockaddr_in* a, 
                int min_thr_num, int max_thr_num, int queue_max_size)
{
    //初始化监听描述符和服务器套接字
    if(a == NULL)
    {
        perror("HTTP初始化错误");
        return;
    }
    this->sockfd = sockfd;
    memset(&(this->addr), 0, sizeof(struct sockaddr_in));
    memcpy(&(this->addr), a, sizeof(struct sockaddr_in));
    SetNonBlock(this->sockfd);
    //初始化线程池
    pool = new threadpool_t(min_thr_num, max_thr_num, queue_max_size);
    //资源保存路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
    return;

}



Server::~Server()
{
    close(sockfd);
    close(ep_fd);
    delete pool;
    delete user;
}


