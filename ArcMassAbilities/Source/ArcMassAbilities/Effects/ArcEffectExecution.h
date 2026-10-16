// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "ArcScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "Effects/ArcExtraCapture.h"
#include "ArcEffectExecution.generated.h"

UENUM()
enum class EArcEffectOutputSide : uint8
{
	Target,
	Source
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectModifierOutput
{
	GENERATED_BODY()

	FArcAttributeRef Attribute;
	EArcModifierOp Operation = EArcModifierOp::Add;
	float Magnitude = 0.f;
	EArcEffectOutputSide Side = EArcEffectOutputSide::Target;
	FGameplayTagContainer SourceTags;
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcScopedModifier
{
	GENERATED_BODY()

	/** Capture this modifier applies to, by identity (attribute + source + snapshot). Replaces the old
	 *  positional CaptureIndex; resolution matches this against the gathered captures (GatherCaptures). */
	UPROPERTY(EditAnywhere)
	FArcAttributeCaptureDef CaptureDef;

	/** DEPRECATED — migrated to CaptureDef in UArcEffectDefinition::PostLoad. Kept serialized only so
	 *  pre-2026-05-29 assets can migrate. Remove in a follow-up after ED_Test/ED_TestEffect re-saved. */
	UPROPERTY()
	int32 CaptureIndex = 0;

	UPROPERTY(EditAnywhere)
	EArcModifierOp Operation = EArcModifierOp::Add;

	UPROPERTY(EditAnywhere)
	EArcModifierType Type = EArcModifierType::Simple;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Simple", EditConditionHides))
	FArcScalableFloat Magnitude;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Simple", EditConditionHides))
	EArcRangeResolution RangeResolution = EArcRangeResolution::Roll;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::SetByCaller", EditConditionHides))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Custom", EditConditionHides,
		BaseStruct="/Script/ArcMassAbilities.ArcMagnitudeCalculation", ExcludeBaseStruct))
	FInstancedStruct CustomMagnitude;

	/** Tags the source of an evaluation must meet for this scoped modifier to apply (e.g. the incoming damage type). */
	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements SourceTagRequirements;

	/** Tags the target of an evaluation must meet for this scoped modifier to apply (e.g. the holder's state). */
	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements TargetTagRequirements;
};

/** Base for custom effect execution logic; subclasses declare attribute captures and scoped modifiers, then compute output magnitudes in Execute. */
USTRUCT(meta=(ShowTooltip))
struct ARCMASSABILITIES_API FArcEffectExecution
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FArcAttributeCaptureDef> Captures;

	UPROPERTY(EditAnywhere, Category = "Scoped Modifiers")
	TArray<FArcScopedModifier> ScopedModifiers;

	UPROPERTY(EditAnywhere)
	TArray<FArcExtraCaptureDef> ExtraCaptures;

	virtual ~FArcEffectExecution() = default;

	virtual void GatherCaptures(
		TArray<FArcAttributeCaptureDef>& OutCaptures,
		TArray<FArcExtraCaptureDef>& OutExtras
	) const
	{
		OutCaptures.Append(Captures);
		OutExtras.Append(ExtraCaptures);
	}

	// Runs once when a Duration or Infinite effect is stored; Execute reads OutState through Context.ExecutionState.
	virtual void CaptureApplicationState(
		const FArcEffectExecutionContext& Context,
		FInstancedStruct& OutState
	) const {}

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) const {}
};
