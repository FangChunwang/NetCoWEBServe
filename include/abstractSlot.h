#pragma once
#include <memory>
#include <functional>

#include "spinlock.h"

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

        AbstractSlot(weakPtr ptr, std::function<void(sharedPtr)> cb) : m_weak_ptr(ptr), m_cb(cb)
        {
        }
        ~AbstractSlot()
        {
            sharedPtr ptr = m_weak_ptr.lock();
            if (ptr)
            {
                m_cb(ptr);
            }
        }

    private:
        weakPtr m_weak_ptr;
        std::function<void(sharedPtr)> m_cb;
        Spinlock spinLock;
    };
}