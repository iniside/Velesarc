// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"

#include "ArcBodyUtilityConsiderations.generated.h"

USTRUCT(BlueprintType, DisplayName = "Body Continuation Consideration")
struct ARCWORLDSIM_API FArcUtilityConsideration_BodyContinuation : public FArcUtilityConsideration
{
	GENERATED_BODY()

	virtual FArcUtilityConsiderationResult Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
