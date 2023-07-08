#include "../include/socket.h"
#include "../include/scheduler.h"
#include "../include/http_conn.h"
#include "../include/coroutine.h"
#include "../include/abstractSlot.h"
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h> // snprintf
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <iostream>

using namespace netco;

Socket::~Socket()
{
	--(*(_pRef));

	// printf("%d还有%d个实例\r\n", _sockfd, *_pRef);
	if (!(*_pRef) && isUseful())
	{
		//::close(_sockfd);
		delete _pRef;
		_pRef = nullptr;
		// std::cout << "The socket of " << _sockfd << "is colse." << std::endl;
		if (m_http_conn != nullptr)
		{
			// printf("%d删除m_http_conn\r\n", _sockfd);
			delete m_http_conn;
			m_http_conn = nullptr;
			// printf("%d删除m_http_conn成功\r\n", _sockfd);
		}
	}
}

bool Socket::getSocketOpt(struct tcp_info *tcpi) const
{
	socklen_t len = sizeof(*tcpi);
	memset(tcpi, 0, sizeof(*tcpi));
	return ::getsockopt(_sockfd, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getSocketOptString(char *buf, int len) const
{
	struct tcp_info tcpi;
	bool ok = getSocketOpt(&tcpi);
	if (ok)
	{
		snprintf(buf, len, "unrecovered=%u "
						   "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
						   "lost=%u retrans=%u rtt=%u rttvar=%u "
						   "sshthresh=%u cwnd=%u total_retrans=%u",
				 tcpi.tcpi_retransmits, // Number of unrecovered [RTO] timeouts
				 tcpi.tcpi_rto,			// Retransmit timeout in usec
				 tcpi.tcpi_ato,			// Predicted tick of soft clock in usec
				 tcpi.tcpi_snd_mss,
				 tcpi.tcpi_rcv_mss,
				 tcpi.tcpi_lost,	// Lost packets
				 tcpi.tcpi_retrans, // Retransmitted packets out
				 tcpi.tcpi_rtt,		// Smoothed round trip time in usec
				 tcpi.tcpi_rttvar,	// Medium deviation
				 tcpi.tcpi_snd_ssthresh,
				 tcpi.tcpi_snd_cwnd,
				 tcpi.tcpi_total_retrans); // Total retransmits for entire connection
	}
	return ok;
}

std::string Socket::getSocketOptString() const
{
	char buf[1024];
	buf[0] = '\0';
	getSocketOptString(buf, sizeof buf);
	return std::string(buf);
}

int Socket::bind(int port)
{
	_port = port;
	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(struct sockaddr_in));
	serv.sin_family = AF_INET;
	serv.sin_port = htons(port);
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	std::cout << htonl(INADDR_ANY) << std::endl;
	int ret = ::bind(_sockfd, (struct sockaddr *)&serv, sizeof(serv));
	return ret;
}

int Socket::listen()
{
	int ret = ::listen(_sockfd, parameter::backLog);
	return ret;
}

Socket Socket::accept_raw()
{
	int connfd = -1;
	struct sockaddr_in client;
	socklen_t len = sizeof(client);
	// printf("即将调用accept函数来创建套接字\r\n");
	connfd = ::accept(_sockfd, (struct sockaddr *)&client, &len);

	if (connfd < 0)
	{
		// printf("创建套结字失败\r\n");
		return Socket(connfd);
	}

	struct sockaddr_in *sock = (struct sockaddr_in *)&client;
	int port = ntohs(sock->sin_port);
	struct in_addr in = sock->sin_addr;
	char ip[INET_ADDRSTRLEN];

	inet_ntop(AF_INET, &client.sin_addr.s_addr, ip, sizeof(ip));
	// printf("ip:%s", ip);
	// printf("调用accept函数成功,返回Socket对象\r\n");
	return Socket(connfd, std::string(ip), port);
}

Socket Socket::accept()
{
	auto ret(accept_raw());
	if (ret.isUseful())
	{
		// printf("即将返回创建的套接字\r\n");
		return ret;
	}
	netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);
	// printf("继续创建套接字\r\n");
	auto con(accept_raw());
	if (con.isUseful())
	{
		return con;
	}
	return accept();
}

