// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisProcessorUtils.h"

#include "ArcMassEntityVisualization.h"

namespace ArcMass::Visualization
{

void RegisterEntityInGrids(
	UArcEntityVisualizationSubsystem& Subsystem,
	FMassEntityHandle Entity,
	const FVector& WorldPos,
	FArcVisRepresentationFragment& Rep,
	bool bHasPhysicsConfig)
{
	const FArcVisualizationGrid& MeshGrid = Subsystem.GetMeshGrid();
	Rep.MeshGridCoords = MeshGrid.WorldToCell(WorldPos);
	Subsystem.RegisterMeshEntity(Entity, WorldPos);

	if (bHasPhysicsConfig)
	{
		const FArcVisualizationGrid& PhysicsGrid = Subsystem.GetPhysicsGrid();
		Rep.PhysicsGridCoords = PhysicsGrid.WorldToCell(WorldPos);
		Subsystem.RegisterPhysicsEntity(Entity, WorldPos);
	}
}

void CollectActivationSignals(
	UArcEntityVisualizationSubsystem& Subsystem,
	const FArcVisRepresentationFragment& Rep,
	FMassEntityHandle Entity,
	bool bHasPhysicsConfig,
	TArray<FMassEntityHandle>& OutMeshActivate,
	TArray<FMassEntityHandle>& OutPhysicsActivate)
{
	if (Subsystem.IsMeshActiveCellCoord(Rep.MeshGridCoords))
	{
		OutMeshActivate.Add(Entity);
	}

	if (bHasPhysicsConfig && Subsystem.IsPhysicsActiveCellCoord(Rep.PhysicsGridCoords))
	{
		OutPhysicsActivate.Add(Entity);
	}
}

void UnregisterEntityFromGrids(
	UArcEntityVisualizationSubsystem& Subsystem,
	FMassEntityHandle Entity,
	const FArcVisRepresentationFragment& Rep,
	bool bHasPhysicsConfig)
{
	Subsystem.UnregisterMeshEntity(Entity, Rep.MeshGridCoords);

	if (bHasPhysicsConfig)
	{
		Subsystem.UnregisterPhysicsEntity(Entity, Rep.PhysicsGridCoords);
	}
}

} // namespace ArcMass::Visualization
