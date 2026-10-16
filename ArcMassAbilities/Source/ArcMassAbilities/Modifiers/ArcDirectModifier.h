// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcDirectModifier.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcDirectModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, SaveGame)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, SaveGame)
	EArcModifierOp Operation = EArcModifierOp::Add;

	UPROPERTY(EditAnywhere, SaveGame)
	float Magnitude = 0.f;

	UPROPERTY(EditAnywhere, SaveGame)
	FArcGameplayTagRequirements SourceTagReqs;

	UPROPERTY(EditAnywhere, SaveGame)
	FArcGameplayTagRequirements TargetTagReqs;
};