ssize_t Socket::read(void *buf, size_t count)
{
	auto ret = ::read(_sockfd, buf, count);
	if (ret >= 0)
	{
		return ret;
	}
	if (ret == -1 && errno == EINTR)
	{
		return read(buf, count);
	}
	netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLHUP);
	return ::read(_sockfd, buf, count);
}

void Socket::connect(const char *ip, int port)
{
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &addr.sin_addr);
	_ip = std::string(ip);
	_port = port;
	auto ret = ::connect(_sockfd, (struct sockaddr *)&addr, sizeof(sockaddr_in));
	if (ret == 0)
	{
		return;
	}
	if (ret == -1 && errno == EINTR)
	{
		return connect(ip, port);
	}
	netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLOUT);
	return connect(ip, port);
}

ssize_t Socket::send(const void *buf, size_t count)
{
	size_t sendIdx = ::send(_sockfd, buf, count, MSG_NOSIGNAL);
	if (sendIdx >= count)
	{
		return count;
	}
	netco::Scheduler::getScheduler()->getProcessor(threadIdx)->waitEvent(_sockfd, EPOLLOUT);
	return send((char *)buf + sendIdx, count - sendIdx);
}

int Socket::shutdownWrite()
{
	int ret = ::shutdown(_sockfd, SHUT_WR);
	return ret;
}

int Socket::setTcpNoDelay(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, IPPROTO_TCP, TCP_NODELAY,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

int Socket::setReuseAddr(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

int Socket::setReusePort(bool on)
{
	int ret = -1;
#ifdef SO_REUSEPORT
	int optval = on ? 1 : 0;
	ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT,
					   &optval, static_cast<socklen_t>(sizeof optval));
#endif
	return ret;
}

int Socket::setKeepAlive(bool on)
{
	int optval = on ? 1 : 0;
	int ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_KEEPALIVE,
						   &optval, static_cast<socklen_t>(sizeof optval));
	return ret;
}

int Socket::setNonBolckSocket()
{
	auto flags = fcntl(_sockfd, F_GETFL, 0);
	int ret = fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK); // ���óɷ�����ģʽ
	return ret;
}

int Socket::setBlockSocket()
{
	auto flags = fcntl(_sockfd, F_GETFL, 0);
	int ret = fcntl(_sockfd, F_SETFL, flags & ~O_NONBLOCK); // ���ó�����ģʽ��
	return ret;
}

void Socket::run_woke()
{
	while (true)
	{
		if (0 == m_http_conn->m_state)
		{

			if (m_http_conn->read_once(_sockfd))
			{
				// 在此处刷新时间轮
				if (getCoroutine() != nullptr)
				{
					printf("%d接收到数据，更新时间轮\r\n", _sockfd);
					Coroutine *tempPtr = getCoroutine();
					TimeWheel::TcpConnectionSlot::ptr tmp = m_weak_slot.lock();
					tempPtr->getMyProcessor()->refresh(tmp);
				}
				// printf("我要开始执行process了\r\n");
				m_http_conn->process(_sockfd);
			}
			else
			{
				if (m_http_conn->over_http == true)
				{
					if (stop == false)
					{
						printf("关闭%d\r\n", _sockfd);
						stop = true;
						close(_sockfd);
					}

					break;
				}
				// m_http_conn->improv = 1;
				// m_http_conn->timer_flag = 1;
			}
		}
		else
		{
			if (m_http_conn->write(_sockfd)) // 回应客户端成功，继续等待写事件到来
			{
				// shutdown(_sockfd, SHUT_RDWR);
				m_http_conn->m_state = 0;
				printf("%d是长连接，数据发送已经完成，继续读取数据\r\n", _sockfd);
				// m_http_conn->improv = 1;
			}
			else
			{
				printf("%d不是长连接，数据发送已经完成，关闭套接字\r\n", _sockfd);
				break;
			}
		}
	}
}

void Socket::setCoroutine(Coroutine *co)
{
	if (coroutine == nullptr)
	{
		coroutine = co;
	}
}
