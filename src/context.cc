#include "../include/context.h"
#include "../include/parameter.h"
#include <stdlib.h>
#include <cstdio>

using namespace netco;

Context::Context(size_t stackSize)
	: pStack_(nullptr), stackSize_(stackSize)
{
}

Context::~Context()
{
	if (pStack_)
	{
		free(pStack_);
	}
}

void Context::makeContext(void (*func)(), Processor *pP, Context *pLink)
{
	if (nullptr == pStack_)
	{
		pStack_ = malloc(stackSize_);
	}
	::getcontext(&ctx_);
	ctx_.uc_stack.ss_sp = pStack_;
	ctx_.uc_stack.ss_size = parameter::coroutineStackSize;
	ctx_.uc_link = pLink->getUCtx();
	makecontext(&ctx_, func, 1, pP);
}

void Context::makeCurContext()
{
	// printf("将为mainCTX_设置当前上下文执行环境\r\n");
	::getcontext(&ctx_);
	// printf("设置上下文执行环境已经完成\r\n");
}

void Context::swapToMe(Context *pOldCtx)
{
	if (nullptr == pOldCtx)
	{
		setcontext(&ctx_);
	}
	else
	{
		// printf("我已完成切换，接下来将运行mainCTX_的上下文环境\r\n");
		swapcontext(pOldCtx->getUCtx(), &ctx_);
	}
}
