/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "ArcGunAttributeSet.generated.h"

/**
 * Unless specified otherwise these attributes act as multiplier bonuses where 1.0 is default value (no bonus)
 * And modifying effects can either add or subtract from this value to make percentage bonus which is
 * calculated as (Attribute * 100) - 100 (to get display percentage value) and in other calculation it is simply multiplied.
 */
UCLASS()
class ARCGUN_API UArcGunAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()
public:
	/*
	 * Percentage Bonus to WeaponDamage
	 * Not this attribute but the one contained on item, which is raw float value.
	 */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_WeaponDamage, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData WeaponDamage;

	/* Percentage Bonus to WeaponDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData HeadshotDamage;

	/* Percentage Bonus to WeaponDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData WeakpointDamage;

	/* Percentage Bonus to WeaponDamage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData CriticalDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData CriticalHitChance;

	/* Percentage Increase In Rounds Per Minute for weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData FireRate;
	
	/* Percentage Reduction in reload speed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData ReloadSpeedBonus;

	/*Bonus value to magazine size on item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxMagazine;

	/* Improved reload time, stability, fire rate, accuracy and weapon swap time. */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Handling, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Handling;

	/* Percentage reduction in weapon spread. */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Accuracy, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Accuracy;

	/* Percentage reduction to weapon recoil. */
	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_Stability, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Stability;
		
	/* Percentage increase of weapon range (which will move damage falloff). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Range;
	
	UFUNCTION()
    void OnRep_WeaponDamage(const FGameplayAttributeData& Old);
	UFUNCTION()
    void OnRep_Accuracy(const FGameplayAttributeData& Old);
	UFUNCTION()
    void OnRep_Stability(const FGameplayAttributeData& Old);
	UFUNCTION()
	void OnRep_Handling(const FGameplayAttributeData& Old);


	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, WeaponDamage);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, HeadshotDamage);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, WeakpointDamage);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, CriticalDamage);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, CriticalHitChance);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, FireRate);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, ReloadSpeedBonus);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, MaxMagazine);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, Handling);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, Accuracy);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, Stability);
	ARC_ATTRIBUTE_ACCESSORS(UArcGunAttributeSet, Range);

	static FGameplayEffectAttributeCaptureSpec GetFireRateCaptureSpecSource(UAbilitySystemComponent* InASC)
	{
		static FGameplayEffectAttributeCaptureDefinition CaptureDef(UArcGunAttributeSet::GetFireRateAttribute()
			, EGameplayEffectAttributeCaptureSource::Source, false);
		static FGameplayEffectAttributeCaptureSpec CaptureSpec(CaptureDef);
		InASC->CaptureAttributeForGameplayEffect(CaptureSpec);
		return CaptureSpec;
	}
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, WeaponDamage);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, HeadshotDamage);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, ReloadSpeedBonus);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, MaxMagazine);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, Handling);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, Accuracy);
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcGunAttributeSet, Stability);

	UArcGunAttributeSet();
};