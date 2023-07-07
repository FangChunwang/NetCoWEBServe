#pragma once
#include <vector>

namespace netco
{
	class Processor;

	enum scheduleStrategy
	{
		MIN_EVENT_FIRST = 0 , 
		ROUND_ROBIN			  
	};


	class ProcessorSelector
	{
	public:
		ProcessorSelector(std::vector<Processor*>& processors, int strategy = MIN_EVENT_FIRST) :  curPro_(-1) , strategy_(strategy) , processors_(processors) {}
		~ProcessorSelector() {}

		inline void setStrategy(int strategy) { strategy_ = strategy; };

		Processor* next();

	private:
		int curPro_;

		int strategy_;

		std::vector<Processor*>& processors_;

	};

}