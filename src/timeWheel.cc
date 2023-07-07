#include "../include/timeWheel.h"

using namespace netco;

/**
 * @brief Construct a new Time Wheel:: Time Wheel object
 * 
 * @param num 时间轮的凹槽数
 * @param interval 间隔时间
 */
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
/**
 * @brief Destroy the Time Wheel:: Time Wheel object
 * 该函数没有做任何事情
 */
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