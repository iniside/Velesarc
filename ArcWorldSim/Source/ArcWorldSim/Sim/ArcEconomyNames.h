// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

struct FMassEntityManager;

namespace ArcEconomyNames
{
	ARCWORLDSIM_API FString ItemName(const FPrimaryAssetId& Item);
	ARCWORLDSIM_API FString RecipeName(const FPrimaryAssetId& RecipeId);
	ARCWORLDSIM_API FString BuildingName(const FMassEntityManager& Manager, FMassEntityHandle Entity);
}
