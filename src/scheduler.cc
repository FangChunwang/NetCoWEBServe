#include "../include/scheduler.h"

#include <sys/sysinfo.h>
#include <cstdio>
using namespace netco;

Scheduler *Scheduler::pScher_ = nullptr;
std::mutex Scheduler::scherMtx_;

Scheduler::Scheduler()
	: proSelector_(processors_)
{
}

Scheduler::~Scheduler()
{
	for (auto pP : processors_)
	{
		pP->stop();
	}
	for (auto pP : processors_)
	{
		pP->join();
		delete pP;
	}
}

bool Scheduler::startScheduler(int threadCnt)
{
	// printf("接下来将创建%d个处理器对象\r\n", threadCnt - 2);
	for (int i = 0; i < threadCnt - 2; ++i)
	{
		processors_.emplace_back(new Processor(i));
		// printf("已经完成第%d个处理器对象的创建\r\n", i);
		processors_[i]->loop();
	}
	return true;
}

Scheduler *Scheduler::getScheduler()
{
	if (nullptr == pScher_)
	{
		std::lock_guard<std::mutex> lock(scherMtx_);
		if (nullptr == pScher_)
		{
			pScher_ = new Scheduler();
			pScher_->startScheduler(::get_nprocs_conf());
		}
	}
	return pScher_;
}

void Scheduler::createNewCo(std::function<void()> &&func, size_t stackSize)
{
	// printf("将创建一个新的携程\r\n");
	proSelector_.next()->goNewCo(std::move(func), stackSize);
}

void Scheduler::createNewCo(std::function<void()> &func, size_t stackSize)
{
	// printf("将创建一个新的携程\r\n");
	proSelector_.next()->goNewCo(func, stackSize);
}

void Scheduler::join()
{
	for (auto pP : processors_)
	{
		pP->join();
	}
}

Processor *Scheduler::getProcessor(int id)
{
	return processors_[id];
}

int Scheduler::getProCnt()
{
	return static_cast<int>(processors_.size());
}
