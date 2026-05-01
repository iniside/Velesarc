#pragma once

#include "Mass/EntityHandle.h"

struct FArcItemData;
struct FMassEntityManager;

namespace ArcItemMassHelpers
{
	ARCCORE_API bool GetMassEntity(const FArcItemData* InItem,
	                               FMassEntityManager*& OutManager,
	                               FMassEntityHandle& OutEntity);
}
