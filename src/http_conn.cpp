#include "../include/http_conn.h"
#include "../include/scheduler.h"
#include <fstream>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <ctype.h>

using namespace netco;
// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// 初始化新接受的连接
// check_state默认为分析请求行状态
void http_conn::init()
{
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    m_state = 0;
    m_write_directory_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_write_directory_buf, '\0', 2048);
    m_real_file = nullptr;
}

/**
 * @brief 从状态机，用于分析出一行内容
 *
 * @return http_conn::LINE_STATUS 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
 */
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/**
 * @brief 读取数据，如果没有数据可读则挂到epoll上并让出CPU
 *
 * @param fd
 * @return true
 * @return false
 */
bool http_conn::read_once(int fd)
{
    // printf("我要执行read_once了\r\n");
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    // printf("开始读取数据\r\n");
    //  bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    bytes_read = ::read(fd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx);
    // perror("error:");
    if (bytes_read > 0) // 成功读到数据
    {
        m_read_idx += bytes_read;
        return true;
    }
    else if (bytes_read == -1 && errno == EINTR)
    {
        // printf("我读取到了-1个字节,且错误码是EINTR\r\n");
        return read_once(fd);
    }
    else if (bytes_read == 0) // 读到0的时候就可以结束本协程了
    {
        // printf("over...\r\n");
        over_http = true;
        return false;
    }

    netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(fd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);

    return read_once(fd);
}

/**
 * @brief 解析http请求行，获得请求方法，目标url及http版本号
 *
 * @param text
 * @return http_conn::HTTP_CODE
 */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // printf("我正在解析请求行\r\n");
    // printf("这一行的内容是：%s\r\n", text);
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        // printf("error1\r\n");
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char *method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
    }
    else
        return BAD_REQUEST;
    // printf("我得到了请求方法：%s\r\n", method);

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    // printf("我运行到了这里\r\n");
    // printf("当前m_url的内容是:%s\r\n", m_url);
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    m_url++; // 去掉path中的'/'
    // 当url为/时，默认当前目录

    if (strlen(m_url) == 0)
    {
        // printf("把m_url设置为当前目录\r\n");
        strcat(m_url, "./");
    }

    // printf("murl读取成功:%s\r\n", m_url);
    m_check_state = CHECK_STATE_HEADER;
    // printf("已经将检查状态设置为CHECK_STATE_HEADER\r\n");
    return NO_REQUEST;
}

