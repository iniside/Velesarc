// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExtension.h"
#include "ArcConditionTypes.h"
#include "AbilitySystem/ArcAttributeSet.h"
#include "AbilitySystem/ArcAttributesTypes.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "ArcConditionAttributeSet.generated.h"

// Non-UHT boilerplate: accessors + capture defs + handler declarations.
// UPROPERTY() declarations must remain explicit above this macro.
#define ARC_CONDITION_ATTRIBUTE_HELPERS(Name) \
	ARC_ATTRIBUTE_ACCESSORS(UArcConditionAttributeSet, Name##Add) \
	ARC_ATTRIBUTE_ACCESSORS(UArcConditionAttributeSet, Name##Remove) \
	ARC_ATTRIBUTE_ACCESSORS(UArcConditionAttributeSet, Name##Saturation) \
	ARC_ATTRIBUTE_CAPTURE_SOURCE(UArcConditionAttributeSet, Name##Add) \
	ARC_DEFINE_ATTRIBUTE_HANDLER(Name##Add) \
	ARC_DEFINE_ATTRIBUTE_HANDLER(Name##Remove)

/**
 * Unified GAS attribute set for the condition system.
 *
 * Each of the 13 conditions has 3 attributes:
 *   {Name}Add        — meta-attribute; GE writes here, handler reads/zeroes/forwards to Mass as positive amount
 *   {Name}Remove     — meta-attribute; same but forwards as negative amount
 *   {Name}Saturation — read-only mirror of Mass fragment saturation (written by sync processor)
 */
UCLASS()
class ARCCONDITIONEFFECTS_API UArcConditionAttributeSet : public UArcAttributeSet
{
	GENERATED_BODY()

public:
	UArcConditionAttributeSet();

	// ---- Group A: Hysteresis ------------------------------------------------

	// Burning
	UPROPERTY()
	FGameplayAttributeData BurningAdd;
	UPROPERTY()
	FGameplayAttributeData BurningRemove;
	UPROPERTY()
	FGameplayAttributeData BurningSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Burning)

	// Bleeding
	UPROPERTY()
	FGameplayAttributeData BleedingAdd;
	UPROPERTY()
	FGameplayAttributeData BleedingRemove;
	UPROPERTY()
	FGameplayAttributeData BleedingSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Bleeding)

	// Chilled
	UPROPERTY()
	FGameplayAttributeData ChilledAdd;
	UPROPERTY()
	FGameplayAttributeData ChilledRemove;
	UPROPERTY()
	FGameplayAttributeData ChilledSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Chilled)

	// Shocked
	UPROPERTY()
	FGameplayAttributeData ShockedAdd;
	UPROPERTY()
	FGameplayAttributeData ShockedRemove;
	UPROPERTY()
	FGameplayAttributeData ShockedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Shocked)

	// Poisoned
	UPROPERTY()
	FGameplayAttributeData PoisonedAdd;
	UPROPERTY()
	FGameplayAttributeData PoisonedRemove;
	UPROPERTY()
	FGameplayAttributeData PoisonedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Poisoned)

	// Diseased
	UPROPERTY()
	FGameplayAttributeData DiseasedAdd;
	UPROPERTY()
	FGameplayAttributeData DiseasedRemove;
	UPROPERTY()
	FGameplayAttributeData DiseasedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Diseased)

	// Weakened
	UPROPERTY()
	FGameplayAttributeData WeakenedAdd;
	UPROPERTY()
	FGameplayAttributeData WeakenedRemove;
	UPROPERTY()
	FGameplayAttributeData WeakenedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Weakened)

	// ---- Group B: Linear Catalysts ------------------------------------------

	// Oiled
	UPROPERTY()
	FGameplayAttributeData OiledAdd;
	UPROPERTY()
	FGameplayAttributeData OiledRemove;
	UPROPERTY()
	FGameplayAttributeData OiledSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Oiled)

	// Wet
	UPROPERTY()
	FGameplayAttributeData WetAdd;
	UPROPERTY()
	FGameplayAttributeData WetRemove;
	UPROPERTY()
	FGameplayAttributeData WetSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Wet)

	// Corroded
	UPROPERTY()
	FGameplayAttributeData CorrodedAdd;
	UPROPERTY()
	FGameplayAttributeData CorrodedRemove;
	UPROPERTY()
	FGameplayAttributeData CorrodedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Corroded)

	// ---- Group C: Binary / Environmental ------------------------------------

	// Blinded
	UPROPERTY()
	FGameplayAttributeData BlindedAdd;
	UPROPERTY()
	FGameplayAttributeData BlindedRemove;
	UPROPERTY()
	FGameplayAttributeData BlindedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Blinded)

	// Suffocating
	UPROPERTY()
	FGameplayAttributeData SuffocatingAdd;
	UPROPERTY()
	FGameplayAttributeData SuffocatingRemove;
	UPROPERTY()
	FGameplayAttributeData SuffocatingSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Suffocating)

	// Exhausted
	UPROPERTY()
	FGameplayAttributeData ExhaustedAdd;
	UPROPERTY()
	FGameplayAttributeData ExhaustedRemove;
	UPROPERTY()
	FGameplayAttributeData ExhaustedSaturation;
	ARC_CONDITION_ATTRIBUTE_HELPERS(Exhausted)

	static FGameplayAttribute GetSaturationAttributeByIndex(int32 Index);

private:
	void ForwardToMass(EArcConditionType ConditionType, float Amount);
};

#undef ARC_CONDITION_ATTRIBUTE_HELPERS
