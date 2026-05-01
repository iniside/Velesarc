// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcEffectExecution.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectModifierOutput
{
	GENERATED_BODY()

	FArcAttributeRef Attribute;
	EArcModifierOp Operation = EArcModifierOp::Add;
	float Magnitude = 0.f;
};

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcScopedModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 CaptureIndex = 0;

	UPROPERTY(EditAnywhere)
	EArcModifierOp Operation = EArcModifierOp::Add;

	UPROPERTY(EditAnywhere)
	EArcModifierType Type = EArcModifierType::Simple;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Simple", EditConditionHides))
	FScalableFloat Magnitude;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::SetByCaller", EditConditionHides))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Custom", EditConditionHides,
		BaseStruct="/Script/ArcMassAbilities.ArcMagnitudeCalculation", ExcludeBaseStruct))
	FInstancedStruct CustomMagnitude;

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements TagFilter;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectExecution
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FArcAttributeCaptureDef> Captures;

	UPROPERTY(EditAnywhere, Category = "Scoped Modifiers")
	TArray<FArcScopedModifier> ScopedModifiers;

	virtual ~FArcEffectExecution() = default;

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers
	) {}
};
