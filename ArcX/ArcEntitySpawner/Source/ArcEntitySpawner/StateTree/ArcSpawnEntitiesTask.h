// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "MassSpawnerTypes.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSTypes.h"
#include "ArcSpawnEntitiesTask.generated.h"

class UMassEntityConfigAsset;

USTRUCT()
struct FArcSpawnEntitiesTaskInstanceData
{
	GENERATED_BODY()

	/** Number of entities to spawn. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 Count = 10;

	/** Entity type to spawn (references a MassEntityConfigAsset). */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FMassSpawnedEntityType EntityType;

	/** Spawn locations from a TQS query. Bind from FArcTQSRunQueryTask.ResultArray output. */
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	TArray<FArcTQSTargetItem> SpawnLocations;

	/** Output: handles of all spawned entities. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FMassEntityHandle> SpawnedEntities;

	/** Output: whether spawning succeeded. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bSuccess = false;
};

/**
 * Self-contained StateTree task that spawns Mass entities directly.
 * Does NOT depend on AArcEntitySpawner — calls UMassSpawnerSubsystem APIs directly.
 *
 * Spawns entities at locations from TQS query results, sets their FTransformFragment,
 * optionally marks them with FArcSpawnedByFragment, tracks them on FArcSpawnerFragment,
 * registers with UArcEntitySpawnerSubsystem, and optionally broadcasts an async message.
 *
 * Signals the entity with NewStateTreeTaskRequired on completion.
 */
USTRUCT(meta = (DisplayName = "Arc Spawn Entities", Category = "Arc|Spawner"))
struct ARCENTITYSPAWNER_API FArcSpawnEntitiesTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcSpawnEntitiesTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Whether to add FArcSpawnedByFragment to each spawned entity. */
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bMarkSpawnedEntities = false;

	/** If set, broadcast FArcEntitiesSpawnedMessage on this channel. Empty = skip broadcast. */
	UPROPERTY(EditAnywhere, Category = "Task")
	FGameplayTag MessageChannel;

#if WITH_EDITOR
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Add"); }
	virtual FColor GetIconColor() const override { return FColor(100, 200, 100); }
#endif
};
