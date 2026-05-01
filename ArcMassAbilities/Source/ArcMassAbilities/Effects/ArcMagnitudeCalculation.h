// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcAttributeCapture.h"
#include "ArcMagnitudeCalculation.generated.h"

struct FArcEffectContext;

USTRUCT()
struct ARCMASSABILITIES_API FArcMagnitudeCalculation
{
	GENERATED_BODY()

	virtual ~FArcMagnitudeCalculation() = default;

	virtual float Calculate(const FArcEffectContext& Context) const PURE_VIRTUAL(FArcMagnitudeCalculation::Calculate, return 0.f;);
};

USTRUCT(BlueprintType, meta=(DisplayName="Attribute Based"))
struct ARCMASSABILITIES_API FArcMagnitude_AttributeBased : public FArcMagnitudeCalculation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef SourceAttribute;

	UPROPERTY(EditAnywhere)
	EArcCaptureSource CaptureFrom = EArcCaptureSource::Source;

	virtual float Calculate(const FArcEffectContext& Context) const override;
};
