#include <iostream>
#include <sys/sysinfo.h>

#include "./include/processor.h"
#include "./include/netco_api.h"
#include "./include/socket.h"
#include "./include/mutex.h"
#include "./include/http_conn.h"
using namespace netco;

// netco http response with one acceptor test
void single_acceptor_server_test()
{
	netco::co_go(
		[]
		{
			// std::cout << "我开始创建套接字啦" << std::endl;
			netco::Socket listener;
			if (listener.isUseful())
			{
				listener.setTcpNoDelay(true);
				listener.setReuseAddr(true);
				listener.setReusePort(true);
				if (listener.bind(8099) < 0)
				{
					printf("创建套接字失败\r\n");
					return;
				}
				listener.listen();
			}
			else
			{
				printf("创建套结字失败\r\n");
			}
			// printf("等待新的连接到来\r\n");
			while (1)
			{
				netco::Socket *conn = new netco::Socket(listener.accept());
				printf("新建立的连接的文字描述符是%d: ", conn->fd());
				conn->setTcpNoDelay(true);
				// 这个地方应该调用http对象的处理函数
				netco::co_go(
					[conn]
					{
						conn->run_woke();
						if (conn->getRef() != nullptr)
						{
							delete conn;
						}
					});
			}
		});
}

int main(int argc, const char *argv[])
{
	if (argc < 2)
	{
		printf("eg: ./a.out path\n");
		exit(1);
	}

	// 修改当前工作目录
	int ret = chdir(argv[1]);
	// netco::RWMutex mu;
	// mutex_test(mu);
	std::cout << "start..." << std::endl;
	single_acceptor_server_test();
	// multi_acceptor_server_test();
	// client_test();
	netco::sche_join();
	std::cout << "end" << std::endl;
	return 0;
}
