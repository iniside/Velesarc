// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "ArcDespawnEntitiesTask.generated.h"

USTRUCT()
struct FArcDespawnEntitiesTaskInstanceData
{
	GENERATED_BODY()

	/** Specific entities to despawn. If empty and bDespawnAll is true, reads from FArcSpawnerFragment. */
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	TArray<FMassEntityHandle> EntitiesToDespawn;

	/** If true, despawns all entities tracked in FArcSpawnerFragment on the spawner entity. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bDespawnAll = true;

	/** Output: whether despawning succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSuccess = false;
};

/**
 * Self-contained StateTree task that despawns Mass entities directly.
 * Does NOT depend on AArcEntitySpawner — calls UMassSpawnerSubsystem APIs directly.
 *
 * Reads entity handles from FArcSpawnerFragment (bDespawnAll) or from input binding.
 * Updates fragment, unregisters from subsystem, signals entity on completion.
 */
USTRUCT(meta = (DisplayName = "Arc Despawn Entities", Category = "Arc|Spawner"))
struct ARCENTITYSPAWNER_API FArcDespawnEntitiesTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcDespawnEntitiesTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Remove"); }
	virtual FColor GetIconColor() const override { return FColor(200, 80, 80); }
#endif
};
