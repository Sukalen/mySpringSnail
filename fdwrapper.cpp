#include<unistd.h>
#include<fcntl.h>
#include<sys/epoll.h>

#include "fdwrapper.h"

int setnonblocking(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}


void add_read_fd(int epollfd,int fd)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN|EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}

void add_write_fd(int epollfd,int fd)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLOUT|EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}

void removefd(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
}

void closefd(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}

void modfd(int epollfd,int fd,int ev)
{
	struct epoll_event event;
	event.data.fd = fd;
	event.events = ev|EPOLLET;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}
