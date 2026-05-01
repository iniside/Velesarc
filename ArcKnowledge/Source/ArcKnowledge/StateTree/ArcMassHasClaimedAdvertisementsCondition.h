// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ArcMassHasClaimedAdvertisementsCondition.generated.h"

/**
 * StateTree condition that checks whether the executing Mass entity
 * has any currently claimed advertisements.
 */
USTRUCT(DisplayName = "Arc Has Claimed Advertisements")
struct ARCKNOWLEDGE_API FArcMassHasClaimedAdvertisementsCondition : public FMassStateTreeConditionBase
{
	GENERATED_BODY()

public:
	FArcMassHasClaimedAdvertisementsCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

	UPROPERTY(EditAnywhere, Category = Condition)
	bool bInvert = false;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
