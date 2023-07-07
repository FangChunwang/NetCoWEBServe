#include "../include/timeWheel.h"

using namespace netco;

TimeWheel::TimeWheel(int num, int interval)
{
    bucket_count = num;
    m_interval = interval;

    for (int i = 0; i < bucket_count; ++i)
    {
        std::vector<TcpConnectionSlot::ptr> tmp;
        m_wheel.push(tmp);
    }
}

TimeWheel::~TimeWheel()
{
}

void TimeWheel::loopFunc()
{
    // DebugLog << "pop src bucket";
    m_wheel.pop();
    std::vector<TcpConnectionSlot::ptr> tmp;
    m_wheel.push(tmp);
    // DebugLog << "push new bucket";
}

void TimeWheel::fresh(TcpConnectionSlot::ptr slot)
{
    // DebugLog << "fresh connection";
    m_wheel.back().emplace_back(slot);
}