// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "ArcEffectComp_GrantImmunity.generated.h"

USTRUCT(BlueprintType, meta=(DisplayName="Grant Immunity"))
struct ARCMASSABILITIES_API FArcEffectComp_GrantImmunity : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer GrantedImmunityTags;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
	virtual void OnRemoved(const FArcEffectContext& Context) const override;
};
