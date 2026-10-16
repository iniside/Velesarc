// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "ArcAttributeEventPayloads.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAttributeChangedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef Attribute;

	UPROPERTY()
	float OldValue = 0.f;

	UPROPERTY()
	float NewValue = 0.f;
};
