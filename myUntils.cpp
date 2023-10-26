#include "myUntils.h"

bool setPortReuse(int fd)
{
    int reuse = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret)
    {
        perror("setsockopt");
        return false;
    }
    return true;
}

void addFdToEpoll(int fd, int epollFd, bool oneShot)
{
    epoll_event event;
    event.data.fd = fd;
    // EPOLLRDHUP 判断对端是否已关闭socket
    event.events = EPOLLIN | EPOLLRDHUP ;
    if (oneShot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event);
    setNoBlock(fd);

}

void delFdFromEpoll(int fd, int epollFd)
{
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr) == 0;
    close(fd);
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
    ev.events = event | EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    ev.data.fd = fd;
    return epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev) == 0;
}
