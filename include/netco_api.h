#pragma once
#include "scheduler.h"
#include "mstime.h"
#include "socket.h"

namespace netco
{

	void co_go(std::function<void()> &func, Socket *socket, size_t stackSize = parameter::coroutineStackSize, int tid = -1);
	void co_go(std::function<void()> &&func, Socket *socket, size_t stackSize = parameter::coroutineStackSize, int tid = -1);

	void co_sleep(Time t);

	void sche_join();

}
