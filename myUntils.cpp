#include "myUntils.h"

bool setPortReuse(int fd)
{
    int reuse = 1;
    return (fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == 0;
}

bool addFdToEpoll(int fd, int epollFd, bool oneShot)
{
    epoll_event event;
    event.data.fd = fd;
    // EPOLLRDHUP 判断对端是否已关闭socket
    event.events = EPOLLIN | EPOLLRDHUP;
    if (oneShot) event.events |= EPOLLONESHOT;
    return epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) == 0;
}

bool delFdFromEpoll(int fd, int epollFd)
{
    return epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int setNoBlock(int fd) 
{
    int oldFlag = fcntl(fd, F_GETFL);
    int flag = oldFlag | O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);   
    return oldFlag;
}

bool updateFdFromEpoll(int fd, int epollFd, int event)
{
    epoll_event ev;
    ev.events = event | EPOLLONESHOT;
    ev.data.fd = fd;
    return epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) == 0;
}
