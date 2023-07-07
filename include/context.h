#pragma once
#include "utils.h"
#include "parameter.h"
#include <ucontext.h>

namespace netco
{

	class Processor;
	class Context
	{
	public:
		Context(size_t stackSize);
		~Context();

		Context(const Context &otherCtx)
			: ctx_(otherCtx.ctx_), pStack_(otherCtx.pStack_)
		{
		}

		Context(Context &&otherCtx)
			: ctx_(otherCtx.ctx_), pStack_(otherCtx.pStack_)
		{
		}

		Context &operator=(const Context &otherCtx) = delete;

		// 设置上下文环境
		void makeContext(void (*func)(), Processor *, Context *);

		void makeCurContext();

		// 移交CPU控制权
		void swapToMe(Context *pOldCtx);

		inline struct ucontext_t *getUCtx() { return &ctx_; };

	private:
		struct ucontext_t ctx_;

		void *pStack_;

		size_t stackSize_;
	};

}