// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassTestStatsFragment.generated.h"

USTRUCT()
struct FArcMassTestStatsFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Health = 0;

	UPROPERTY()
	float Speed = 0.f;

	UPROPERTY()
	int32 Armor = 0;
};
