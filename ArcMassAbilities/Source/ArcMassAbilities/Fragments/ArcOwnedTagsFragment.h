// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tags/ArcTagCountContainer.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "Events/ArcPendingEvents.h"
#include "ArcOwnedTagsFragment.generated.h"

struct FArcTagDelta
{
	int32 OldCount;
	int32 NewCount;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcOwnedTagsFragment : public FMassFragment
{
	GENERATED_BODY()

	FArcTagDelta AddTag(FGameplayTag Tag);
	FArcTagDelta RemoveTag(FGameplayTag Tag);
	void AddTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity);
	void RemoveTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity);

	const FArcTagCountContainer& GetTags() const { return Tags; }

	UPROPERTY(EditAnywhere)
	FArcTagCountContainer Tags;
};

template<>
struct TMassFragmentTraits<FArcOwnedTagsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
