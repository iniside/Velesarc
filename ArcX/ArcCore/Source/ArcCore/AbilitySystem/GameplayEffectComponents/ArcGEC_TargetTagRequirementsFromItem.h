// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "GameplayEffectTypes.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "UObject/Object.h"
#include "ArcGEC_TargetTagRequirementsFromItem.generated.h"

USTRUCT()
struct FArcItemFragment_GameplayEffectTargetTagRequirements : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** Tag requirements the target must have for this GameplayEffect to be applied. This is pass/fail at the time of application. If fail, this GE fails to apply. */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FGameplayTagRequirements ApplicationTagRequirements;

	/** Once Applied, these tags requirements are used to determined if the GameplayEffect is "on" or "off". A GameplayEffect can be off and do nothing, but still applied. */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FGameplayTagRequirements OngoingTagRequirements;

	/** Tag requirements that if met will remove this effect. Also prevents effect application. */
	UPROPERTY(EditDefaultsOnly, Category = Tags)
	FGameplayTagRequirements RemovalTagRequirements;
};

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcGEC_TargetTagRequirementsFromItem : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual bool CanGameplayEffectApply(const FActiveGameplayEffectsContainer& ActiveGEContainer, const FGameplayEffectSpec& GESpec) const override;
};