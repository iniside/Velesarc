// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcExtraCapture.h"

struct FArcEvaluateParameters;

#include "ArcExtraCaptureSource_TargetEntity.generated.h"

/** Resolves the given attribute from the effect context's TargetEntity.
 *  Returns false when the entity is invalid, the attribute ref is invalid,
 *  or the attribute cannot be found on the entity. */
USTRUCT(BlueprintType, meta=(DisplayName="Target Entity Attribute"))
struct ARCMASSABILITIES_API FArcExtraCaptureSource_TargetEntity : public FArcExtraCaptureSource
{
	GENERATED_BODY()

	/** The attribute to read from the target entity's attribute fragment. */
	UPROPERTY(EditAnywhere)
	FArcAttributeRef Attribute;

	virtual bool ResolveValue(
		const FArcEffectContext& Context,
		FMassEntityManager& EntityManager,
		const FArcEvaluateParameters& EvalParams,
		float& OutValue) const override;

	virtual EArcCaptureSource GetCaptureSide() const override { return EArcCaptureSource::Target; }
};
