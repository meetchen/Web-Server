#pragma once
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>


bool setPortReuse(int fd);

bool addFdToEpoll(int fd, int epollFd, bool oneShot);

bool delFdFromEpoll(int fd, int epollFd);

bool updateFdFromEpoll(int fd, int epollFd, int event);

int setNoBlock(int fd);
