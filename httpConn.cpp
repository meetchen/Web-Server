#include "httpConn.h"

int HttpConn::m_connCount = 0;
int HttpConn::m_epollFd = 0;

HttpConn::HttpConn() : m_read_idx(0)
{
}

HttpConn::~HttpConn()
{
}

void HttpConn::init(sockaddr_in& addr, int connFd)
{
    this->addr = addr;
    this->connFd = connFd;
    this->m_write_idx = 0;
    this->m_has_send = 0;
    this->m_need_send = 0;
    // std::cout << connFd << std::endl;
    bool ret = setPortReuse(connFd);
    addFdToEpoll(connFd, this->m_epollFd, true);
    m_connCount++;
    this->init();
}

bool HttpConn::readAll()
{

    if (m_read_idx >= READ_BUFF_SIZE)
    {
        return false;
    }

    while (true)
    {
        int len = recv(this->connFd, this->readBuf + m_read_idx, READ_BUFF_SIZE - m_read_idx, 0);
        if (len == -1)
        {
            /*
                具体来说，在非阻塞套接字中：
                    当 recv 操作试图从空的接收缓冲区中读取数据时，它会返回 EWOULDBLOCK 错误码，表示操作无法立即完成。
                    当 send 操作试图将数据发送到满的发送缓冲区时，同样会返回 EWOULDBLOCK 错误码。
                EINTR 链接中断
            */
            if (errno == EINTR || errno == EWOULDBLOCK)
            {
                break;
            }
            return false;
        }
        else if (len == 0)
        {
            return false;
        }

        m_read_idx += len;

        // printf("recv data:\n%s\n", readBuf);

        return true;
    }

    return true;
}

// 将所需要写的数据写入写缓冲区
bool HttpConn::writeALL()
{
    // std::cout << "write All" << std::endl;

    // for (int i = 0; i < m_write_idx; i++)
    // {
    //     printf("%c", writeBuf[i]);
    // }
    // puts("");

    if (m_need_send == 0)
    {
        updateFdFromEpoll(this->connFd, this->m_epollFd, EPOLLIN);
        return false;
    }
    while (true)
    {
        int ret = writev(this->connFd, m_write_all, 2);

        if (ret <= -1)
        {
            if (errno == EAGAIN)
            {
                updateFdFromEpoll(this->connFd, this->m_epollFd, EPOLLOUT);
                return true;
            }
            return false;
        }

        m_has_send += ret;
        m_need_send -= ret;

        printf("ret = %d, has finish : %d, need to send: %d , resoure len : %d\n", ret, m_has_send, m_need_send, m_stat_resoure.st_size);
        if (m_need_send <= 0)
        {
            updateFdFromEpoll(this->connFd, this->m_epollFd, EPOLLIN);
            return false;
        }

        if (m_has_send >= m_write_all[0].iov_len)
        {
            m_write_all[0].iov_len = 0;
            m_write_all[1].iov_base = m_write_all[1].iov_base + (m_has_send - m_write_idx);
            m_write_all[1].iov_len = m_need_send;
        }
        else
        {
            m_write_all[0].iov_base += m_has_send;
            m_write_all[0].iov_len = m_write_idx - m_has_send;
        }
    }

    return true;
}

bool HttpConn::close_conn()
{
    delFdFromEpoll(this->connFd, this->m_epollFd);
    close(this->connFd);
    return true;
}

void HttpConn::process()
{
    // 从读缓冲区拿到数据
    HTTP_CODE retRead = process_read();
    if (retRead == INCOMPLETE_REQUEST)
    {
        // 读取的数据不完整，继续监控读事件
        updateFdFromEpoll(this->connFd, this->m_epollFd, EPOLLIN);
        return;
    }

    // 处理过程
    // std::cout << "get resoure data" << std::endl;

    // 将数据写入到写缓冲区
    bool retWrite = process_write();
    if (!retWrite)
    {
        this->close_conn();
    }

    // 写事件成功，继续监听准备写入写缓冲区
    updateFdFromEpoll(this->connFd, this->m_epollFd, EPOLLOUT);
    return;
}

