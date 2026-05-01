// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ScalableFloat.h"
#include "StructUtils/InstancedStruct.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcEffectModifier.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcEffectModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	EArcModifierOp Operation = EArcModifierOp::Add;

	UPROPERTY(EditAnywhere)
	EArcModifierType Type = EArcModifierType::Simple;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Simple", EditConditionHides))
	FScalableFloat Magnitude;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::SetByCaller", EditConditionHides))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Custom", EditConditionHides, BaseStruct="/Script/ArcMassAbilities.ArcMagnitudeCalculation", ExcludeBaseStruct))
	FInstancedStruct CustomMagnitude;

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements TagFilter;

	UPROPERTY(EditAnywhere)
	uint8 Channel = 0;
};
