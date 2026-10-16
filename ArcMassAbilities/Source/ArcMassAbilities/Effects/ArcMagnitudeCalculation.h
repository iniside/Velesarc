// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcAttributeCapture.h"
#include "Attributes/ArcAggregator.h"
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

USTRUCT(meta=(Hidden))
struct ARCMASSABILITIES_API FArcLiveMagnitude : public FArcMagnitudeCalculation
{
	GENERATED_BODY()

	virtual float Calculate(const FArcEffectContext& Context) const override final;

	virtual void GetSourceAttributes(TArray<FArcAttributeRef>& OutAttributes) const {}

	virtual float EvaluateLive(const FArcAggregationContext& Context) const PURE_VIRTUAL(FArcLiveMagnitude::EvaluateLive, return 0.f;);
};

USTRUCT(BlueprintType, meta=(DisplayName="Live Attribute Linear"))
struct ARCMASSABILITIES_API FArcLiveMagnitude_AttributeLinear : public FArcLiveMagnitude
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere)
	float Coefficient = 1.f;

	UPROPERTY(EditAnywhere)
	float PreAdd = 0.f;

	UPROPERTY(EditAnywhere)
	float PostAdd = 0.f;

	virtual void GetSourceAttributes(TArray<FArcAttributeRef>& OutAttributes) const override;

	virtual float EvaluateLive(const FArcAggregationContext& Context) const override;
};
