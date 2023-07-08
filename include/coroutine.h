#pragma once
#include <functional>
#include <memory>
#include "context.h"
#include "utils.h"
#include "abstractSlot.h"

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
	class Socket;

	class Coroutine
	{
	public:
		Coroutine(Processor *, Socket *, size_t stackSize, std::function<void()> &&);
		Coroutine(Processor *, Socket *, size_t stackSize, std::function<void()> &);
		~Coroutine();

		DISALLOW_COPY_MOVE_AND_ASSIGN(Coroutine);

		void resume();

		void yield();

		Processor *getMyProcessor() { return m_pMyProcessor; }

		void startFunc() { coFunc_(); };

		// 返回上下文环境
		Context *getCtx() { return &ctx_; }

		Socket *getSocket() { return clientSocket; }

		void bindSocketToCoroutine(Coroutine *co);

	private:
		std::function<void()> coFunc_;

		Processor *m_pMyProcessor;
		Socket *clientSocket;

		int status_;

		Context ctx_;
	};

}