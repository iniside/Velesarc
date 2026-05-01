// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcActiveEffectHandle.h"
#include <atomic>

namespace ArcActiveEffectHandlePrivate
{
	static std::atomic<uint32> GNextId{1};
}

FArcActiveEffectHandle FArcActiveEffectHandle::Generate()
{
	FArcActiveEffectHandle Handle;
	Handle.Id = ArcActiveEffectHandlePrivate::GNextId.fetch_add(1, std::memory_order_relaxed);
	return Handle;
}
