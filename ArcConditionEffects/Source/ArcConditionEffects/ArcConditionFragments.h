// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"

#include "ArcConditionFragments.generated.h"

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcConditionStatesFragment : public FMassFragment
{
	GENERATED_BODY()
	FArcConditionState States[ArcConditionTypeCount];
};

USTRUCT()
struct ARCCONDITIONEFFECTS_API FArcConditionConfigsShared : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FArcConditionConfig Configs[ArcConditionTypeCount];

	FArcConditionConfigsShared()
	{
		InitDefaultConditionConfigs(Configs);
	}
};
