#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "locker.h"

namespace netco
{
    class http_conn
    {
    public:
        static const int FILENAME_LEN = 200;
        static const int READ_BUFFER_SIZE = 2048;
        static const int WRITE_BUFFER_SIZE = 1024; // 写缓冲区的大小
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
        enum CHECK_STATE
        {
            CHECK_STATE_REQUESTLINE = 0, // 当前正在解析请求行
            CHECK_STATE_HEADER,          // 当前正在解析请求头
            CHECK_STATE_CONTENT          // 当前正在解析请求体
        };
        enum HTTP_CODE
        {
            NO_REQUEST,        // 请求不完整，需要继续读取客户数据
            GET_REQUEST,       // 表示获得了一个完整的客户请求
            BAD_REQUEST,       // 表示客户语法错误
            NO_RESOURCE,       // 表示服务器没有资源
            FORBIDDEN_REQUEST, // 表示客户对资源没有足够的访问权限
            FILE_REQUEST,      // 文件请求，获取文件成功
            INTERNAL_ERROR,    // 表示服务器内部错误
            CLOSED_CONNECTION, // 表示客户端已经关闭连接了
            DIRECTORY_RESOURCE // 目录
        };
        enum LINE_STATUS
        {
            LINE_OK = 0, // 读取到一个完整的行
            LINE_BAD,    // 行出错
            LINE_OPEN    // 行数据尚且不完整
        };

    public:
        http_conn() {}
        ~http_conn() {}

    public:
        void init();
        void close_conn(bool real_close = true);
        void process(int cfd);
        bool read_once(int fd);
        bool write(int cfd);
        sockaddr_in *get_address()
        {
            return &m_address;
        }
        int timer_flag;
        int improv;
        bool over_http = 0;

    private:
        HTTP_CODE process_read(int cfd);
        bool process_write(HTTP_CODE ret);
        HTTP_CODE parse_request_line(char *text);
        HTTP_CODE parse_headers(char *text);
        HTTP_CODE parse_content(char *text);
        HTTP_CODE do_request(int cfd);
        char *get_line() { return m_read_buf + m_start_line; };
        LINE_STATUS parse_line();
        void unmap();
        bool add_response(const char *format, ...);
        // bool add_response(const char *format, int status, const char *title);
        bool add_content(const char *content);
        bool add_status_line(int status, const char *title);
        bool add_headers(int content_length);
        bool add_content_type();
        bool add_content_length(int content_length);
        bool add_linger();
        bool add_blank_line();
        bool write_error_noFile(int status, const char *title, const char *text);
        bool write_directory(int status, const char *title, const char *text);
        bool write_dir(const char *dirname);
        void send_respond_head(int cfd, int no, const char *desp, const char *type, long len);
        void send_file(int cfd, const char *filename);
        const char *get_file_type(const char *name);
        void encode_str(char *to, int tosize, const char *from);
        void decode_str(char *to, char *from);
        int hexit(char c);

    public:
        static int m_epollfd;
        static int m_user_count;
        int m_state; // 读为0, 写为1

    private:
        int m_sockfd;
        sockaddr_in m_address;
        char m_read_buf[READ_BUFFER_SIZE];
        long m_read_idx;
        long m_checked_idx;
        int m_start_line;
        char m_write_buf[WRITE_BUFFER_SIZE]; // 只保存响应头中的内容
        char m_write_directory_buf[2048];    // 保存目录内容
        int m_write_directory_idx = 0;
        int m_write_idx;           // 当前应该写入发送缓冲区中的位置
        CHECK_STATE m_check_state; // 当前正在解析的状态
        METHOD m_method;
        // char m_real_file[FILENAME_LEN];
        char *m_real_file;
        char *m_url;
        char *m_version;
        char *m_host;
        long m_content_length;
        bool m_linger;
        char *m_file_address;
        struct stat m_file_stat; // 保存请求文件的文件属性
        struct iovec m_iv[2];
        int m_iv_count;
        int cgi;             // 是否启用的POST
        char *m_string;      // 存储请求头数据
        int bytes_to_send;   // 还剩下多少数据没有发送
        int bytes_have_send; // 已经发送了多少数据
        char *doc_root;

        std::map<std::string, std::string> m_users;
        int m_TRIGMode;
        int m_close_log;
    };
}
#endif