/**
 * @brief 解析http请求的一个头部信息
 *
 * @param text
 * @return http_conn::HTTP_CODE
 */
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{

    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        // LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

/**
 * @brief 判断http请求是否被完整读入
 *
 * @param text
 * @return http_conn::HTTP_CODE
 */
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/**
 * @brief 处理读取的数据
 *
 * @param cfd
 * @return http_conn::HTTP_CODE
 */
http_conn::HTTP_CODE http_conn::process_read(int cfd)
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    // printf("我将要读取一行内容\r\n");
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();
        // printf("我已经读取完成了一行内容\r\n");
        m_start_line = m_checked_idx;
        // LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            // printf("分析请求行的返回值是%d\r\n", ret);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            // printf("我正在解析请求头\r\n");
            ret = parse_headers(text);
            // printf("解析请求头的结果是:%d\r\n", ret);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                // printf("我获得了一个完整的数据");
                return do_request(cfd);
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request(cfd);
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request(int cfd)
{
    m_real_file = m_url;
    /*处理文件请求*/
    // 获取文件属性
    // printf("正在获取文件属性，文件名称是:%s\r\n", m_url);
    int ret = stat(m_url, &m_file_stat);

    if (ret < 0)
    {
        // printf("没有该文件\r\n");
        return NO_RESOURCE;
    }
    if (!(m_file_stat.st_mode & S_IROTH))
    {
        // printf("没有访问权限\r\n");
        return FORBIDDEN_REQUEST;
    }

    if (S_ISDIR(m_file_stat.st_mode))
    {
        // printf("该文件是个目录\r\n");
        return DIRECTORY_RESOURCE;
    }

    int fd = open(m_real_file, O_RDONLY);
    if (fd < 0)
    {
        // printf("打开文件失败\r\n");
    }
    // printf("打开文件成功\r\n");
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write(int cfd)
{
    // printf("开始发送数据\r\n");
    int temp = 0;

    if (bytes_to_send == 0)
    {
        // printf("发送数据完成\r\n");
        //  modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(cfd, m_iv, m_iv_count);

        // size_t sendIdx = ::send(_sockfd, buf, count, MSG_NOSIGNAL);
        // if (sendIdx >= count)
        // {
        // 	return count;
        // }
        // netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLOUT);
        // return send((char *)buf + sendIdx, count - sendIdx);

        if (temp < 0)
        {
            netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(cfd, EPOLLOUT);
            continue;
            // return write(cfd);
            //  unmap();
            //  return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            // modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}
/**
 * format是格式
 */
bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);

    // LOG_INFO("request:%s", m_write_buf);

    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    int len = sprintf(m_write_buf + m_write_idx, "%s %d %s\r\n", "HTTP/1.1", status, title);
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    return true;
}

bool http_conn::add_headers(int content_len)
{
    // return add_content_length(content_len) && add_linger() &&
    //        add_blank_line();
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    int len = sprintf(m_write_buf + m_write_idx, "Content-Length:%d\r\n", content_len);
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "\r\n");
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    return true;
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type()
{
    return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
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
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
        break;
    }
    case NO_RESOURCE:
    {
        // printf("将404错误页面写入发送缓冲区\r\n");
        write_error_noFile(404, error_404_title, error_404_form);
        break;
        // return true;
    }
    case DIRECTORY_RESOURCE:
    {
        //printf("开始将目录数据写入\r\n");
        int ret = write_directory(200, ok_200_title, get_file_type(".html"));
        //printf("目录数据写入的结果是:%d\r\n", ret);
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_write_directory_buf;
        m_iv[1].iov_len = m_write_directory_idx;
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_write_directory_idx;
        return true;
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
void http_conn::process(int cfd)
{
    // printf("我要开始执行process_read了\r\n");
    HTTP_CODE read_ret = process_read(cfd);
    if (read_ret == NO_REQUEST) // 请求不完整需要继续读取客户数据
    {
        // netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(cfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);
        // modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    // printf("process_read的结果是:%d\r\n", read_ret);
    bool write_ret = process_write(read_ret);

    // 写缓冲区内的内容已经注入完毕，接下来进入写状态
    m_state = 1;
    // modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

bool http_conn::write_error_noFile(int status, const char *title, const char *text)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    int len = sprintf(m_write_buf + m_write_idx, "%s %d %s\r\n", "HTTP/1.1", status, title);
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Content-Type:%s\r\n", "text/html");
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Content-Length:%d\r\n", -1);
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Connection: close\r\n");
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    // send(cfd, buf, strlen(buf), 0);
    len = sprintf(m_write_buf + m_write_idx, "\r\n");
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;
    // send(cfd, "\r\n", 2, 0);

    len = sprintf(m_write_buf + m_write_idx, "<html><head><title>%d %s</title></head>\n", status, title);
    if (len < 0)
    {
        printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    sprintf(m_write_buf + m_write_idx, "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
    if (len < 0)
    {
        // printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    sprintf(m_write_buf + m_write_idx, "%s\n", text);
    if (len < 0)
    {
        // printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    sprintf(m_write_buf + m_write_idx, "<hr>\n</body>\n</html>\n");
    if (len < 0)
    {
        // printf("写入缓冲区已满\r\n");
        return false;
    }
    m_write_idx += len;

    // send(cfd, buf, strlen(buf), 0);

    return true;
}

bool http_conn::write_dir(const char *dirname)
{
    int i, ret;
    int len = 0;
    // 拼一个html页面<table></table>
    // printf("检索的目录是%s\r\n", dirname);
    len = sprintf(m_write_directory_buf + m_write_directory_idx, "<html><head><title>directory: %s</title></head>", dirname);
    if (len < 0)
    {
        printf("第一步出错拉\r\n");
        return false;
    }
    m_write_directory_idx += len;

    len = sprintf(m_write_directory_buf + m_write_directory_idx, "<body><h1>CurDirectory: %s</h1><table>", dirname);
    if (len < 0)
    {
        printf("第二步出错拉\r\n");
        return false;
    }
    m_write_directory_idx += len;

    char enstr[1024] = {0};
    char path[1024] = {0};

    // 目录项二级指针
    struct dirent **ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort);
    // printf("检索到的目录中的文件有%d个\r\n", num);
    //  遍历
    for (i = 0; i < num; ++i)
    {

        char *name = ptr[i]->d_name;

        // 拼接文件的完整路径
        sprintf(path, "%s/%s", dirname, name);
        // printf("path = %s ===================\n", path);
        struct stat st;
        stat(path, &st);

        // 编码生成 %E5 %A7 之类的东西
        encode_str(enstr, sizeof(enstr), name);

        // 如果是文件
        if (S_ISREG(st.st_mode))
        {
            len = sprintf(m_write_directory_buf + m_write_directory_idx,
                          "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                          enstr, name, (long)st.st_size);
            if (len < 0)
            {
                printf("第三步出错拉\r\n");
                return false;
            }
            m_write_directory_idx += len;
        }
        else if (S_ISDIR(st.st_mode))
        { // 如果是目录
            len = sprintf(m_write_directory_buf + m_write_directory_idx,
                          "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                          enstr, name, (long)st.st_size);
            if (len < 0)
            {
                printf("第四步出错拉\r\n");
                return false;
            }
            m_write_directory_idx += len;
        }
    }

    len = sprintf(m_write_directory_buf + m_write_directory_idx, "</table></body></html>");
    if (len < 0)
    {
        printf("第五步出错拉\r\n");
        return false;
    }
    m_write_directory_idx += len;

    return true;
    // printf("dir message send OK!!!!\n");
}

void http_conn::send_respond_head(int cfd, int no, const char *desp, const char *type, long len)
{
    char buf[1024] = {0};
    // 状态行
    sprintf(buf, "http/1.1 %d %s\r\n", no, desp);
    send(cfd, buf, strlen(buf), 0);
    // 消息报头
    sprintf(buf, "Content-Type:%s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%ld\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    // 空行
    send(cfd, "\r\n", 2, 0);
}

// 发送文件
void http_conn::send_file(int cfd, const char *filename)
{
    // 打开文件
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        // send_error(cfd, 404, "Not Found", "NO such file or direntry");
        exit(1);
    }

    // 循环读文件
    char buf[4096] = {0};
    int len = 0, ret = 0;
    while ((len = read(fd, buf, sizeof(buf))) > 0)
    {
        // 发送读出的数据
        ret = send(cfd, buf, len, 0);
        if (ret == -1)
        {
            if (errno == EAGAIN)
            {
                perror("send error:");
                continue;
            }
            else if (errno == EINTR)
            {
                perror("send error:");
                continue;
            }
            else
            {
                perror("send error:");
                exit(1);
            }
        }
    }
    if (len == -1)
    {
        perror("read file error");
        exit(1);
    }

    close(fd);
}

const char *http_conn::get_file_type(const char *name)
{
    const char *dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

void http_conn::encode_str(char *to, int tosize, const char *from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from)
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char *)0)
        {
            *to = *from;
            ++to;
            ++tolen;
        }
        else
        {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}

void http_conn::decode_str(char *to, char *from)
{
    for (; *from != '\0'; ++to, ++from)
    {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
        {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        }
        else
        {
            *to = *from;
        }
    }
    *to = '\0';
}

int http_conn::hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

bool http_conn::write_directory(int status, const char *title, const char *type)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;

    int len = sprintf(m_write_buf + m_write_idx, "http/1.1 %d %s\r\n", status, title); // 状态行
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Content-Type:%s\r\n", type); // 类型
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "Content-Length:%d\r\n", -1); // 长度
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    len = sprintf(m_write_buf + m_write_idx, "\r\n"); // 换行
    if (len < 0)
    {
        return false;
    }
    m_write_idx += len;

    return write_dir(m_url);
}
