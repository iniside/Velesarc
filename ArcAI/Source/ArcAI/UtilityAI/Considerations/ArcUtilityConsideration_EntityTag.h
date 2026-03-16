// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_EntityTag.generated.h"

/**
 * Checks the target Mass entity's FArcMassGameplayTagContainerFragment
 * against a gameplay tag query.
 * Returns 1.0 if the query matches, 0.0 otherwise.
 */
USTRUCT(BlueprintType, DisplayName = "Entity Gameplay Tags Consideration")
struct ARCAI_API FArcUtilityConsideration_EntityGameplayTags : public FArcUtilityConsideration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Consideration")
	FGameplayTagQuery TagQuery;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};

/**
 * Checks whether the target Mass entity has a specific FMassTag in its archetype.
 * Select the tag type via the MassTagType property.
 * Returns 1.0 if the entity has the tag, 0.0 otherwise.
 */
USTRUCT(BlueprintType, DisplayName = "Entity Mass Tag Consideration")
struct ARCAI_API FArcUtilityConsideration_EntityMassTag : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** The FMassTag-derived struct to check for on the target entity. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (MetaStruct = "/Script/MassEntity.MassTag"))
	TObjectPtr<UScriptStruct> MassTagType;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
