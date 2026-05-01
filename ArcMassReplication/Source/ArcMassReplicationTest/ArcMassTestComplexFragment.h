// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMassTestComplexFragment.generated.h"

USTRUCT()
struct FArcMassTestComplexFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Health = 0;

	UPROPERTY()
	int32 EntityId = 0;

	UPROPERTY()
	float Damage = 0.f;

	UPROPERTY()
	int32 Armor = 0;
};
