// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Items/Fragments/ArcItemFragment.h"
#include "ArcItemFragment_AbilityGameplayCue.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ability Gameplay Cue", Category = "Gameplay Ability", ToolTip = "Associates a GameplayCue tag with this item. When the item's ability triggers the cue, the system plays the corresponding visual/audio feedback. Use for hit impacts, activation flashes, or status effect visuals."))
struct ARCCORE_API FArcItemFragment_AbilityGameplayCue : public FArcItemFragment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;
};