HttpConn::HTTP_CODE HttpConn::process_read()
{
    HTTP_CODE ret = INCOMPLETE_REQUEST;
    LINE_STATUS lineState = GOOD_LINE;
    char *text = nullptr;
    while (
        (m_main_state == CHECK_STATE_CONTENT && lineState == GOOD_LINE) ||
        ((lineState = parseLine()) == GOOD_LINE))
    {
        text = getLine();
        // 下次getLine的起始地址
        m_parse_line = m_parseIdx;
        // printf("Parse line : %s \n", text);
        switch (m_main_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parseFirstRow(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }

        case CHECK_STATE_HEADER:
        {
            ret = parseHeader(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GOOD_REQUEST)
            {
                return doRequest();
            }

            break;
        }

        case CHECK_STATE_CONTENT:
        {
            ret = parseBody(text);
            if (ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if (ret == GOOD_REQUEST)
            {
                return doRequest();
            }
            break;
        }

        default:
        {
            INTERNAL_ERROR;
        }
        }
    }
    return INCOMPLETE_REQUEST;
}

bool HttpConn::process_write()
{
    // std::cout << " begin to process_write" << std::endl;
    writeRespHeader();
    m_write_all[0].iov_base = writeBuf;
    m_write_all[0].iov_len = m_write_idx;
    m_write_all[1].iov_base = m_resoure;
    m_write_all[1].iov_len = m_stat_resoure.st_size;

    m_need_send = m_write_idx + m_stat_resoure.st_size;
    return true;
}

HttpConn::LINE_STATUS HttpConn::parseLine()
{
    for (; m_parseIdx < m_read_idx; ++m_parseIdx)
    {
        if (readBuf[m_parseIdx] == '\r')
        {
            if (m_parseIdx + 1 == m_read_idx)
                return BAD_LINE;
            if (readBuf[m_parseIdx + 1] == '\n')
            {
                readBuf[m_parseIdx++] = '\0';
                readBuf[m_parseIdx++] = '\0';
                return GOOD_LINE;
            }
            return BAD_LINE;
        }
        else if (readBuf[m_parseIdx] == '\n')
        {
            if (m_parseIdx && readBuf[m_parseIdx - 1] == 'r')
            {
                readBuf[m_parseIdx] = '\0';
                readBuf[m_parseIdx - 1] = '\0';
                return BAD_LINE;
            }
            return BAD_LINE;
        }
    }
    return INCOMPLETE_LINE;
}

char *HttpConn::getLine()
{
    return readBuf + m_parse_line;
}

HttpConn::HTTP_CODE HttpConn::parseFirstRow(char *str)
{

    // GET / HTTP/1.1
    char *temp = strtok(str, " ");

    if (temp == nullptr)
        return BAD_REQUEST;
    if (strcasecmp(temp, "GET") != 0)
        return BAD_REQUEST;
    m_method = GET;

    temp = strtok(NULL, " ");
    if (temp == nullptr)
        return BAD_REQUEST;
    if (strlen(temp) == 1)
        strcpy(m_req_resoures, "index.html");
    else
        strcpy(m_req_resoures, temp + 1);

    printf("m_req_resoures = %s\n", m_req_resoures);

    temp = strtok(NULL, " ");
    if (temp == nullptr)
        return BAD_REQUEST;
    if (strcasecmp(temp, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    strcpy(m_version, temp);

    m_main_state = CHECK_STATE_HEADER;

    // printf("parse FirstRow: \n\tpath = %s \n\tversion = %s \n", m_req_resoures, m_version);

    return GOOD_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::parseHeader(char *str)
{
    if (strlen(str) <= 1)
    {
        m_main_state = CHECK_STATE_CONTENT;
        return INCOMPLETE_REQUEST;
    }
    std::string temp = str;
    int idx = temp.find(':');

    std::string key = temp.substr(0, idx);
    std::string value = temp.substr(idx + 2);
    std::cout << key << ":" << value << std::endl;
    puts(" ");
    m_header_info.insert({key, value});
}

HttpConn::HTTP_CODE HttpConn::parseBody(char *str)
{
    // std::cout << "parse Body...." << std::endl;
    return GOOD_REQUEST;
}

void HttpConn::init()
{
    m_parseIdx = 0;
    m_parse_line = 0;
}

HttpConn::HTTP_CODE HttpConn::doRequest()
{
    m_resoure = getResoure(m_req_resoures);
    if (m_resoure == nullptr)
        return FILE_REQUEST;
    // printf("data : %s \n", m_resoure);
    return GOOD_REQUEST;
}

char *HttpConn::getResoure(char *str)
{
    std::string dist = m_resourePath + std::string(str);
    if (access(dist.c_str(), F_OK) == 0)
    {
        int ret = stat(dist.c_str(), &m_stat_resoure);
        if (ret == -1)
        {
            perror("stat");
            return nullptr;
        }
        int fd = open(dist.c_str(), O_RDONLY);
        off_t len = lseek(fd, 0, SEEK_END);
        void *addr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
        if (addr == (void *)-1)
        {
            perror("mmap");
            throw std::exception();
        }
        return (char *)addr;
    }
    return nullptr;
}

HttpConn::HTTP_CODE HttpConn::writeRespHeader()
{
    /*
        HTTP/1.1 200 OK
        Connection: keep-alive
        Content-Encoding: gzip
        Content-Security-Policy: frame-ancestors 'self' https://chat.baidu.com http://mirror-chat.baidu.com https://fj-chat.baidu.com https://hba-chat.baidu.com https://hbe-chat.baidu.com https://njjs-chat.baidu.com https://nj-chat.baidu.com https://hna-chat.baidu.com https://hnb-chat.baidu.com http://debug.baidu-int.com;
        Content-Type: text/html; charset=utf-8
        Date: Wed, 25 Oct 2023 01:42:23 GMT
        Server: BWS/1.1
        Traceid: 1698198143057394381816446783597320951195
        X-Ua-Compatible: IE=Edge,chrome=1
        Transfer-Encoding: chunked

    */
    char buf[100];
    sprintf(buf, "%s 200 OK \r\n", m_version);
    writeLine(buf);

    memset(buf, 0, sizeof buf);
    writeLine("Connection: keep-alive \r\n");
    writeLine("Content-Type: text/html; charset=utf-8 \r\n");
    writeLine("\r\n");
    

    return INCOMPLETE_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::writeRespContens()
{
}

HttpConn::LINE_STATUS HttpConn::writeLine(char *text)
{

    m_write_idx += strlen(text);
    strcpy(writeBuf + m_write_line, text);
    printf("write line: \n%s\n", writeBuf + m_write_line);
    m_write_line += strlen(text);
    return GOOD_LINE;
}