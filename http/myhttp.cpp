#include "../lib.h"


//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

int HTTP::m_user_count = 0;
int HTTP::m_epollfd = -1;

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
//关闭连接，关闭一个连接，客户总量减 
void HTTP::close_conn(bool real_close)
{
    if (real_close && (sockfd != -1))
    {
        printf("close %d\n", sockfd);
        removefd(m_epollfd, sockfd);
        sockfd = -1;
        m_user_count--;
    }
}
HTTP::HTTP(int sockfd, struct sockaddr_in* a)
{

}
void HTTP::init(int sockfd, const sockaddr_in &addr, socklen_t _socklen, char *root, int TRIGMode,
                     int close_log, std::string user, std::string passwd, std::string sqlname)
{
    this->sockfd = sockfd;
    cli_addr = addr;

    addfd(m_epollfd, sockfd, true, is_ET);
    m_user_count++;

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    is_ET = TRIGMode;
    m_close_log = close_log;

    socklen = _socklen;

    // strcpy(sql_user, user.c_str());
    // strcpy(sql_passwd, passwd.c_str());
    // strcpy(sql_name, sqlname.c_str());

    init();
}
void HTTP::init()
{
    t_rlen = MAX_BUFF_SIZE;
    t_wlen = 0;
    r_wlen = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    r_rlen = 0;
    m_write_idx = 0;
    cgi = 0;
    r_mode = 0; //读写
    
    memset(recv_buff, '\0', MAX_BUFF_SIZE);
    memset(m_write_buf, '\0', MAX_WRITE_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

//非阻塞模式下的套接字read
bool HTTP::ReadNonBlock()
{
    if(r_rlen > MAX_BUFF_SIZE)
    {
        return false;
    }
    int bytes_read = 0;
    while (true)
        {
            bytes_read = recv(sockfd, recv_buff + r_rlen, t_rlen - r_rlen, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;
                perror("recv -1:");
                return false;
            }
            else if (bytes_read == 0)
            {
                /*连接已中止*/
                perror("recv 0:");
                return false;
            }
            r_rlen += bytes_read;
        }
        return true;
}

//非阻塞模式下的写
bool HTTP::WriteNonBlock()
{
    int count = 0;
    if(t_wlen == 0)
    {
        modfd(m_epollfd, sockfd, EPOLLIN, is_ET);
        init(); //一次HTTP时间处理完成，初始化
        return true;
    }

    while(1)
    {
        count = writev(sockfd, m_iv, m_iv_count);

        if(count < 0)
        {
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, sockfd, EPOLLOUT, is_ET);
                return true;
            }
            unmap(); /*关闭文件*/
            perror("error: ");
            return false;
        }

        r_wlen += count;
        t_wlen -= count;

        if(r_wlen >= m_iv[0].iov_len)
        {
            //此时第一个发送完全，重新发送后面的
            m_iv[0].iov_len = 0;
            m_iv[0].iov_base = m_file_address + (r_wlen - m_write_idx);
            m_iv[0].iov_len = t_wlen;
        }
        else
        {
            m_iv[0].iov_len = m_iv[0].iov_len - r_wlen;
            m_iv[0].iov_base = m_write_buf + r_wlen;
        }

        if(t_wlen <= 0)
        {
            //发送完全
            unmap();
            modfd(m_epollfd, sockfd, EPOLLIN, is_ET);

            if(m_linger)
            {
                init();
                return true;
            }
            else
            {
                perror("发送完全");
                return false;
            }
        }
    }
    
}


