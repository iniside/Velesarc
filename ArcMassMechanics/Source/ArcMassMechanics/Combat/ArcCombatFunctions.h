// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"

struct FMassEntityManager;
struct FArcEffectExecutionContext;
struct FArcArmorCurve;
class UArcEffectDefinition;

namespace ArcMechCombat
{
	ARCMASSMECHANICS_API FGameplayTag GetOutcomeTag(EArcHitOutcome Outcome);

	ARCMASSMECHANICS_API bool IsLeafDamageType(const FGameplayTag& Tag);

	ARCMASSMECHANICS_API FGameplayTag FindEffectDamageType(const UArcEffectDefinition* Definition);

	ARCMASSMECHANICS_API void PublishHitResolved(FMassEntityManager& EntityManager, FMassEntityHandle Source, FMassEntityHandle Target,
		const FGameplayTagContainer& SourceTags, const FGameplayTagContainer& TargetTags, FArcHitResolvedPayload&& Payload);

	ARCMASSMECHANICS_API FArcEffectApplyResult ApplyEffectWithSetByCaller(FMassEntityManager& EntityManager, FMassEntityHandle Target,
		FMassEntityHandle Source, UArcEffectDefinition* Effect, FGameplayTag SetByCallerTag, float Magnitude);

	ARCMASSMECHANICS_API bool IsPeriodicTick(const FArcEffectExecutionContext& Context);

	ARCMASSMECHANICS_API FString DescribeHit(const FArcEffectExecutionContext& Context);

	ARCMASSMECHANICS_API void AddFlatAmount(float Flat, float Sign, float IfAmount, float& InOutValue);

	ARCMASSMECHANICS_API void ApplyIncomingBonus(const FArcEffectExecutionContext& Context, float Flat, float Percent, float MaxPercent,
		float IfTakenFlat, float IfTakenMultiplier, float& InOutValue);

	ARCMASSMECHANICS_API float Mitigate(const FArcEffectExecutionContext& Context, float Value, const FArcArmorCurve* ArmorCurve, float ArmorK,
		float Armor, float DamageReduction, float Resistance);

	ARCMASSMECHANICS_API void PublishHitEvent(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, bool bCrit,
		const FGameplayTag& Zone, const FGameplayTag& DamageType, float Final);

	ARCMASSMECHANICS_API void PublishDamageEvent(const FArcEffectExecutionContext& Context, const FArcEvaluateParameters& HitParams, bool bCrit,
		const FGameplayTag& Zone, const FGameplayTag& DamageType, float PreMitigation, float Final);
}
