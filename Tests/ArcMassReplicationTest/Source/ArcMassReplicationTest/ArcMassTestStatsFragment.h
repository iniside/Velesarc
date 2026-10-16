// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMassTestStatsFragment.generated.h"

USTRUCT(meta = (Hidden))
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
