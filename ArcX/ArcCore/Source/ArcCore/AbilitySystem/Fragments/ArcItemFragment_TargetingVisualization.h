// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_TargetingVisualization.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Targeting Visualization", Category = "Gameplay Ability", ToolTip = "Defines the client-side targeting visualization for this item's ability. References an actor class spawned to preview the targeting area (e.g., a ground decal or holographic indicator) and a global targeting tag for system identification."))
struct ARCCORE_API FArcItemFragment_TargetingVisualization : public FArcItemFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag GlobalTargetingTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client"))
	TSoftClassPtr<AActor> TargetingVisualizationActorClass;
};