//切割收到的数据，将其分为一下三部分，并parse
//从状态机，用于分析一行内容
//返回值为行的读取状态，有LINE_OK, LINE_BAD, LINE_OPEN
HTTP::LINE_STATUS HTTP::parse_line()
{
    char temp;
    for(; m_checked_idx < r_rlen; ++m_checked_idx)
    {
        temp = recv_buff[m_checked_idx];
        if(temp == '\r')
        {
            if((m_checked_idx + 1) == r_rlen)
                return LINE_OPEN;
            else if(recv_buff[m_checked_idx + 1] == '\n')
            {
                recv_buff[m_checked_idx++] = '\0';
                recv_buff[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if(m_checked_idx > 1 && recv_buff[m_checked_idx - 1] == '\r')
            {
                recv_buff[m_checked_idx++] = '\0';
                recv_buff[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
//HTTP报文解析，主要分三部分解析
HTTP::HTTP_CODE HTTP::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if(strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }else{
        //其他请求不管了
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url,  '/');
    }

    if(!m_url || m_url[0] != '/')
    {
        return BAD_REQUEST;
    }

    if(strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;

}
HTTP::HTTP_CODE HTTP::parse_headers(char *text)
{
    if(text[0] == '\0')
    {
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if(strncasecmp(text, "keep-alive", 10) == 0)
        {
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        //打印未知处理头
    }
    return NO_REQUEST;
}
HTTP::HTTP_CODE HTTP::parse_content(char *text)
{   
    if(r_rlen >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
HTTP::HTTP_CODE HTTP::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    const char* p = strrchr(m_url, '/');

    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3'))
    {
        //根据标志判断是登录还是注册检查
        char flag = m_url[1];
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //提取用户名和密码
        //user = 123, passwd = 123;
        char name[100], passwd[100];
        int i;
        for(i = 5; m_string[i] != '&'; ++i)
        {
            name[i-5] = m_string[i];
        }
        name[i-5] = '\0';
        int j = 0;
        for(i = i+10; m_string[i] != '\0'; ++i, ++j)
            passwd[j] = m_string[i];
        passwd[j] = '\0';

        if(*(p+1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //如果没有重名的，进行增加数据
            //先直接省略
        }
        else if(*(p + 1) == '2')
        {
            /*如果是登录，直接判断，查询成功返回1，否则返回失败*/
        }
    }
    if(*(p + 1) == '0')
    {
        char *m_url_real = (char*)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if(*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
     else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

    if(stat(m_real_file, &m_file_stat) < 0)
    {
        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
void HTTP::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
HTTP::HTTP_CODE HTTP::process_read()
{
    HTTP::LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        m_start_line = m_checked_idx;
        switch(m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                break;
            }
            case CHECK_STATE_HEADER:
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if(ret == GET_REQUEST)
                {
                    //将需要传输的内容准备好，然后设置后，并不直接发送
                    return do_request(); 
                }
                break;
            }       
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content(text);
                if(ret == GET_REQUEST)
                    return do_request();
                line_status = LINE_OPEN;
                break;
            } 
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}
bool HTTP::add_response(const char *format, ...)
{
    if (m_write_idx >= MAX_WRITE_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, MAX_WRITE_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (MAX_WRITE_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    // LOG_INFO("request:%s", m_write_buf);

    return true;
}

bool HTTP::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool HTTP::add_headers(int content_len)
{
    return add_content_length(content_len) && add_linger() &&
           add_blank_line();
}
bool HTTP::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool HTTP::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool HTTP::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool HTTP::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool HTTP::add_content(const char *content)
{
    return add_response("%s", content);
}

bool HTTP::process_write(HTTP_CODE ret)
{
    switch(ret)
    {
        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if (!add_content(error_500_form))
                return false;
            break;
        }
        case BAD_REQUEST:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if (!add_content(error_404_form))
                return false;
            break;
        }
        case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if (!add_content(error_403_form))
                return false;
            break;
        }
        case FILE_REQUEST:
        {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                t_wlen = m_write_idx + m_file_stat.st_size;
                return true;
            }
            else
            {
                const char *ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if (!add_content(ok_string))
                    return false;
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    t_wlen = m_write_idx;
    return true;
}
void HTTP::process(void)
{
    /*程序运行时将此函数传入线程池运行*/
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, sockfd, EPOLLIN, is_ET);
        return ;
    }
    /*此时只是将发送数据准备好，实际发送在EPOLLOUT事件出发后实现*/
    bool write_ret = process_write(read_ret); 
    if(!write_ret) //成功失败
    {
        close_conn();
        return;
    }
    modfd(m_epollfd, sockfd, EPOLLOUT, is_ET);
}









