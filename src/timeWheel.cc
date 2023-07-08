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
    if (!m_wheel.front().empty())
    {
        printf("loopFunc: 连接数目:%d\r\n", m_wheel.front().size());
        printf("loopFunc: use_count:%d\r\n", m_wheel.front()[0].use_count());
    }

    m_wheel.pop();
    std::vector<TcpConnectionSlot::ptr> tmp;
    m_wheel.push(tmp);
    // DebugLog << "push new bucket";
}

void TimeWheel::fresh(TcpConnectionSlot::ptr slot)
{
    // DebugLog << "fresh connection";
    m_wheel.back().emplace_back(slot);
    printf("use_count:%d\r\n", slot.use_count());
}