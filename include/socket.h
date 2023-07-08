#pragma once
#include "utils.h"
#include "parameter.h"
#include "http_conn.h"
#include "abstractSlot.h"
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include <memory>

struct tcp_info;
namespace netco
{
	class Socket
	{
	public:
		explicit Socket(int sockfd, std::string ip = "", int port = -1)
			: _sockfd(sockfd), _pRef(new int(1)), _port(port), _ip(ip), m_http_conn(new http_conn())
		{
			// printf("构造一个Socket对象，其fd为:%d\r\n", sockfd);
			// std::cout << "ip地址是:" << ip << std::endl;
			// printf("端口号是:%d\r\n", port);
			if (sockfd > 0)
			{
				setNonBolckSocket();
				// printf("成功设置套接字为非阻塞\r\n");
				m_http_conn->init();
			}
			// printf("构造一个Socket对象成功\r\n");
		}

		Socket()
			: _sockfd(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP)),
			  _pRef(new int(1)), _port(-1), _ip("")
		{
		}

		Socket(const Socket &otherSock) : _sockfd(otherSock._sockfd), m_http_conn(otherSock.m_http_conn)
		{
			*(otherSock._pRef) += 1;
			_pRef = otherSock._pRef;
			_ip = otherSock._ip;
			_port = otherSock._port;
		}

		Socket(Socket &&otherSock) : _sockfd(otherSock._sockfd), m_http_conn(otherSock.m_http_conn)
		{
			*(otherSock._pRef) += 1;
			_pRef = otherSock._pRef;
			_ip = std::move(otherSock._ip);
			_port = otherSock._port;
		}

		Socket &operator=(const Socket &otherSock) = delete;
		~Socket();
		int fd() const { return _sockfd; }

		bool isUseful() { return _sockfd >= 0; }

		int bind(int port);

		int listen();

		Socket accept();

		ssize_t read(void *buf, size_t count);

		void connect(const char *ip, int port);

		ssize_t send(const void *buf, size_t count);

		std::string ip() { return _ip; }

		int port() { return _port; }

		bool getSocketOpt(struct tcp_info *) const;

		bool getSocketOptString(char *buf, int len) const;

		std::string getSocketOptString() const;

		int shutdownWrite();

		int setTcpNoDelay(bool on);

		int setReuseAddr(bool on);

		int setReusePort(bool on);

		int setKeepAlive(bool on);

		int setNonBolckSocket();

		int *getRef() { return _pRef; }

		int setBlockSocket();

		// void SetNoSigPipe();
		void run_woke();
		void add() { ++(*_pRef); }

		void setClose() { stop = true; }
		bool getStatus() { return stop; }

		http_conn *getHttpConnect() { return m_http_conn; }

	private:
		Socket accept_raw();
		// fd
		int _sockfd;
		int *_pRef;
		int _port;

		// ip
		std::string _ip;

		http_conn *m_http_conn;
		bool stop{false};
	};
}