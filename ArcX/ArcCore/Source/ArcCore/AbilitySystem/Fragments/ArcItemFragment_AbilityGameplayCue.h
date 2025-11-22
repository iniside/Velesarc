// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_AbilityGameplayCue.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_AbilityGameplayCue : public FArcItemFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;
};
