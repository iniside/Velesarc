// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcDirectModifier.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcDirectModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	EArcModifierOp Operation = EArcModifierOp::Add;

	UPROPERTY(EditAnywhere)
	float Magnitude = 0.f;

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements SourceTagReqs;

	UPROPERTY(EditAnywhere)
	FGameplayTagRequirements TargetTagReqs;
};
