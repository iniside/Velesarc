// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sim/ArcBuildingFragments.h"

class USmartObjectDefinition;

namespace ArcWorldSim
{
ARCWORLDSIM_API int32 CountWorkerSlots(const USmartObjectDefinition& Definition);
ARCWORLDSIM_API void SyncProductionSlots(
	FArcProductionSlotsFragment& Production,
	const FArcProductionConfigFragment& Config,
	const USmartObjectDefinition* Definition);
}
