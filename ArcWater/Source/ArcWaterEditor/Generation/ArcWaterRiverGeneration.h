// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcWaterRiverModifier;
struct FArcWaterRiverGeneratedData;

namespace ArcWaterRiverGeneration
{
	bool Compute(UArcWaterRiverModifier& Modifier, FArcWaterRiverGeneratedData& OutGeneratedData,
		FTransform& OutGeneratedFrame, FText& OutError);
	bool Rebuild(UArcWaterRiverModifier& Modifier, FText& OutError);
}
