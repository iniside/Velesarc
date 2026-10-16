// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
class UArcBuildingDefinition;
class UArcMassEntityConfigAsset;
struct FActorSpawnParameters;

namespace ArcBuilderGhostPreview
{
	/**
	 * Spawn the placement ghost for Def: ActorClass / engine vis trait class if resolvable,
	 * else a plain AActor assembled from the config's Arc visualization traits (a union of
	 * components across every supported trait the config carries).
	 * Returns nullptr (and logs a warning) when Def offers no usable preview source.
	 */
	AActor* SpawnGhostActor(UWorld& World, const UArcBuildingDefinition& Def,
		const FActorSpawnParameters& SpawnParams);

	/**
	 * True when Config carries at least one ghost-buildable visualization source
	 * (engine UMassVisualizationTrait with HighResTemplateActor, or an Arc vis trait
	 * whose IsValid() passes). Shared by the spawn path and IsDataValid.
	 */
	bool HasSupportedVisualization(const UArcMassEntityConfigAsset& Config);
}
