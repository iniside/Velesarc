// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanGameplayTagCondition.generated.h"

/**
 * Filters plan candidates by checking gameplay tags on the actor via IGameplayTagAssetInterface.
 * The entity's actor must have all RequireTags and none of the IgnoreTags to pass.
 */
USTRUCT(BlueprintType, DisplayName = "Gameplay Tag Filter")
struct FArcSmartObjectPlanGameplayTagCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Entity actor must have ALL of these tags to be considered. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequireTags;

	/** Entity actor must have NONE of these tags to be considered. */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer IgnoreTags;

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
