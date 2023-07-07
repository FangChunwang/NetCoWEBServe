#include "../include/timer.h"
#include "../include/coroutine.h"
#include "../include/epoller.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <string.h>
#include <unistd.h>

using namespace netco;

Timer::Timer()
	: timeFd_(-1)
{}

Timer::~Timer() 
{
	if (isTimeFdUseful())
	{
		::close(timeFd_);
	}
}

bool Timer::init(Epoller* pEpoller)
{
	timeFd_ = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (isTimeFdUseful())
	{
		return pEpoller->addEv(nullptr, timeFd_, EPOLLIN | EPOLLPRI | EPOLLRDHUP);
	}
	return false;
}

void Timer::getExpiredCoroutines(std::vector<Coroutine*>& expiredCoroutines)
{
	Time nowTime = Time::now();
	while (!timerCoHeap_.empty() && timerCoHeap_.top().first <= nowTime)
	{
		expiredCoroutines.push_back(timerCoHeap_.top().second);
		timerCoHeap_.pop();
	}

	if (!expiredCoroutines.empty())
	{
		ssize_t cnt = TIMER_DUMMYBUF_SIZE;
		while (cnt >= TIMER_DUMMYBUF_SIZE)
		{
			cnt = ::read(timeFd_, dummyBuf_, TIMER_DUMMYBUF_SIZE);
		}
	}

	if (!timerCoHeap_.empty())
	{
		Time time = timerCoHeap_.top().first;
		resetTimeOfTimefd(time);
	}
}

void Timer::runAt(Time time, Coroutine* pCo)
{
	timerCoHeap_.push(std::move(std::pair<Time, Coroutine*>(time, pCo)));
	if (timerCoHeap_.top().first == time)
	{
		resetTimeOfTimefd(time);
	}
}

/**
 * @brief 重新设置计时器的计时时间
 * 
 * @param time 
 * @return true 
 * @return false 
 */
bool Timer::resetTimeOfTimefd(Time time)
{
	struct itimerspec newValue;
	struct itimerspec oldValue;
	memset(&newValue, 0, sizeof newValue);
	memset(&oldValue, 0, sizeof oldValue);
	newValue.it_value = time.timeIntervalFromNow();
	int ret = ::timerfd_settime(timeFd_, 0, &newValue, &oldValue);
	return ret < 0 ? false : true;
}

/**
 * @brief 设置协程在多长时间后执行
 * 
 * @param time 延时时间，单位ms
 * @param pCo 延时协程
 */
void Timer::runAfter(Time time, Coroutine* pCo)
{
	Time runTime(Time::now().getTimeVal() + time.getTimeVal());
	runAt(runTime, pCo);
}

void Timer::wakeUp()
{
	resetTimeOfTimefd(Time::now());
}
