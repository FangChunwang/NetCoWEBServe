#pragma once
#include <vector>
#include <functional>

#include "processor.h"
#include "processor_selector.h"

namespace netco
{

	class Scheduler
	{
	protected:
		Scheduler();
		~Scheduler();

	public:
		DISALLOW_COPY_MOVE_AND_ASSIGN(Scheduler);

		static Scheduler *getScheduler();

		void createNewCo(std::function<void()> &&func, size_t stackSize);
		void createNewCo(std::function<void()> &func, size_t stackSize);

		Processor *getProcessor(int);

		int getProCnt();

		void join();

	private:
		bool startScheduler(int threadCnt);

		static Scheduler *pScher_;

		static std::mutex scherMtx_;

		std::vector<Processor *> processors_;

		ProcessorSelector proSelector_;
	};

}