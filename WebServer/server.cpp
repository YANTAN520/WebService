
#include "server.h"
#include <new>
//抛出信号
static void alarm_handler_fun(int signal_num)
{
    pthread_cond_signal(timer.get_time_out_cond());
    alarm(timer.get_step() / 1000); //单位转换
}

/*传入线程池的，不能直接放在类里,timeout*/
void timer_handler(void)
{
    #ifdef DEBUGER
    printf("定时器处理\t\n");
    #endif
    TimeListElem* cur = timer.get_tail(), *tmp;
    HTTP* link;
    cur = cur->pre;
    uint64_t now_time = GetTickCount();
    while(cur != timer.get_head())
    {
        if(cur->target_time <= now_time)
        {
            tmp = cur;
            cur = cur->pre;
            timer.delete_list(tmp);
            /*断开连接释放资源*/
            link = list_entry(HTTP, tmp, tim);
            continue;
        }
        break;
    }
    return;
}

/*超时线程处理，等待超时信号触发，然后加锁进行不活跃连接清理*/
void* threadpool_time(void* arg) 
{
    //任务队列中有任务，就在工作线程中运行任务，其他时候通过
    //循环阻塞和查询
    threadpool_t* pool = (threadpool_t*)arg;

    while(true)
    {
        pthread_mutex_lock(pool->get_lock());
        //没有任务且没有shutdowm，阻塞
        while(!(pool->get_shutdown()))
        {
            pthread_cond_wait(timer.get_time_out_cond(), pool->get_lock()); /*进入阻塞会释放锁*/
            /*超时处理*/
            timer_handler();
        }

        if(pool->get_shutdown())
        {
            //立刻结束此线程
            pthread_mutex_unlock(pool->get_lock());
            pthread_detach(pthread_self());
            pthread_exit(NULL);
        }
        pthread_mutex_unlock((pool->get_lock()));
    }
    pthread_exit(NULL);
}


static void stop_handler_func(int signal_num)
{
    /*关闭所有资源*/
    /*定时器资源不用释放*/
    
    /*http连接资源也不用释放*/

    /*线程池资源需要全部关闭*/
    
}

/*当读入数据时*/
void* run(void* arg)
{
#ifdef DEBUGER
    printf("进入到run函数\t\n");
#endif
    if(arg != nullptr)
    {
        HTTP* http = (HTTP*)arg;
        TimeListElem* node = http->get_list_elem();
        if(http->r_mode == 0)
        {
            //读取文件
#ifdef DEBUGER
            printf("开始读取数据\t\n");
#endif
            if(http->ReadNonBlock())
            {
#ifdef DEBUGER
            printf("开始解析数据\t\n");
#endif
                http->process(); //开始读取报文和解析报文
#ifdef DEBUGER
            printf("解析数据完成\t\n");
#endif
            }
            else
            {
                #ifdef DEBUGER
                printf("数据读取失败\t\n");
                #endif
            }
        }
        else
        {
#ifdef DEBUGER
            printf("开始写数据\t\n");
#endif
            //写文件
            if(http->WriteNonBlock()){
                #ifdef DEBUGER
                printf("写入完成\t\n");
                #endif
            }
            else
            {
                #ifdef DEBUGER
                printf("写入失败\t\n");
                #endif
            }

        }
        timer.insert_list_normal(node);
    }
    else
    {
        #ifdef DEBUGER
        printf("传递参数错误\t\n");
        #endif
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
            // if(errno == EWOULDBLOCK) continue;
            if(ret != -1) continue;
            perror("Accept");
            return -1;
        }
        if(HTTP::m_user_count >= MAX_FD)
        {
            printf("连接数量过多,连接建立失败\n\t");
            return -1;
        }
        return ret;
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
    try
    {
        user = new HTTP[MAX_FD];
    }
    catch(const std::bad_alloc &e)
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
    return epoll_init(this->sockfd);
}

//处理有连接发送时任务，
int Server::deal_connection(void)
{
    /*调用一个accept生成新的套接字，
    在epoll中声明，并生成HTTP文件放入HTTP文件中*/
    struct sockaddr_in cli;
    int clen = 0;
    int connfd = AcceptNonBlock(sockfd, &cli, (socklen_t*)&clen);
    if(connfd < 0)
    {
        return ACCEPT_ERROR;
    }
    //创建HTTP结构体，并存入
    user[connfd].init(connfd, cli, clen, m_root, 1, 1, "123", "123", "suibian"); 
    timer.insert_list_normal(&(user[connfd].tim)); //放入链表
    return connfd;
}

int Server::deal_read_cli(int readfd)
{
    // user[readfd].set_mode(0);
#ifdef DEBUGER
    printf("添加成功\t\n");
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
        nfds = epoll_wait(ep_fd, events, MAX_EVENT_NUMBER, TIMESLOT);
        if (nfds < 0 && errno != EINTR)
        {
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
                int min_thr_num, int max_thr_num, int queue_max_size,
                int timeout)
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
    /*定时器加入*/
    time_out_step = timeout;
    signal(SIGALRM, alarm_handler_fun);
    signal(SIGTERM, stop_handler_func);
    alarm(time_out_step); /*定时时间多久*/
    timer.set_step(timeout*1000); /*s => ms*/
    return;
}



Server::~Server()
{
    close(sockfd);
    close(ep_fd);
    delete pool;
    delete[] user;
}

