// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "ArcEntitySpawnerTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcEntitySpawner, Log, All);

UENUM(BlueprintType)
enum class EArcEntitySpawnMode : uint8
{
	/** TQS query determines spawn positions, spawns on BeginPlay. */
	Default,

	/** A StateTree running on a Mass agent controls spawning via tasks. */
	StateTree
};

class UMassEntityConfigAsset;

/** Fragment on spawner entity: tracks all entities it has spawned. */
USTRUCT()
struct ARCENTITYSPAWNER_API FArcSpawnerFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FMassEntityHandle> SpawnedEntities;

	/** Persisted stable GUIDs of currently alive spawned entities. */
	UPROPERTY(SaveGame)
	TArray<FGuid> SpawnedEntityGuids;
};

template<>
struct TMassFragmentTraits<FArcSpawnerFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/** Fragment on spawned entities (optional): identifies which spawner created them. */
USTRUCT()
struct ARCENTITYSPAWNER_API FArcSpawnedByFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassEntityHandle SpawnerEntity;

	/** Stable spawner GUID — persisted, links entity back to its spawner across save/load. */
	UPROPERTY(SaveGame)
	FGuid SpawnerGuid;
};

/** Async message payload broadcast when entities are spawned. */
USTRUCT(BlueprintType)
struct ARCENTITYSPAWNER_API FArcEntitiesSpawnedMessage
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMassEntityHandle> SpawnedEntities;

	UPROPERTY()
	TArray<FVector> SpawnLocations;

	UPROPERTY()
	FMassEntityHandle SpawnerEntity;

	UPROPERTY()
	TSoftObjectPtr<UMassEntityConfigAsset> EntityConfig;
};

/**
 * Base class for post-spawn entity initializers.
 * Subclass in C++ or Blueprint to customize entities after spawning.
 * Initializers run while the EntityCreationContext is alive — observers haven't fired yet.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class ARCENTITYSPAWNER_API UArcEntityInitializer : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Called to initialize a batch of freshly spawned entities.
	 * Override in C++ for direct fragment access, or implement BP_InitializeEntities in Blueprint.
	 */
	virtual void InitializeEntities(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities);

protected:
	/** Blueprint override point for entity initialization. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mass|Spawn", meta = (DisplayName = "Initialize Entities"))
	void BP_InitializeEntities(const TArray<FMassEntityHandle>& Entities);
};
