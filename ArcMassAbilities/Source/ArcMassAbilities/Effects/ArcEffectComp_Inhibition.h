// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcEffectComp_Inhibition.generated.h"

USTRUCT(BlueprintType, meta=(DisplayName="Inhibition"))
struct ARCMASSABILITIES_API FArcEffectComp_Inhibition : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer InhibitedByTags;
};
