// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FArcVisRepresentationFragment;
class UArcEntityVisualizationSubsystem;

namespace ArcMass::Visualization
{
	void RegisterEntityInGrids(
		UArcEntityVisualizationSubsystem& Subsystem,
		FMassEntityHandle Entity,
		const FVector& WorldPos,
		FArcVisRepresentationFragment& Rep,
		bool bHasPhysicsConfig);

	void CollectActivationSignals(
		UArcEntityVisualizationSubsystem& Subsystem,
		const FArcVisRepresentationFragment& Rep,
		FMassEntityHandle Entity,
		bool bHasPhysicsConfig,
		TArray<FMassEntityHandle>& OutMeshActivate,
		TArray<FMassEntityHandle>& OutPhysicsActivate);

	void UnregisterEntityFromGrids(
		UArcEntityVisualizationSubsystem& Subsystem,
		FMassEntityHandle Entity,
		const FArcVisRepresentationFragment& Rep,
		bool bHasPhysicsConfig);
}
