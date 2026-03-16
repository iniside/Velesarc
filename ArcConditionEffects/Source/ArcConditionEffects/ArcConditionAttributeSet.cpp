// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionAttributeSet.h"

#include "Engine/World.h"

#include "ArcConditionEffectsSubsystem.h"
#include "AbilitySystem/ArcGameplayAbilityActorInfo.h"

// ---------------------------------------------------------------------------
// Constructor — register attribute handlers
// ---------------------------------------------------------------------------

#define ARC_REGISTER_CONDITION_HANDLERS(Name) \
	ARC_REGISTER_ATTRIBUTE_HANDLER(Name##Add) \
	ARC_REGISTER_ATTRIBUTE_HANDLER(Name##Remove)

UArcConditionAttributeSet::UArcConditionAttributeSet()
{
	// Group A: Hysteresis
	ARC_REGISTER_CONDITION_HANDLERS(Burning)
	ARC_REGISTER_CONDITION_HANDLERS(Bleeding)
	ARC_REGISTER_CONDITION_HANDLERS(Chilled)
	ARC_REGISTER_CONDITION_HANDLERS(Shocked)
	ARC_REGISTER_CONDITION_HANDLERS(Poisoned)
	ARC_REGISTER_CONDITION_HANDLERS(Diseased)
	ARC_REGISTER_CONDITION_HANDLERS(Weakened)

	// Group B: Linear Catalysts
	ARC_REGISTER_CONDITION_HANDLERS(Oiled)
	ARC_REGISTER_CONDITION_HANDLERS(Wet)
	ARC_REGISTER_CONDITION_HANDLERS(Corroded)

	// Group C: Binary / Environmental
	ARC_REGISTER_CONDITION_HANDLERS(Blinded)
	ARC_REGISTER_CONDITION_HANDLERS(Suffocating)
	ARC_REGISTER_CONDITION_HANDLERS(Exhausted)
}

#undef ARC_REGISTER_CONDITION_HANDLERS

// ---------------------------------------------------------------------------
// Attribute Handlers — Add (positive) and Remove (negative) for each condition
// ---------------------------------------------------------------------------

#define ARC_CONDITION_ADD_HANDLER(Name, EnumVal) \
ARC_DECLARE_ATTRIBUTE_HANDLER(UArcConditionAttributeSet, Name##Add) \
{ \
	return [&](const struct FGameplayEffectModCallbackData& Data) \
	{ \
		const float Value = Get##Name##Add(); \
		Set##Name##Add(0); \
		ForwardToMass(EArcConditionType::EnumVal, Value); \
	}; \
}

#define ARC_CONDITION_REMOVE_HANDLER(Name, EnumVal) \
ARC_DECLARE_ATTRIBUTE_HANDLER(UArcConditionAttributeSet, Name##Remove) \
{ \
	return [&](const struct FGameplayEffectModCallbackData& Data) \
	{ \
		const float Value = Get##Name##Remove(); \
		Set##Name##Remove(0); \
		ForwardToMass(EArcConditionType::EnumVal, -Value); \
	}; \
}

// Group A: Hysteresis
ARC_CONDITION_ADD_HANDLER(Burning, Burning)
ARC_CONDITION_REMOVE_HANDLER(Burning, Burning)
ARC_CONDITION_ADD_HANDLER(Bleeding, Bleeding)
ARC_CONDITION_REMOVE_HANDLER(Bleeding, Bleeding)
ARC_CONDITION_ADD_HANDLER(Chilled, Chilled)
ARC_CONDITION_REMOVE_HANDLER(Chilled, Chilled)
ARC_CONDITION_ADD_HANDLER(Shocked, Shocked)
ARC_CONDITION_REMOVE_HANDLER(Shocked, Shocked)
ARC_CONDITION_ADD_HANDLER(Poisoned, Poisoned)
ARC_CONDITION_REMOVE_HANDLER(Poisoned, Poisoned)
ARC_CONDITION_ADD_HANDLER(Diseased, Diseased)
ARC_CONDITION_REMOVE_HANDLER(Diseased, Diseased)
ARC_CONDITION_ADD_HANDLER(Weakened, Weakened)
ARC_CONDITION_REMOVE_HANDLER(Weakened, Weakened)

// Group B: Linear Catalysts
ARC_CONDITION_ADD_HANDLER(Oiled, Oiled)
ARC_CONDITION_REMOVE_HANDLER(Oiled, Oiled)
ARC_CONDITION_ADD_HANDLER(Wet, Wet)
ARC_CONDITION_REMOVE_HANDLER(Wet, Wet)
ARC_CONDITION_ADD_HANDLER(Corroded, Corroded)
ARC_CONDITION_REMOVE_HANDLER(Corroded, Corroded)

// Group C: Binary / Environmental
ARC_CONDITION_ADD_HANDLER(Blinded, Blinded)
ARC_CONDITION_REMOVE_HANDLER(Blinded, Blinded)
ARC_CONDITION_ADD_HANDLER(Suffocating, Suffocating)
ARC_CONDITION_REMOVE_HANDLER(Suffocating, Suffocating)
ARC_CONDITION_ADD_HANDLER(Exhausted, Exhausted)
ARC_CONDITION_REMOVE_HANDLER(Exhausted, Exhausted)

#undef ARC_CONDITION_ADD_HANDLER
#undef ARC_CONDITION_REMOVE_HANDLER

// ---------------------------------------------------------------------------
// ForwardToMass — shared logic for all handlers
// ---------------------------------------------------------------------------

void UArcConditionAttributeSet::ForwardToMass(EArcConditionType ConditionType, float Amount)
{
	if (FMath::IsNearlyZero(Amount))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const FArcGameplayAbilityActorInfo* ActorInfo =
		static_cast<const FArcGameplayAbilityActorInfo*>(ASC->AbilityActorInfo.Get());
	if (!ActorInfo)
	{
		return;
	}

	const FMassEntityHandle EntityHandle = ActorInfo->EntityHandle;
	if (!EntityHandle.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UArcConditionEffectsSubsystem* Subsystem = World->GetSubsystem<UArcConditionEffectsSubsystem>();
	if (Subsystem)
	{
		Subsystem->ApplyCondition(EntityHandle, ConditionType, Amount);
	}
}
