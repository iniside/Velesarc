// Copyright Lukasz Baran. All Rights Reserved.

#include "Display/ArcTagDisplayRegistry.h"

namespace ArcTagDisplayRegistry_Private
{
	// No free helpers needed yet — impl lives on the class.
}

const FArcTagDisplayData* UArcTagDisplayRegistry::FindDisplay(FGameplayTag Tag) const
{
	// Exact match first.
	if (const FArcTagDisplayData* Found = Entries.Find(Tag))
	{
		return Found;
	}

	// Walk up the tag hierarchy. RequestDirectParent() returns an invalid tag at the root,
	// so the loop always terminates.
	FGameplayTag Current = Tag.RequestDirectParent();
	while (Current.IsValid())
	{
		if (const FArcTagDisplayData* Found = Entries.Find(Current))
		{
			return Found;
		}
		Current = Current.RequestDirectParent();
	}

	return nullptr;
}
