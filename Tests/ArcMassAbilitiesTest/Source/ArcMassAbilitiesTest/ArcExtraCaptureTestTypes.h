// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcExtraCapture.h"

#include "ArcExtraCaptureTestTypes.generated.h"

struct FArcEvaluateParameters;

/** Test-only concrete subclass of FArcExtraCaptureSource.
 *  Returns a caller-set constant; optionally forced to fail.
 *  The Attribute parameter is intentionally ignored — the constant resolver
 *  does not need to know which attribute is being captured. */
USTRUCT(meta = (Hidden))
struct FArcTestExtraCaptureSource_Constant : public FArcExtraCaptureSource
{
	GENERATED_BODY()

	float ConstantValue = 0.f;
	bool bResolveSucceeds = true;

	/** Number of times ResolveValue has been called — used to verify live re-resolve. */
	mutable int32 ResolveCount = 0;

	virtual bool ResolveValue(
		const FArcEffectContext& /*Context*/,
		FMassEntityManager& /*EntityManager*/,
		const FArcEvaluateParameters& /*EvalParams*/,
		float& OutValue) const override
	{
		ResolveCount++;
		if (!bResolveSucceeds)
		{
			return false;
		}
		OutValue = ConstantValue;
		return true;
	}
};
