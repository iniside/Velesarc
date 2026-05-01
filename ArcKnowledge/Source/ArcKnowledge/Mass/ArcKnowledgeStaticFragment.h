// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "ArcKnowledgeEntryDefinition.h"

#include "ArcKnowledgeStaticFragment.generated.h"

/**
 * Const shared fragment: immutable per-archetype config for static knowledge entities.
 * Tags define what this entity advertises to the knowledge R-tree.
 * Shared across all entities of the same archetype.
 */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeStaticConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	FGameplayTagContainer KnowledgeTags;

	/** Optional data asset defining payload, instruction, and initial knowledge for this entity type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Knowledge")
	TObjectPtr<UArcKnowledgeEntryDefinition> Definition;
};

/**
 * Tag marking entities that are static knowledge sources.
 * Used by observer processors for R-tree insert/remove.
 */
USTRUCT()
struct ARCKNOWLEDGE_API FArcKnowledgeStaticTag : public FMassTag
{
	GENERATED_BODY()
};
