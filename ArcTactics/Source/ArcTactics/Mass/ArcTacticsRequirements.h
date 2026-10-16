// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsNode.h"
#include "Mass/EntityHandle.h"

struct FArcTacticsCompiledData;
struct FMassEntityManager;

namespace ArcTacticsRequirements
{
	ARCTACTICS_API EArcTacticsExecutionRequirement Gather(const FArcTacticsCompiledData& CompiledData);

	ARCTACTICS_API bool FindMissing(
		const FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		EArcTacticsExecutionRequirement Requirements,
		TArray<FName>& OutMissing);
}
