// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_TargetingVisualization.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_TargetingVisualization : public FArcItemFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag GlobalTargetingTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client"))
	TSoftClassPtr<AActor> TargetingVisualizationActorClass;
};
