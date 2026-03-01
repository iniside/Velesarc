// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeTypes.h"
#include "ArcKnowledgeEntryDefinition.h"

#include "ArcKnowledgeFragments.generated.h"

/**
 * Per-entity mutable fragment: knowledge system membership and role.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeMemberFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knowledge")
	FGameplayTag Role;

	/** Knowledge handles registered by the observer for this entity's capabilities. */
	TArray<FArcKnowledgeHandle> RegisteredKnowledgeHandles;
};

template<>
struct TMassFragmentTraits<FArcKnowledgeMemberFragment> final
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
struct ARCKNOWLEDGE_API FArcKnowledgeMemberConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Optional data asset defining predefined knowledge entries to register on spawn.
	  * When set, the definition's entries are registered via RegisterFromDefinition(). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TObjectPtr<UArcKnowledgeEntryDefinition> Definition;

	/** What this NPC type can do (e.g., "Capability.Smith", "Capability.Gather", "Capability.Carry").
	  * Each tag is registered as a separate knowledge entry at the entity's location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	FGameplayTagContainer CapabilityTags;
};

/**
 * Tag marking entities that are knowledge system members.
 * Used by observer processors for add/remove registration.
 */
USTRUCT()
struct ARCKNOWLEDGE_API FArcKnowledgeMemberTag : public FMassTag
{
	GENERATED_BODY()
};
