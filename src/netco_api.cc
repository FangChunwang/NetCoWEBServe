#include "../include/netco_api.h"
using namespace netco;
// void netco::co_go(std::function<void()> &&func, size_t stackSize, int tid)
// {
// 	if (tid < 0)
// 	{
// 		netco::Scheduler::getScheduler()->createNewCo(std::move(func), stackSize);
// 	}
// 	else
// 	{
// 		tid %= netco::Scheduler::getScheduler()->getProCnt();
// 		netco::Scheduler::getScheduler()->getProcessor(tid)->goNewCo(std::move(func), stackSize);
// 	}
// }

void netco::co_go(std::function<void()> func, Socket *socket, size_t stackSize, int tid)
{
	if (tid < 0)
	{
		netco::Scheduler::getScheduler()->createNewCo(func, socket, stackSize);
	}
	else
	{
		tid %= netco::Scheduler::getScheduler()->getProCnt();
		netco::Scheduler::getScheduler()->getProcessor(tid)->goNewCo(func, socket, stackSize);
	}
}

void netco::co_sleep(Time time)
{
	netco::Scheduler::getScheduler()->getProcessor(threadIdx)->wait(time);
}

void netco::sche_join()
{
	netco::Scheduler::getScheduler()->join();
}