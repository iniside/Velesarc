// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaPopulationTypes.h"

const FArcAreaPopulationEntry* UArcAreaPopulationDefinition::FindEntryForRole(const FGameplayTag& RoleTag) const
{
	for (const FArcAreaPopulationEntry& Entry : Entries)
	{
		if (Entry.RoleTag == RoleTag)
		{
			return &Entry;
		}
	}
	return nullptr;
}
