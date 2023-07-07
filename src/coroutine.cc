#include "../include/coroutine.h"
#include "../include/processor.h"
#include "../include/parameter.h"

using namespace netco;

static void coWrapFunc(Processor *pP)
{
	pP->getCurRunningCo()->startFunc();
	pP->killCurCo();
}

Coroutine::Coroutine(Processor *pMyProcessor, size_t stackSize, std::function<void()> &&func)
	: coFunc_(std::move(func)), pMyProcessor_(pMyProcessor), status_(CO_DEAD), ctx_(stackSize)
{
	status_ = CO_READY;
}

Coroutine::Coroutine(Processor *pMyProcessor, size_t stackSize, std::function<void()> &func)
	: coFunc_(func), pMyProcessor_(pMyProcessor), status_(CO_DEAD), ctx_(stackSize)
{
	status_ = CO_READY;
}

Coroutine::~Coroutine()
{
}

void Coroutine::resume()
{
	Context *pMainCtx = pMyProcessor_->getMainCtx();
	switch (status_)
	{
	case CO_READY:
		status_ = CO_RUNNING;
		ctx_.makeContext((void (*)(void))coWrapFunc, pMyProcessor_, pMainCtx);
		ctx_.swapToMe(pMainCtx);
		break;

	case CO_WAITING:
		status_ = CO_RUNNING;
		ctx_.swapToMe(pMainCtx);
		break;

	default:

		break;
	}
}
/**
 * @brief 放弃CPU的执行权
 * 但是此函数只是将协程的状态设置为CO_WAITING
 */
void Coroutine::yield()
{
	status_ = CO_WAITING;
};