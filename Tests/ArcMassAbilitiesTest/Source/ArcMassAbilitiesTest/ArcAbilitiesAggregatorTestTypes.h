// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectExecution.h"
#include "ArcAbilitiesAggregatorTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcTestGatherCapturesExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef ExtraAttribute;

	UPROPERTY()
	EArcCaptureSource ExtraSource = EArcCaptureSource::Source;

	UPROPERTY()
	bool bExtraSnapshot = false;

	UPROPERTY()
	FArcAttributeRef OutputAttribute;

	FArcAttributeCaptureDef GetExtraDef() const
	{
		return Arc::Capture::MakeRuntimeCaptureDef(ExtraAttribute, ExtraSource, bExtraSnapshot);
	}

	virtual void GatherCaptures(
		TArray<FArcAttributeCaptureDef>& OutCaptures,
		TArray<FArcExtraCaptureDef>& OutExtras) const override
	{
		Super::GatherCaptures(OutCaptures, OutExtras);
		OutCaptures.Add(GetExtraDef());
	}

	virtual void Execute(
		const FArcEffectExecutionContext& Context,
		TArray<FArcEffectModifierOutput>& OutModifiers) const override
	{
		const FArcEvaluateParameters EvalParams;

		FArcEffectModifierOutput Output;
		Output.Attribute = OutputAttribute;
		Output.Operation = EArcModifierOp::Add;
		Output.Magnitude = Context.CalcCapturedMagnitude(GetExtraDef(), EvalParams);
		OutModifiers.Add(Output);
	}
};

USTRUCT(meta = (Hidden))
struct FArcTestReplacingGatherExecution : public FArcEffectExecution
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef GatheredAttribute;

	virtual void GatherCaptures(
		TArray<FArcAttributeCaptureDef>& OutCaptures,
		TArray<FArcExtraCaptureDef>& OutExtras) const override
	{
		OutCaptures.Add(Arc::Capture::MakeRuntimeCaptureDef(GatheredAttribute, EArcCaptureSource::Source, true));
	}
};
