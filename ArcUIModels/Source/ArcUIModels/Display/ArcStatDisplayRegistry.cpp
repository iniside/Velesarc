// Copyright Lukasz Baran. All Rights Reserved.

#include "Display/ArcStatDisplayRegistry.h"

const FArcStatDisplayData* UArcStatDisplayRegistry::FindDisplay(const FArcAttributeRef& Stat) const
{
	return Entries.Find(Stat);
}
