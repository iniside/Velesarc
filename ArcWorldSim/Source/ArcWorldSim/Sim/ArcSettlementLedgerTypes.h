// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

struct ARCWORLDSIM_API FArcNeedRow
{
	FMassEntityHandle Building;
	TArray<FPrimaryAssetId> AcceptableItems;
	int32 Quantity = 0;
};

struct ARCWORLDSIM_API FArcHaveRow
{
	FMassEntityHandle Place;
	FPrimaryAssetId Item;
	int32 Quantity = 0;
};
