// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

namespace ArcTownGridKeys
{
	/**
	 * Identity of one town's partition-actor family on one grid. UActorPartitionSubsystem caches on
	 * (cell, class + grid guid) with no grid size in the key, so the size and the runtime grid have to
	 * fold in here or two families alias onto one actor.
	 */
	ARCTOWNEDITOR_API FGuid MakeTownGridGuid(const FGuid& TownId, uint32 GridSize, FName RuntimeGridName);
}
