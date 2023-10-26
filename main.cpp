#include "lockUntils.h"
#include "httpConn.h"
#include "myUntils.h"
#include "threadPool.h"
// C++函数模板和类模板的声明和定义问题
#include "threadPool.cpp"

#include <iostream>
#include <unistd.h>
#include <libgen.h>
#include <string>
#include <error.h>
#include <errno.h>

#define MAX_CONNECT 65535
#define MAX_EPOOL_LISTEN 10000

int main(int argc, char *argv[])
{

    int port = -1;

    // 检查输入参数
    if (argc < 2)
    {
        printf("arg error, please input like this : ./%s port\n", basename(argv[0]));
        return 1;
    }
    else
    {
        port = atoi(argv[1]);    
    }


    // 启动线程池
    ThreadPool<HttpConn> *pool = nullptr;

    try
    {
        pool = new ThreadPool<HttpConn>;
    }
    catch(...)
    {
        std::cout << "Thread Pool init failure..." << std::endl;
        exit(-1);
    }

    // 忽悠信号 算数运行出错
    signal(SIGPIPE, SIG_IGN);
    
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    address.sin_family = AF_INET;

    // 设置端口复用
    setPortReuse(listenFd);
    
    bind(listenFd, (struct sockaddr*)&address, sizeof(address));
    
    listen(listenFd, 10);

    int epollFd = epoll_create(10);
    
    HttpConn::m_epollFd = epollFd;
    
    epoll_event event;
    event.data.fd = listenFd;
    event.events = EPOLLIN | EPOLLRDHUP;
    setNoBlock(listenFd);
    epoll_ctl(epollFd, EPOLL_CTL_ADD, listenFd, &event);


    // 保存所有链接
    HttpConn* connects = new HttpConn[MAX_CONNECT];

    // 用于保存epoll监控的结果
    epoll_event *events = new epoll_event[MAX_EPOOL_LISTEN];
    
    while (true)
    {
        int ret = epoll_wait(epollFd, events, MAX_EPOOL_LISTEN, -1);
        if (ret == -1 && errno != ERANGE) 
        {
            perror("epoll_wait");
            continue;
        }
        for (int i = 0; i < ret; i++)
        {
            auto& cur = events[i];
            auto fd = cur.data.fd;
            if (listenFd == fd)
            {
                if (HttpConn::m_connCount >= MAX_CONNECT)
                {
                    continue;
                }
                sockaddr_in address;
                socklen_t len = sizeof(sockaddr_in);
                int clienFd = accept(listenFd, (struct sockaddr*)&address, &len);
                connects[clienFd].init(address, clienFd);
                HttpConn::m_connCount++;
            }
            else if (cur.events & EPOLLIN)
            {
                printf("epoll in catch, fd = %d \n", fd);
                if (connects[fd].readAll())
                {
                    pool->addWork(connects + fd);
                }
                else
                {
                    connects[fd].close_conn();
                }
            }
            else if (cur.events & EPOLLOUT)
            {
                printf("epoll out catch, fd = %d \n", fd);
                if (!connects[fd].writeALL())
                {
                    connects[fd].close_conn();
                }
            }
            else if (cur.events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR))
            {
                connects[fd].close_conn();
            }
        }
    }

    close(epollFd);
    close(listenFd);
    delete []connects;
    delete []events;
    delete pool;

    return 0;

}