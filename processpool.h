#ifndef PROCESSPOOL_H
#define PROCESSPOOL_H

#include<stdio.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/wait.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<assert.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<signal.h>
#include<vector>

#include "log.h"
#include "fdwrapper.h"

using std::vector;

class process
{
public:
	process():m_pid(-1){}

public:
	int m_busy_ratio;
	pid_t m_pid;
	int m_pipefd[2];
};


template<typename C,typename H,typename M>
class processpool
{
private:
	processpool(int listenfd,int process_number = 8);
public:
	static processpool<C,H,M>* create(int listenfd,int process_number = 8)
	{
		if(!m_instance)
		{
			m_instance = new processpool<C,H,M>(listenfd,process_number);
		}
		return m_instance;
	}

	~processpool()
	{
		delete[] m_sub_process;
	}

	void run(const vector<H>& arg);

private:
	void notify_parent_busy_ratio(int pipefd,M* manager);
	int get_most_free_srv();
	void setup_sig_pipe();
	void run_parent();
	void run_child(const vector<H>& arg);

private:
	static const int MAX_PROCESS_NUMBER = 16;
	static const int USER_PER_PROCESS = 65536;
	static const int MAX_EVENT_NUMBER = 10000;

	int m_process_number;
	int m_idx;
	int m_epollfd;
	int m_listenfd;
	int m_stop;
	process* m_sub_process;
	
	static processpool<C,H,M>* m_instance;

};

template<typename C,typename H,typename M>
processpool<C,H,M>* processpool<C,H,M>::m_instance = NULL;

static int EPOLL_WAIT_TIME = 5000;
static sig_pipefd[2];
static void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(sig_pipefd[1],(char*)&msg,1,0);
	errno = save_errno;
}

static void addsig(int sig, void (*handler)(int), bool restart = true)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = handler;
	if(restart)
	{
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert( sigaction(sig,&sa,NULL) != -1);
}


template<typename C,typename H,typename M>
processpool<C,H,M>::processpool(int listenfd,int process_number)
	:m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false)
{
	assert( process_number>0 && process_number<=MAX_PROCESS_NUMBER);

	m_sub_process = new process[process_number];
	assert(m_sub_process);

	for(int i=0;i<process_number;++i)
	{
		int ret = socketpair(AF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
		assert( 0 == ret);




























#endif
