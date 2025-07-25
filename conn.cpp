#include<exception>
#include<errno.h>
#include<string.h>

#include "conn.h"
#include "fdwrapper.h"
#include "log.h"

conn::conn()
{
	m_srvfd = -1;
	
	m_clt_buf = new char[BUF_SIZE];
	if(!m_clt_buf)
	{
		throw std::exception();
	}

	m_srv_buf = new char[BUF_SIZE];
	if(!m_srv_buf)
	{
		throw std::exception();
	}

	reset();
}

conn::~conn()
{
	delete[] m_clt_buf;
	delete[] m_srv_buf;
}

void conn::init_clt(int sockfd,const sockaddr_in& cliaddr)
{
	m_cltfd = sockfd;
	m_clt_address = cliaddr;
}

void conn::init_srv(int sockfd,const sockaddr_in& servaddr)
{
	m_srvfd = sockfd;
	m_srv_address = servaddr;
}

void conn::reset()
{
	m_clt_read_idx = 0;
	m_clt_write_idx = 0;
	m_srv_read_idx = 0;
	m_srv_write_idx = 0;
	
	m_srv_closed = false;
	m_cltfd = -1;
	memset(m_clt_buf,'\0',BUF_SIZE);
	memset(m_srv_buf,'\0',BUF_SIZE);
}

RET_CODE conn::read_clt()
{
	int bytes_read = 0;
	while(1)
	{
		if(m_clt_read_idx >= BUF_SIZE)
		{
			log(LOG_ERR,__FILE__,__LINE__,"%s","the client read buffer is full, let server write");
			return BUFFER_FULL;
		}

		bytes_read = recv(m_cltfd,m_clt_buf+m_clt_read_idx,BUF_SIZE - m_clt_read_idx,0);
		if(-1 == bytes_read)
		{
			if( EAGAIN == errno || EWOULDBLOCK == errno)
			{
				break;
			}
			return IOERR;
		}
		else if( 0 == bytes_read)
		{
			return CLOSED;
		}

		m_clt_read_idx += bytes_read;
	}
	return m_clt_read_idx - m_clt_write_idx > 0 ? OK : NOTHING;
}

RET_CODE conn::read_srv()
{
	int bytes_read = 0;
	while(1)
	{
		if(m_srv_read_idx >= BUF_SIZE)
		{
			log(LOG_ERR,__FILE__,__LINE__,"%s","the server read buffer is full, let client write");
			return BUFFER_FULL;
		}

		bytes_read = recv(m_srvfd,m_srv_buf+m_srv_read_idx,BUF_SIZE - m_srv_read_idx,0);
		if(-1 == bytes_read)
		{
			if( EAGAIN == errno || EWOULDBLOCK == errno)
			{
				break;
			}
			return IOERR;
		}
		else if( 0 == bytes_read)
		{
			log(LOG_ERR,__FILE__,__LINE__,"%s","the server should not close the persist connection");
			return CLOSED;
		}

		m_srv_read_idx += bytes_read;
	}
	return m_srv_read_idx - m_srv_write_idx > 0 ? OK : NOTHING;
}

RET_CODE conn::write_clt()
{
	int bytes_write = 0;
	while(1)
	{
		if(m_srv_read_idx <= m_srv_write_idx)
		{
			m_srv_read_idx = 0;
			m_srv_write_idx = 0;
			return BUFFER_EMPTY;
		}

		bytes_write = send(m_cltfd,m_srv_buf + m_srv_write_idx,m_srv_read_idx - m_srv_write_idx,0);
		if( -1 == bytes_write)
		{
			if( EAGAIN == errno || EWOULDBLOCK == errno)
			{
				return TRY_AGAIN;
			}
			log(LOG_ERR,__FILE__,__LINE__,"write client socket failed,%s",strerror(errno));
			return IOERR;
		}
		else if( 0 == bytes_write)
		{
			return CLOSED;
		}

		m_srv_write_idx += bytes_write;
	}
}

RET_CODE conn::write_srv()
{
	int bytes_write = 0;
	while(1)
	{
		if(m_clt_read_idx <= m_clt_write_idx)
		{
			m_clt_read_idx = 0;
			m_clt_write_idx = 0;
			return BUFFER_EMPTY;
		}

		bytes_write = send(m_srvfd,m_clt_buf+m_clt_write_idx,m_clt_read_idx-m_clt_write_idx,0);
		if( -1 == bytes_write)
		{
			if( EAGAIN == errno || EWOULDBLOCK == errno)
			{
				return TRY_AGAIN;
			}
			log(LOG_ERR,__FILE__,__LINE__,"write server socket failed,%s",strerror(errno));
			return IOERR;
		}
		else if( 0 == bytes_write)
		{
			return CLOSED;
		}

		m_clt_write_idx += bytes_write;
	}
}
	

