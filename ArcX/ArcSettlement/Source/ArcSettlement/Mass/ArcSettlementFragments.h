// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcSettlementTypes.h"

#include "ArcSettlementFragments.generated.h"

/**
 * Per-entity mutable fragment: which settlement this NPC belongs to and its role.
 */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcSettlementMemberFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	FArcSettlementHandle SettlementHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settlement")
	FGameplayTag Role;

	/** Knowledge handles registered by the observer for this entity's capabilities. */
	TArray<FArcKnowledgeHandle> RegisteredKnowledgeHandles;
};

template<>
struct TMassFragmentTraits<FArcSettlementMemberFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Const shared fragment: immutable per-archetype config describing NPC capabilities.
 * Shared across all entities of the same archetype.
 */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcSettlementMemberConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** What this NPC type can do (e.g., "Capability.Smith", "Capability.Gather", "Capability.Carry"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	FGameplayTagContainer CapabilityTags;
};

/**
 * Tag marking entities that belong to a settlement.
 * Used by observer processors for add/remove registration.
 */
USTRUCT()
struct ARCSETTLEMENT_API FArcSettlementMemberTag : public FMassTag
{
	GENERATED_BODY()
};
