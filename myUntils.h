#pragma once
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>


bool setPortReuse(int fd);

void addFdToEpoll(int fd, int epollFd, bool oneShot);

void delFdFromEpoll(int fd, int epollFd);

bool updateFdFromEpoll(int fd, int epollFd, int event);

int setNoBlock(int fd);
