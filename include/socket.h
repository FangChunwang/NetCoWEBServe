//@Author Liu Yukang
#pragma once

#include "utils.h"
#include "parameter.h"
#include "../include/http_conn.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>

struct tcp_info;
namespace netco
{

	// Socket�࣬������Socket����Ĭ�϶��Ƿ�������
	// ְ��
	// 1���ṩfd���������API
	// 2������fd����������
	// ���������ü�������ĳһfdû�����˾ͻ�close
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

		// ���ص�ǰSocket��fd
		int fd() const { return _sockfd; }

		// ���ص�ǰSocket�Ƿ����
		bool isUseful() { return _sockfd >= 0; }

		// ��ip��port����ǰSocket
		int bind(int port);

		// ��ʼ������ǰSocket
		int listen();

		// ����һ�����ӣ�����һ�������ӵ�Socket
		Socket accept();

		// ��socket�ж�����
		ssize_t read(void *buf, size_t count);

		// ipʾ����"127.0.0.1"
		void connect(const char *ip, int port);

		// ��socket��д����
		ssize_t send(const void *buf, size_t count);

		// ��ȡ��ǰ�׽��ֵ�Ŀ��ip
		std::string ip() { return _ip; }

		// ��ȡ��ǰ�׽��ֵ�Ŀ��port
		int port() { return _port; }

		// ��ȡ�׽��ֵ�ѡ��,�ɹ��򷵻�true����֮������false
		bool getSocketOpt(struct tcp_info *) const;

		// ��ȡ�׽��ֵ�ѡ����ַ���,�ɹ��򷵻�true����֮������false
		bool getSocketOptString(char *buf, int len) const;

		// ��ȡ�׽��ֵ�ѡ����ַ���
		std::string getSocketOptString() const;

		// �ر��׽��ֵ�д����
		int shutdownWrite();

		// �����Ƿ���Nagle�㷨������Ҫ��������ݰ�����������ʱ���ܻ�����
		int setTcpNoDelay(bool on);

		// �����Ƿ��ַ����
		int setReuseAddr(bool on);

		// �����Ƿ�˿�����
		int setReusePort(bool on);

		// �����Ƿ�ʹ���������
		int setKeepAlive(bool on);

		// ����socketΪ��������
		int setNonBolckSocket();

		int *getRef() { return _pRef; }

		// ����socketΪ������
		int setBlockSocket();

		// void SetNoSigPipe();
		void run_woke();
		http_conn *getHttpConnect() { return m_http_conn; }

	private:
		// ����һ�����ӣ�����һ�������ӵ�Socket
		Socket accept_raw();
		// fd
		int _sockfd;

		// ���ü���
		int *_pRef;

		// �˿ں�
		int _port;

		// ip
		std::string _ip;

		http_conn *m_http_conn;
	};

}