#include "../include/coroutine.h"
#include "../include/processor.h"
#include "../include/parameter.h"
#include "../include/socket.h"
#include "../include/timeWheel.h"

using namespace netco;

static void coWrapFunc(Processor *pP, Coroutine *co)
{
	if (co->getSocket() != nullptr)
	{ // 创建一个sharedpr管理的对象
		std::shared_ptr<Socket> socketRep = std::make_shared<Socket>(co->getSocket()->fd(), co->getSocket()->ip(), co->getSocket()->port());
		TimeWheel::TcpConnectionSlot::ptr tmp = std::make_shared<AbstractSlot<Socket>>(co->getSocket());
		// printf("use_count：%d\r\n", tmp.use_count());
		// co->getSocket()->m_weak_slot = tmp;
		// printf("coWrapFunc_0: use_count：%d\r\n", tmp.use_count());
		pP->getTimeWheel()->fresh(tmp);
		// printf("coWrapFunc_1: use_count：%d\r\n", tmp.use_count());
	}
	// if (co->getSocket() != nullptr)
	//  printf("coWrapFunc_2: use_count：%d\r\n", co->getSocket()->m_weak_slot.use_count());
	pP->getCurRunningCo()->startFunc();

	pP->killCurCo();
}

Coroutine::Coroutine(Processor *pMyProcessor, Socket *socket, size_t stackSize, std::function<void()> &&func)
	: coFunc_(std::move(func)), m_pMyProcessor(pMyProcessor), clientSocket(socket), status_(CO_DEAD), ctx_(stackSize)
{
	status_ = CO_READY;
}

Coroutine::Coroutine(Processor *pMyProcessor, Socket *socket, size_t stackSize, std::function<void()> &func)
	: coFunc_(func), m_pMyProcessor(pMyProcessor), clientSocket(socket), status_(CO_DEAD), ctx_(stackSize)
{
	status_ = CO_READY;
}

Coroutine::~Coroutine()
{
}

void Coroutine::resume()
{
	Context *pMainCtx = m_pMyProcessor->getMainCtx();
	switch (status_)
	{
	case CO_READY:
		status_ = CO_RUNNING;
		ctx_.makeContext((void (*)(void))coWrapFunc, m_pMyProcessor, this, pMainCtx);
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