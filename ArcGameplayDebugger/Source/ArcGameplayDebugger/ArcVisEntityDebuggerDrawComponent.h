// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcVisEntityDebuggerDrawComponent.generated.h"

struct FArcVisEntityDebugSelectedEntity
{
	FVector Location = FVector::ZeroVector;
	FColor Color = FColor::Cyan;
	FString Label;
};

struct FArcVisEntityDebugGridCell
{
	FVector Center = FVector::ZeroVector;
	FVector HalfExtent = FVector::ZeroVector;
	FColor Color = FColor::White;
	float Thickness = 2.f;
	FString CountLabel;
};

struct FArcVisEntityDebugHolderInstance
{
	FVector Location = FVector::ZeroVector;
};

struct FArcVisEntityDebugHolderDrawData
{
	FVector CellCenter = FVector::ZeroVector;
	FVector CellHalfExtent = FVector::ZeroVector;
	TArray<FArcVisEntityDebugHolderInstance> Instances;
	TArray<FVector> ReferencingEntityLocations;
};

struct FArcVisEntityDebugDrawData
{
	bool bHasSelectedEntity = false;
	FArcVisEntityDebugSelectedEntity SelectedEntity;
	bool bShowGrid = false;
	TArray<FArcVisEntityDebugGridCell> OccupiedCells;
	TArray<FArcVisEntityDebugGridCell> BoundaryCells;
	FArcVisEntityDebugGridCell PlayerCell;
	bool bHasPlayerCell = false;
	bool bHasSelectedHolder = false;
	FArcVisEntityDebugHolderDrawData SelectedHolder;
};

UCLASS(ClassGroup = Debug)
class UArcVisEntityDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdateVisData(const FArcVisEntityDebugDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcVisEntityDebugDrawData Data;
};
