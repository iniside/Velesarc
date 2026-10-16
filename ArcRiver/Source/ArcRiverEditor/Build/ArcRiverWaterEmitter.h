// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Extraction/ArcRiverGeoreference.h"
#include "Templates/Function.h"

class UArcRiverGeneratorComponent;
class UArcTerrainTexturePatchModifier;

namespace ArcRiverWaterEmitter
{
	bool CreateOrUpdateWaterZone(
		UArcRiverGeneratorComponent& Generator,
		const UArcTerrainTexturePatchModifier& Modifier,
		FText& OutError);

	int32 DestroyWaterBodies(UArcRiverGeneratorComponent& Generator);

	int32 DestroyAllWaterBodies(UArcRiverGeneratorComponent& Generator);

	bool SpawnWaterBodies(
		UArcRiverGeneratorComponent& Generator,
		TConstArrayView<ArcRiverGeoreference::FArcRiverPolyline> Polylines,
		TFunctionRef<bool()> ShouldContinue,
		int32& OutSpawnedCount,
		FText& OutError);
}
