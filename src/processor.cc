#include "../include/processor.h"
#include "../include/parameter.h"
#include "../include/spinlock_guard.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <cstdio>
#include <utility>
#include <functional>
using namespace netco;

__thread int threadIdx = -1;

/**
 * @brief Construct a new Processor:: Processor object
 * 该构造函数会初始化mainCtx_
 * @param tid 处理器唯一标识
 */
Processor::Processor(int tid)
	: tid_(tid), status_(PRO_STOPPED), pLoop_(nullptr), runningNewQue_(0),
	  pCurCoroutine_(nullptr), mainCtx_(0), m_timeWheel(new TimeWheel(6, 10000))
{
	mainCtx_.makeCurContext();
}

Processor::~Processor()
{
	if (PRO_RUNNING == status_)
	{
		stop();
	}
	if (PRO_STOPPING == status_)
	{
		join();
	}
	if (nullptr != pLoop_)
	{
		delete pLoop_;
	}
	for (auto co : coSet_)
	{
		delete co;
	}
	if (m_timeWheel != nullptr)
	{
		delete m_timeWheel;
	}
}

void Processor::resume(Coroutine *pCo)
{
	if (nullptr == pCo)
	{
		return;
	}

	if (coSet_.find(pCo) == coSet_.end())
	{
		return;
	}

	pCurCoroutine_ = pCo;
	pCo->resume();
}

void Processor::yield()
{
	pCurCoroutine_->yield();
	mainCtx_.swapToMe(pCurCoroutine_->getCtx());
}

void Processor::wait(Time time)
{
	pCurCoroutine_->yield();
	timer_.runAfter(time, pCurCoroutine_);
	mainCtx_.swapToMe(pCurCoroutine_->getCtx());
}

void Processor::goCo(Coroutine *pCo)
{
	{
		SpinlockGuard lock(newQueLock_);
		newCoroutines_[!runningNewQue_].push(pCo);
	}
	wakeUpEpoller();
}

void Processor::goCoBatch(std::vector<Coroutine *> &cos)
{
	{
		SpinlockGuard lock(newQueLock_);
		for (auto pCo : cos)
		{
			newCoroutines_[!runningNewQue_].push(pCo);
		}
	}
	wakeUpEpoller();
}

bool Processor::loop()
{
	//	初始化epoll
	if (!epoller_.init())
	{
		return false;
	}

	//	初始化定时器
	if (!timer_.init(&epoller_))
	{
		return false;
	}

	auto timeWheelLoop = [this]
	{
		while (true)
		{
			// printf("开始运行时间轮的loopfunc\r\n");
			m_timeWheel->loopFunc();
			printf("时间轮转动一次\r\n");
			wait(Time(this->m_timeWheel->getInterval()));
		}
	};
	// 时间轮的初始化在处理器创建时已经完成
	goNewCo(timeWheelLoop, nullptr, parameter::coroutineStackSize);

	pLoop_ = new std::thread(
		[this]
		{
			printf("this is a new thread\r\n");
			threadIdx = tid_;
			status_ = PRO_RUNNING;
			while (PRO_RUNNING == status_)
			{
				//
				if (actCoroutines_.size())
				{
					actCoroutines_.clear();
				}
				if (timerExpiredCo_.size())
				{
					timerExpiredCo_.clear();
				}
				// 得到epoll上活跃的连接
				epoller_.getActEvServ(parameter::epollTimeOutMs, actCoroutines_);

				// 得到计时器达到的协程，先执行计时器到达的协程
				timer_.getExpiredCoroutines(timerExpiredCo_);
				size_t timerCoCnt = timerExpiredCo_.size();
				for (size_t i = 0; i < timerCoCnt; ++i)
				{
					resume(timerExpiredCo_[i]);
				}

				// 再执行刚到达的协程
				Coroutine *pNewCo = nullptr;
				int runningQue = runningNewQue_;

				while (!newCoroutines_[runningQue].empty())
				{
					{
						pNewCo = newCoroutines_[runningQue].front();
						newCoroutines_[runningQue].pop();
						coSet_.insert(pNewCo);
					}
					resume(pNewCo);
				}

				{
					SpinlockGuard lock(newQueLock_);
					runningNewQue_ = !runningQue;
				}

				// 最后执行epoll唤醒的协程
				size_t actCoCnt = actCoroutines_.size();
				for (size_t i = 0; i < actCoCnt; ++i)
				{
					resume(actCoroutines_[i]);
				}

				// 回收执行完的协程
				for (auto deadCo : removedCo_)
				{
					coSet_.erase(deadCo);
					// delete deadCo;
					{
						SpinlockGuard lock(coPoolLock_);
						coPool_.delete_obj(deadCo);
					}
					printf("处理器%d还剩余有%ld个连接\r\n", tid_, getCoCnt());
				}
				removedCo_.clear();
			}
			status_ = PRO_STOPPED;
		});
	return true;
}

void Processor::waitEvent(int fd, int ev)
{
	epoller_.addEv(pCurCoroutine_, fd, ev);
	yield();
	epoller_.removeEv(pCurCoroutine_, fd, ev);
}

void Processor::stop()
{
	status_ = PRO_STOPPING;
}

void Processor::join()
{
	pLoop_->join();
}

void Processor::wakeUpEpoller()
{
	timer_.wakeUp();
}

/**
 * 运行一个新的协程
 * 该函数会从协程中创建一个协程对象
 * 创建了一个协程也就意味着有一个新的TCP连接
 */
void Processor::goNewCo(std::function<void()> &&coFunc, Socket *socket, size_t stackSize)
{
	// Coroutine* pCo = new Coroutine(this, stackSize, std::move(coFunc));
	printf("处理器%d有%ld个连接\r\n", tid_, getCoCnt() + 1);
	Coroutine *pCo = nullptr;

	{
		SpinlockGuard lock(coPoolLock_);
		pCo = coPool_.new_obj(this, socket, stackSize, std::move(coFunc));
	}

	goCo(pCo);
}

void Processor::goNewCo(std::function<void()> &coFunc, Socket *socket, size_t stackSize)
{
	// Coroutine* pCo = new Coroutine(this, stackSize, coFunc);
	Coroutine *pCo = nullptr;

	{
		SpinlockGuard lock(coPoolLock_);
		pCo = coPool_.new_obj(this, socket, stackSize, coFunc);
	}

	goCo(pCo);
}

void Processor::killCurCo()
{
	removedCo_.push_back(pCurCoroutine_);
}

void Processor::refresh(TimeWheel::TcpConnectionSlot::ptr ptrTemp)
{
	getTimeWheel()->fresh(ptrTemp);
}