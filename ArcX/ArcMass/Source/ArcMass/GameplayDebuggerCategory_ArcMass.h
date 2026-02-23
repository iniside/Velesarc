// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "GameplayDebuggerCategory.h"

struct FMassEntityManager;
class UArcEntityVisualizationSubsystem;
class APlayerController;
class AActor;

/**
 * Gameplay Debugger category for ArcMass entity visualization.
 *
 * Draws:
 * - Visualization grid cells with color-coded states (active/inactive/empty)
 * - Per-entity markers with representation type (actor vs ISM)
 * - Lifecycle phase labels
 * - Distance information from player
 * - Floor-level shading to indicate cell activation state
 */
class FGameplayDebuggerCategory_ArcMass : public FGameplayDebuggerCategory
{
	using Super = FGameplayDebuggerCategory;

public:
	FGameplayDebuggerCategory_ArcMass();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

private:
	void OnToggleCells() { bShowCells = !bShowCells; }
	void OnToggleEntities() { bShowEntities = !bShowEntities; }
	void OnToggleLifecycle() { bShowLifecycle = !bShowLifecycle; }
	void OnToggleDistances() { bShowDistances = !bShowDistances; }

	/** Collect and draw cell information. */
	void CollectCellData(const UArcEntityVisualizationSubsystem& VisSub, FMassEntityManager& EntityManager, const FVector& ViewLocation);

	/** Collect and draw per-entity information. */
	void CollectEntityData(FMassEntityManager& EntityManager, const FVector& ViewLocation, const FVector& ViewDirection, const UArcEntityVisualizationSubsystem& VisSub);

	// --- Toggle states ---
	bool bShowCells = true;
	bool bShowEntities = true;
	bool bShowLifecycle = true;
	bool bShowDistances = true;

	// --- Cached label data for DrawData ---
	struct FEntityLabel
	{
		float Score = 0.f;
		FVector Location = FVector::ZeroVector;
		FString Text;
	};
	TArray<FEntityLabel> EntityLabels;
};

#endif // WITH_GAMEPLAY_DEBUGGER
