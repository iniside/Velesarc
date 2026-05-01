// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisEntityDebuggerDrawComponent.h"

void UArcVisEntityDebuggerDrawComponent::UpdateVisData(const FArcVisEntityDebugDrawData& InData)
{
	Data = InData;
	RefreshShapes();
}

void UArcVisEntityDebuggerDrawComponent::CollectShapes()
{
	if (Data.bHasSelectedEntity)
	{
		const FArcVisEntityDebugSelectedEntity& Entity = Data.SelectedEntity;
		StoredSpheres.Emplace(60.f, Entity.Location, Entity.Color);
		StoredLines.Emplace(Entity.Location, Entity.Location + FVector(0, 0, 300.f), Entity.Color, 2.f);
		StoredTexts.Emplace(Entity.Label, Entity.Location + FVector(0, 0, 320.f), FColor::White);
	}

	if (!Data.bShowGrid)
	{
		return;
	}

	for (const FArcVisEntityDebugGridCell& Cell : Data.OccupiedCells)
	{
		FBox CellBox = FBox::BuildAABB(FVector::ZeroVector, Cell.HalfExtent);
		FTransform CellTransform(FQuat::Identity, Cell.Center);
		StoredBoxes.Emplace(CellBox, Cell.Color, CellTransform, FDebugRenderSceneProxy::EDrawType::Invalid, Cell.Thickness);
		if (!Cell.CountLabel.IsEmpty())
		{
			StoredTexts.Emplace(Cell.CountLabel, Cell.Center + FVector(0, 0, Cell.HalfExtent.Z * 0.7f), Cell.Color);
		}
	}

	if (Data.bHasPlayerCell)
	{
		FBox PlayerBox = FBox::BuildAABB(FVector::ZeroVector, Data.PlayerCell.HalfExtent);
		FTransform PlayerTransform(FQuat::Identity, Data.PlayerCell.Center);
		StoredBoxes.Emplace(PlayerBox, FColor::White, PlayerTransform, FDebugRenderSceneProxy::EDrawType::Invalid, 3.f);
	}

	for (const FArcVisEntityDebugGridCell& Cell : Data.BoundaryCells)
	{
		FBox BoundaryBox = FBox::BuildAABB(FVector::ZeroVector, Cell.HalfExtent);
		FTransform BoundaryTransform(FQuat::Identity, Cell.Center);
		StoredBoxes.Emplace(BoundaryBox, Cell.Color, BoundaryTransform, FDebugRenderSceneProxy::EDrawType::Invalid, Cell.Thickness);
	}

	if (Data.bHasSelectedHolder)
	{
		const FArcVisEntityDebugHolderDrawData& Holder = Data.SelectedHolder;

		FBox HolderBox = FBox::BuildAABB(FVector::ZeroVector, Holder.CellHalfExtent);
		FTransform HolderTransform(FQuat::Identity, Holder.CellCenter);
		StoredBoxes.Emplace(HolderBox, FColor::Orange, HolderTransform, FDebugRenderSceneProxy::EDrawType::Invalid, 3.f);

		for (const FArcVisEntityDebugHolderInstance& Inst : Holder.Instances)
		{
			StoredSpheres.Emplace(40.f, Inst.Location, FColor::Orange);
		}

		for (const FVector& EntityLoc : Holder.ReferencingEntityLocations)
		{
			StoredLines.Emplace(EntityLoc, Holder.CellCenter, FColor::Cyan, 1.5f);
		}
	}
}
