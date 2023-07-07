#pragma once
#include "parameter.h"
#include "utils.h"
#include <iostream>
namespace netco
{
	struct MemBlockNode
	{
		union
		{
			MemBlockNode *next;
			char data;
		};
	};

	template <size_t objSize>
	class MemPool
	{
	public:
		MemPool()
			: _freeListHead(nullptr), _mallocListHead(nullptr), _mallocTimes(0)
		{
			if (objSize < sizeof(MemBlockNode))
			{
				objSize_ = sizeof(MemBlockNode);
			}
			else
			{
				objSize_ = objSize;
			}
		};

		~MemPool();

		DISALLOW_COPY_MOVE_AND_ASSIGN(MemPool);

		void *AllocAMemBlock();
		void FreeAMemBlock(void *block);

	private:
		MemBlockNode *_freeListHead;

		MemBlockNode *_mallocListHead;

		size_t _mallocTimes;

		size_t objSize_;
	};

	template <size_t objSize>
	MemPool<objSize>::~MemPool()
	{
		while (_mallocListHead)
		{
			MemBlockNode *mallocNode = _mallocListHead;
			_mallocListHead = mallocNode->next;
			free(static_cast<void *>(mallocNode));
		}
		std::cout << "The time of _mallocTimes��" << _mallocTimes << std::endl;
		std::cout << "The mempool is deleted." << std::endl;
	}

	template <size_t objSize>
	void *MemPool<objSize>::AllocAMemBlock()
	{
		void *ret;
		if (nullptr == _freeListHead)
		{
			size_t mallocCnt = parameter::memPoolMallocObjCnt + _mallocTimes;
			void *newMallocBlk = malloc(mallocCnt * objSize_ + sizeof(MemBlockNode));
			MemBlockNode *mallocNode = static_cast<MemBlockNode *>(newMallocBlk);
			mallocNode->next = _mallocListHead;
			_mallocListHead = mallocNode;
			newMallocBlk = static_cast<char *>(newMallocBlk) + sizeof(MemBlockNode);
			for (size_t i = 0; i < mallocCnt; ++i)
			{
				MemBlockNode *newNode = static_cast<MemBlockNode *>(newMallocBlk);
				newNode->next = _freeListHead;
				_freeListHead = newNode;
				newMallocBlk = static_cast<char *>(newMallocBlk) + objSize_;
			}
			++_mallocTimes;
		}
		ret = &(_freeListHead->data);
		_freeListHead = _freeListHead->next;
		return ret;
	}

	template <size_t objSize>
	void MemPool<objSize>::FreeAMemBlock(void *block)
	{
		if (nullptr == block)
		{
			return;
		}
		MemBlockNode *newNode = static_cast<MemBlockNode *>(block);
		newNode->next = _freeListHead;
		_freeListHead = newNode;
	}
}
