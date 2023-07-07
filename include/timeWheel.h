#pragma once
#include <memory>
#include <queue>

#include "abstractSlot.h"

namespace netco
{
    class Processor;
    class Socket;
    class TimeWheel
    {
    public:
        typedef std::shared_ptr<TimeWheel> ptr;
        typedef AbstractSlot<Socket> TcpConnectionSlot;

        TimeWheel(int num, int interval);
        ~TimeWheel();
        void fresh(TcpConnectionSlot::ptr slot);
        void loopFunc();

    private:
        int bucket_count;
        int m_interval; // 时间间隔
        std::queue<std::vector<TcpConnectionSlot::ptr>> m_wheel;
    };
}