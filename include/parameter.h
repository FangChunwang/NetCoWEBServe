#pragma once
#include <stddef.h>

namespace netco
{
	namespace parameter
	{

		const static size_t coroutineStackSize = 32 * 1024;

		static constexpr int epollerListFirstSize = 16;

		static constexpr int epollTimeOutMs = 10000;

		constexpr static unsigned backLog = 4096;

		static constexpr size_t memPoolMallocObjCnt = 40;
	}

}