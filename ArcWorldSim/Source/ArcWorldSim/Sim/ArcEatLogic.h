// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemId.h"
#include "Mass/EntityHandle.h"

struct FArcHungerNeedFragment;
struct FMassEntityManager;
namespace ArcWorldSim
{
struct FEatPortion
{
	FArcItemId ItemId;
	int32 StackCount = 0;
	float Nutrition = 0.0f;
};

bool FindEatPortion(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Consumer,
	FEatPortion& OutPortion);

void ApplyEatNutrition(
	FMassEntityManager& EntityManager,
	const FMassEntityHandle User,
	FArcHungerNeedFragment& Hunger,
	float Nutrition,
	double Now);

bool ConsumeOnePortion(
	FMassEntityManager& EntityManager,
	FMassEntityHandle Consumer,
	FArcItemId ItemId,
	int32 CurrentStacks);
}
