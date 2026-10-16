// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tags/ArcTagCountContainer.h"
#include "Mass/EntityHandle.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Events/ArcPendingEvents.h"
#include "ArcOwnedTagsFragment.generated.h"

struct FMassEntityManager;

struct FArcTagDelta
{
	int32 OldCount;
	int32 NewCount;
};

namespace ArcAbilities
{
	ARCMASSABILITIES_API void AddOwnedTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTagContainer& InTags);
	ARCMASSABILITIES_API void RemoveOwnedTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTagContainer& InTags);
}

USTRUCT()
struct ARCMASSABILITIES_API FArcOwnedTagsFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	const FArcTagCountContainer& GetTags() const { return Tags; }

	UPROPERTY(EditAnywhere)
	FArcTagCountContainer Tags;

private:
	friend void ArcAbilities::AddOwnedTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTagContainer& InTags);
	friend void ArcAbilities::RemoveOwnedTags(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FGameplayTagContainer& InTags);

	FArcTagDelta AddTag(FGameplayTag Tag);
	FArcTagDelta RemoveTag(FGameplayTag Tag);
	void AddTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity);
	void RemoveTags(const FGameplayTagContainer& InTags, TArray<FArcPendingTagEvent>& OutEvents, FMassEntityHandle Entity);
};

template<>
struct TMassFragmentTraits<FArcOwnedTagsFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
