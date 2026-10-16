// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcEffectComp_Inhibition.generated.h"

USTRUCT(BlueprintType, meta=(DisplayName="Inhibition", Tooltip="Suppresses this effect's ongoing influence while the target entity owns any of InhibitedByTags, without removing the effect."))
struct ARCMASSABILITIES_API FArcEffectComp_Inhibition : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer InhibitedByTags;
};
