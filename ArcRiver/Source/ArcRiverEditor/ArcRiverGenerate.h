// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcRiverGeneratorComponent;

namespace ArcRiverGenerate
{
	bool CreateWaterZone(UArcRiverGeneratorComponent& Generator, FText& OutError);

	bool GenerateRivers(UArcRiverGeneratorComponent& Generator, FText& OutError);

	bool ClearRivers(UArcRiverGeneratorComponent& Generator, FText& OutError);

	bool ClearAllRivers(UArcRiverGeneratorComponent& Generator, FText& OutError);
}
