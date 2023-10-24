#pragma once
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <iostream>
#include <unistd.h>
#include <cstdio>
#include <stdio.h>
#include <unordered_map>
#include <string>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <sys/mman.h>
#include "myUntils.h"


class HttpConn
{
public:
    // HTTP请求方法，这里只支持GET
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT};
    
    /*
        主状态机的状态 解析客户端请求时，
            CHECK_STATE_REQUESTLINE:当前正在分析请求行
            CHECK_STATE_HEADER:当前正在分析头部字段
            CHECK_STATE_CONTENT:当前正在解析请求体
    */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    
    /*
        服务器处理HTTP请求的可能结果，报文解析的结果
            INCOMPLETE_REQUEST  :   请求不完整，需要继续读取客户数据
            GOOD_REQUEST        :   表示获得了一个完成的客户请求
            BAD_REQUEST         :   表示客户请求语法错误
            NO_RESOURCE         :   表示服务器没有资源
            FORBIDDEN_REQUEST   :   表示客户对资源没有足够的访问权限
            FILE_REQUEST        :   文件请求,获取文件成功
            INTERNAL_ERROR      :   表示服务器内部错误
            CLOSED_CONNECTION   :   表示客户端已经关闭连接了
    */
    enum HTTP_CODE { INCOMPLETE_REQUEST, GOOD_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    
    /*
        从状态机的三种可能状态，即行的读取状态，分别表示
            GOOD_LINE = 读取到一个完整的行
            BAD_LINE = 行出错 
            INCOMPLETE_LINE =行数据尚且不完整
    */
    enum LINE_STATUS { GOOD_LINE = 0, BAD_LINE, INCOMPLETE_LINE };


    static int m_connCount;
    static int m_epollFd;
    static const int READ_BUFF_SIZE = 2048;
    static const int WRITE_BUFF_SIZE = 1024;

    const std::string m_resourePath = "/home/duanran/workSpace/cppWorkSpace/Web-Server/resoure/";

    HttpConn();
    ~HttpConn();
    void init(sockaddr_in addr, int connFd);
    bool readAll();
    bool writeALL();
    bool close();
    void process();

private:
    sockaddr_in addr;
    int connFd;
    char readBuf[READ_BUFF_SIZE];
    char writeBuf[WRITE_BUFF_SIZE];
    int m_read_idx; // 当前缓存区已经有多少数据了
    int m_parseIdx; // 当前已经读到了那个坐标
    int m_parse_line;  // 要获取的行的数据，在缓冲区的起始下标
    std::unordered_map<std::string, std::string> m_header_info;
    
    CHECK_STATE m_main_state;
    HTTP_CODE m_http_state;

    HTTP_CODE process_read();
    HTTP_CODE process_write();

    LINE_STATUS parseLine(); // 将缓冲区的数据进行处理，处理成一行的，即添加'\0'
    char * getLine();      // 从读缓冲区读取完整的一行数据
    char * getResoure(char *);


    HTTP_CODE parseFirstRow(char * str);  // 处理请求首行
    HTTP_CODE parseHeader(char * str);    // 处理请求头
    HTTP_CODE parseBody(char * str);      // 处理请求体

    HTTP_CODE doRequest(); // 对请求的任务进行处理


    METHOD m_method;
    char m_req_resoures[128];
    char m_version[128];
    char *m_resoure;

    void init();
};