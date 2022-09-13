#ifndef MYHTTP_H_
#define MYHTTP_H_

#include "../sys.h"
#include "../lib.h"
#include "../timer/timer.h"

// struct TimeListElem;
// struct TimeList;

//HOST: %s\r\n
class HTTP{
public:
    static const int FILENAME_LEN = 200;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    /*状态机三种状态，分别表示为当前正在分析请求行；分析头部；内容*/
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    /*处理结果：*/
    enum HTTP_CODE
    {
        NO_REQUEST,     //请求不完整，继续读取客户端信息
        GET_REQUEST,    //获得了一个完整客户端请求
        BAD_REQUEST,    //客户端请求语法错误
        NO_RESOURCE,    //服务器无资源
        FORBIDDEN_REQUEST,  //客户对资源没有足够的访问权限
        FILE_REQUEST,   //
        INTERNAL_ERROR, //表示服务器内部错误
        CLOSED_CONNECTION   //表示客户端已经关闭连接
    };
    /*从状态机三种可能，即行的读取状态；读取到一个完整行，行出错和行数据不完整*/
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    //HTTP报文生成和解析函数
    HTTP(){};
    ~HTTP(){};
    HTTP(int sockfd, struct sockaddr_in* a);
    void init(int sockfd, const sockaddr_in &addr, socklen_t _socklen, char *root, int TRIGMode,
                     int close_log, std::string user, std::string passwd, std::string sqlname);
    void process(void);
    void set_mode(int mode){r_mode = mode;};
    void close_conn(bool real_close = true);
    TimeListElem* get_list_elem(){ return &tim; }
    // bool read_once();
    // bool write();
    bool ReadNonBlock(void);
    bool WriteNonBlock(void);
    sockaddr_in *get_address()
    {
        return &cli_addr;
    }

private:
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE process_read(void);
    char *get_line(void) { return recv_buff + m_start_line; };
    HTTP_CODE do_request(void);
    bool process_write(HTTP_CODE ret);
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    void init();

public:
    //
    static int m_epollfd;
    static int m_user_count;
    int r_mode;                     //运行模式，0：读； 1：写
        /*定时器*/
    struct TimeListElem tim;
private:
    //报文服务器和客户端信息
    struct sockaddr_in cli_addr;    //客户端信息
    socklen_t socklen;
    // int ep_fd;
    int sockfd;                     //写或者读的套接字
    // struct sockaddr_in ser_addr; //服务器本身的信息
    int t_wlen;                     //目标写数据长度，使用writev函数
    int r_wlen;                     //实际读写长度
    int t_rlen;                     //目标读取长度,使用recv
    int r_rlen;                     //实际读取长度
    int is_ET;
    int cgi;        //是否启用的POST
    int m_linger;   //保持连接
    int m_checked_idx;
    int m_start_line;
    int m_write_idx;
    char *m_url;
    char *m_host;
    char *m_string; //存储请求头数据
    char m_write_buf[MAX_WRITE_SIZE];
    char recv_buff[MAX_BUFF_SIZE];
    char m_real_file[FILENAME_LEN];
    char* m_file_address; //用于存储打开的文件
    struct stat m_file_stat;
    char *doc_root;
    struct iovec m_iv[2];
    int m_iv_count;
    struct epoll_event ev;


    //HTTP报文
    CHECK_STATE m_check_state;
    METHOD m_method;
    char *m_version;
    int m_content_length;
    int m_close_log;


};


#endif