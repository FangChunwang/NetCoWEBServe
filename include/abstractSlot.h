#pragma once
#include <memory>
#include <functional>
#include <sys/socket.h>
#include <cstdio>

namespace netco
{
    // 当 AbstractSlot 对象析构时，会调用 m_cb 成员函数，函数参数是指向 T 的 shared_ptr 指针
    template <typename T>
    class AbstractSlot
    {
    public:
        typedef std::shared_ptr<AbstractSlot> ptr;
        typedef std::weak_ptr<T> weakPtr;
        typedef std::shared_ptr<T> sharedPtr;

        AbstractSlot(T *socket) : m_socket(socket)
        {
        }
        ~AbstractSlot()
        {
            if (m_socket != nullptr)
            {
                // printf("调用shutdown函数\r\n");
                shutdown(m_socket->fd(), SHUT_RDWR);
                // printf("调用shutdown函数成功\r\n");
            }
        }

    private:
        T *m_socket;
    };
}