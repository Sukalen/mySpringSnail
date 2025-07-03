#include<stdio.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<string.h>
#include<unistd.h>
#include<vector>

#include "log.h"
#include "conn.h"
#include "mgr.h"
#include "processpool.h"

using std::vector;

static const char* version = "1.0";

static void usage(const char* prog)
{
	log(LOG_INFO,__FILE__,__LINE__,"usage:%s [-h] [-v] [-x] [-f config_file]",prog);
	printf("-h\thelp\n-v\tshow the version\n-x\tset the log level to DEBUG\n-f\tread the config_file(essential)\n");
}


int main(int argc,char** argv)
{
	char cfg_file[1024];
	memset(cfg_file,'\0',sizeof(cfg_file));
	int option;
	while( (option = getopt(argc,argv,"f:xvh")) != -1)
	{
		switch(option)
		{
			case 'x':
			{
				set_loglevel(LOG_DEBUG);
				break;
			}
			case 'v':
			{
				log(LOG_INFO,__FILE__,__LINE__,"%s %s",argv[0],version);
				return 0;
			}
			case 'h':
			{
				usage(basename(argv[0]));
				return 0;
			}
			case 'f':
			{
				memcpy(cfg_file,optarg,strlen(optarg)+1);
				break;
			}
			case '?':
			{
				log(LOG_ERR,__FILE__,__LINE__,"un-recognized option %c",option);
				usage(basename(argv[0]));
				return 1;
			}
		}
	}

	if('\0' == cfg_file[0])
	{
		log(LOG_ERR,__FILE__,__LINE__,"%s","please specify the config file");
		return 1;
	}

	int cfg_fd = open(cfg_file,O_RDONLY);
	if(!cfg_fd)
	{
		log(LOG_ERR,__FILE__,__LINE__,"read config file met error:%s",strerror(errno));
		return 1;
	}

	struct stat cfg_stat;
	if(fstat(cfg_fd,&cfg_stat)<0)
	{
		log(LOG_ERR,__FILE__,__LINE__,"read config file met error:%s",strerror(errno));
		return 1;
	}

	char* buf = new char[cfg_stat.st_size+1];
	memset(buf,'\0',cfg_stat.st_size+1);

	ssize_t read_sz = read(cfg_fd,buf,cfg_stat.st_size);
	if(read_sz < 0)
	{
		log(LOG_ERR,__FILE__,__LINE__,"read config file met error:%s",strerror(errno));
		return 1;
	}

	vector<host> balance_srv;
	vector<host> logical_srv;
	host tmp_host;
	memset(tmp_host.m_hostname,'\0',1024);
	char* tmp_hostname;
	char* tmp_port;
	char* tmp_conncnt;
	bool opentag = false;
	char* tmp = buf;
	char* tmp2 = NULL;
	char* tmp3 = NULL;
	char* tmp4 = NULL;

	while(tmp2 = strpbrk(tmp,"\n"))
	{
		*tmp2++ = '\0';
		if(tmp3 = strstr(tmp,"Listen"))
		{
			tmp_hostname = tmp3+7;
			tmp4 = strstr(tmp_hostname,":");
			if(!tmp4)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			*tmp4++ = '\0';
			tmp_host.m_port = atoi(tmp4);
			memcpy(tmp_host.m_hostname,tmp_hostname,strlen(tmp_hostname)+1);
			balance_srv.push_back(tmp_host);
			memset(tmp_host.m_hostname,'\0',1024);
		}
		else if(strstr(tmp,"<logical_host>"))
		{
			if(opentag)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			opentag = true;
		}
		else if(strstr(tmp,"</logical_host>"))
		{
			if(!opentag)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			logical_srv.push_back(tmp_host);
			memset(tmp_host.m_hostname,'\0',1024);
			opentag = false;
		}
		else if(tmp3 = strstr(tmp,"<name>"))
		{
			tmp_hostname = tmp3+6;
			tmp4 = strstr(tmp_hostname,"</name>");
			if(!tmp4)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			*tmp4 = '\0';
			memcpy(tmp_host.m_hostname,tmp_hostname,strlen(tmp_hostname)+1);
		}
		else if(tmp3 = strstr(tmp,"<port>"))
		{
			tmp_port = tmp3+6;
			tmp4 = strstr(tmp_port,"</port>");
			if(!tmp4)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			*tmp4 = '\0';
			tmp_host.m_port = atoi(tmp_port);
		}
		else if(tmp3 = strstr(tmp,"<conns>"))
		{
			tmp_conncnt = tmp3+7;
			tmp4 = strstr(tmp_conncnt,"</conns>");
			if(!tmp4)
			{
				log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
				return 1;
			}
			*tmp4 = '\0';
			tmp_host.m_conncnt = atoi(tmp_conncnt);
		}
		else
		{
			
		}

		tmp = tmp2;
	}

	if(0 == balance_srv.size()||0 == logical_srv.size())
	{
		log(LOG_ERR,__FILE__,__LINE__,"%s","parse config file failed");
		return 1;
	}
	delete[] buf;
	const char* ip = balance_srv[0].m_hostname;
	int port = balance_srv[0].m_port;
	int listenfd = socket(AF_INET,SOCK_STREAM,0);
	assert(listenfd >= 0);

	int ret = -1;
	struct sockaddr_in servaddr;
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	ret = inet_pton(AF_INET,ip,&servaddr.sin_addr);
	if(ret <= 0)
	{
		log(LOG_ERR,__FILE__,__LINE__,"Invalid IP address:%s",ip);
		return 1;
	}

	ret = bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
	assert(ret != -1);
	ret = listen(listenfd,5);
	assert(ret != -1);

	processpool<conn,host,mgr>* pool = processpool<conn,host,mgr>::create(listenfd,logical_srv.size());

	if(pool)
	{
		pool->run(logical_srv);
		delete pool;
	}
	close(listenfd);
	return 0;
}



	

		

		
			
			
			



















