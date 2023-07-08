#pragma once
#include <queue>
#include <set>
#include <mutex>
#include <thread>
#include "objpool.h"
#include "spinlock.h"
#include "context.h"
#include "coroutine.h"
#include "epoller.h"
#include "timer.h"
#include "abstractSlot.h"
#include "timeWheel.h"

extern __thread int threadIdx;

namespace netco
{
	class Socket;
	enum processerStatus
	{
		PRO_RUNNING = 0,
		PRO_STOPPING,
		PRO_STOPPED
	};

	enum newCoAddingStatus
	{
		NEWCO_ADDING = 0,
		NEWCO_ADDED
	};

	class Processor
	{
	public:
		Processor(int);
		~Processor();

		DISALLOW_COPY_MOVE_AND_ASSIGN(Processor);

		// 运行一个新的协程
		void goNewCo(std::function<void()> &&func, Socket *socket, size_t stackSize);
		void goNewCo(std::function<void()> &func, Socket *socket, size_t stackSize);
		// void goNewCoAfterTime(std::function<void()> &func, size_t stackSize, Time time);

		void yield();

		// 等待一个时间后运行协程
		void wait(Time time);

		// 杀死一个协程
		void killCurCo();

		bool loop();

		void stop();

		void join();

		// 让fd等待某个事件并让出CPU
		void waitEvent(int fd, int ev);

		// 获取当前正在运行的协程
		inline Coroutine *getCurRunningCo() { return pCurCoroutine_; };

		inline Context *getMainCtx() { return &mainCtx_; }

		inline size_t getCoCnt() { return coSet_.size(); }

		void goCo(Coroutine *co);

		void goCoBatch(std::vector<Coroutine *> &cos);

		void registerToTimeWheel();

		TimeWheel *getTimeWheel() { return m_timeWheel; }
		void refresh(TimeWheel::TcpConnectionSlot::ptr ptrTemp);

	private:
		// 恢复运行一个协程
		void resume(Coroutine *);

		inline void wakeUpEpoller();

	private:
		// 处理器标识
		int tid_;

		int status_;

		std::thread *pLoop_;

		// 协程双缓冲队列
		std::queue<Coroutine *> newCoroutines_[2];

		// 正在运行的协程队列号
		volatile int runningNewQue_;

		Spinlock newQueLock_;

		Spinlock coPoolLock_;

		// std::mutex newCoQueMtx_;

		// 在epoll上活跃的协程
		std::vector<Coroutine *> actCoroutines_;

		std::set<Coroutine *> coSet_;

		// 存放定时时间到达的协程
		std::vector<Coroutine *> timerExpiredCo_;

		// 存放即将被回收的协程
		std::vector<Coroutine *> removedCo_;

		Epoller epoller_;

		Timer timer_;

		ObjPool<Coroutine> coPool_;

		Coroutine *pCurCoroutine_;

		Context mainCtx_;

		TimeWheel *m_timeWheel;
	};
}
