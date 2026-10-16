// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "StructUtils/InstancedStruct.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcEffectContext.generated.h"


class UArcEffectDefinition;
class UArcAbilityDefinition;
struct FMassEntityManager;
struct FArcEffectSpec;
struct FArcCapturedExtra;

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectContext
{
	GENERATED_BODY()

	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle TargetEntity;
	FMassEntityHandle SourceEntity;
	UArcEffectDefinition* EffectDefinition = nullptr;
	int32 StackCount = 0;
	UPROPERTY()
	FInstancedStruct SourceData;

	UPROPERTY()
	FGameplayTagContainer OriginTags;

	UPROPERTY()
	TObjectPtr<UArcAbilityDefinition> SourceAbility = nullptr;

	FArcAbilityHandle SourceAbilityHandle;

	FArcActiveEffectHandle EffectHandle;
};

UENUM()
enum class EArcEffectExecutionTrigger : uint8
{
	Application,
	PeriodicTick
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectExecutionContext
{
	GENERATED_BODY()

	const FArcEffectSpec* OwningSpec = nullptr;
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle TargetEntity;
	FGameplayTagContainer PassedInTags;
	const FInstancedStruct* ExecutionState = nullptr;
	EArcEffectExecutionTrigger Trigger = EArcEffectExecutionTrigger::Application;

	const FArcEffectSpec& GetOwningSpec() const;
	FMassEntityHandle GetSourceEntity() const;
	int32 GetStackCount() const;

	FArcAttribute GetAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Ref) const;

	bool AttemptCalculateCapturedAttributeMagnitude(
		const FArcAttributeCaptureDef& CaptureDef,
		const FArcEvaluateParameters& EvalParams,
		float& OutMagnitude) const;

	/** Convenience wrapper: returns the captured magnitude, or DefaultValue if the capture misses
	 *  (a successful capture overwrites; a miss leaves the value untouched). */
	float CalcCapturedMagnitude(
		const FArcAttributeCaptureDef& CaptureDef,
		const FArcEvaluateParameters& EvalParams,
		float DefaultValue = 0.f) const
	{
		float Value = DefaultValue;
		AttemptCalculateCapturedAttributeMagnitude(CaptureDef, EvalParams, Value);
		return Value;
	}

	/** Looks up the captured extra matching Def by equality (source type + attribute + bSnapshot).
	 *  For snapshot entries returns the pre-resolved value; for live entries re-resolves via the
	 *  source resolver on each call.  Returns true on success (OutValue populated), false on
	 *  miss or resolve failure. */
	bool AttemptResolveExtraCapture(
		const FArcExtraCaptureDef& Def,
		const FArcEvaluateParameters& EvalParams,
		float& OutValue) const;

private:
	/** Core magnitude compute for a resolved captured attribute entry. */
	bool ComputeCapturedMagnitude(const FArcCapturedAttribute& Captured, const FArcEvaluateParameters& EvalParams, float& OutMagnitude) const;

	/** Core extra-capture resolve for a resolved captured extra entry. */
	bool ResolveExtraAt(const FArcCapturedExtra& Entry, const FArcEvaluateParameters& EvalParams, float& OutValue) const;
};
