// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcEffectComp_ChanceToApply.generated.h"

USTRUCT(BlueprintType, meta=(DisplayName="Chance To Apply"))
struct ARCMASSABILITIES_API FArcEffectComp_ChanceToApply : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
	float Chance = 1.f;

	virtual bool OnPreApplication(const FArcEffectContext& Context) const override;
};
