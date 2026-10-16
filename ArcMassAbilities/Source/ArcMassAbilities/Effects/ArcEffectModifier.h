// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "ArcScalableFloat.h"
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
	FArcScalableFloat Magnitude;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Simple", EditConditionHides))
	EArcRangeResolution RangeResolution = EArcRangeResolution::Roll;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::SetByCaller", EditConditionHides))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere, meta=(EditCondition="Type==EArcModifierType::Custom", EditConditionHides, BaseStruct="/Script/ArcMassAbilities.ArcMagnitudeCalculation", ExcludeBaseStruct))
	FInstancedStruct CustomMagnitude;

	/** Tags the source of an evaluation must meet for this modifier to qualify (e.g. the incoming damage type). */
	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements SourceTagRequirements;

	/** Tags the target of an evaluation must meet for this modifier to qualify (e.g. the holder's state). */
	UPROPERTY(EditAnywhere)
	FArcGameplayTagRequirements TargetTagRequirements;

	UPROPERTY(EditAnywhere)
	uint8 Channel = 0;
};
