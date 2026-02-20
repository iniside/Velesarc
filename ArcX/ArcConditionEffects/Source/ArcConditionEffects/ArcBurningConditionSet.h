// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcBurningConditionSet.generated.h"

/**
 * 
 */
UCLASS()
class ARCCONDITIONEFFECTS_API UArcBurningConditionSet : public UArcAttributeSet
{
	GENERATED_BODY()
	
protected:
	UPROPERTY()
	FGameplayAttributeData BurningAdd;
	
	UPROPERTY()
	FGameplayAttributeData BurningRemove;
	
public:
	ARC_ATTRIBUTE_ACCESSORS(UArcBurningConditionSet, BurningAdd);
	ARC_ATTRIBUTE_ACCESSORS(UArcBurningConditionSet, BurningRemove);
	
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcBurningConditionSet, BurningAdd);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcBurningConditionSet, BurningRemove);
	
	UArcBurningConditionSet();
	
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;
};
