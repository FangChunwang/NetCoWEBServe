#pragma once
#include <functional>
#include "context.h"
#include "utils.h"

namespace netco
{

	enum coStatus
	{
		CO_READY = 0,
		CO_RUNNING,
		CO_WAITING,
		CO_DEAD
	};

	class Processor;

	class Coroutine
	{
	public:
		Coroutine(Processor *, size_t stackSize, std::function<void()> &&);
		Coroutine(Processor *, size_t stackSize, std::function<void()> &);
		~Coroutine();

		DISALLOW_COPY_MOVE_AND_ASSIGN(Coroutine);

		void resume();

		void yield();

		Processor *getMyProcessor() { return pMyProcessor_; }

		inline void startFunc() { coFunc_(); };

		// 返回上下文环境
		inline Context *getCtx() { return &ctx_; }

	private:
		std::function<void()> coFunc_;

		Processor *pMyProcessor_;

		int status_;

		Context ctx_;
	};

}