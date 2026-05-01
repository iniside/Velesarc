// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawner.h"

#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MassAgentComponent.h"
#include "MassEntityConfigAsset.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSTypes.h"
#include "ArcEntitySpawnerSubsystem.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/ArcMassFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntitySpawner)

AArcEntitySpawner::AArcEntitySpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->Mobility = EComponentMobility::Static;

	AgentComponent = CreateDefaultSubobject<UMassAgentComponent>(TEXT("MassAgent"));

	SetCanBeDamaged(false);
	bAutoSpawnOnBeginPlay = true;
	SpawnerGuid = FGuid::NewGuid();

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	SetActorHiddenInGame(true);
#endif
}

void AArcEntitySpawner::BeginPlay()
{
	Super::BeginPlay();

	check(GEngine);
	const ENetMode NetMode = GEngine->GetNetMode(GetWorld());
	if (NetMode == NM_Client)
	{
		return;
	}

	if (UArcEntitySpawnerSubsystem* SpawnerSub = UWorld::GetSubsystem<UArcEntitySpawnerSubsystem>(GetWorld()))
	{
		SpawnerSub->RegisterActorSpawner(SpawnerGuid, this);
	}

	switch (SpawnMode)
	{
	case EArcEntitySpawnMode::Default:
		BeginPlayDefault();
		break;
	case EArcEntitySpawnMode::StateTree:
		BeginPlayStateTree();
		break;
	}
}

void AArcEntitySpawner::BeginPlayDefault()
{
	if (!bAutoSpawnOnBeginPlay)
	{
		return;
	}

	auto DoSpawnOrReconcile = [this]()
	{
		if (bHasSpawned)
		{
			// Loaded from save — PostLoad hook already reconciled entity handles.
			// Just check if we need to fill a gap with fresh spawns.
			const int32 RestoredCount = SpawnedEntityGuids.Num();
			const int32 Gap = GetSpawnCount() - RestoredCount;

			if (Gap > 0 && bAutoRespawnOnDeath)
			{
				const int32 OriginalCount = Count;
				Count = Gap;
				DoSpawning();
				Count = OriginalCount;
			}
			else
			{
				OnSpawningFinished.Broadcast();
			}
		}
		else
		{
			DoSpawning();
		}
	};

	const UMassSimulationSubsystem* SimSubsystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(GetWorld());
	if (SimSubsystem == nullptr || SimSubsystem->IsSimulationStarted())
	{
		DoSpawnOrReconcile();
	}
	else
	{
		SimulationStartedHandle = UMassSimulationSubsystem::GetOnSimulationStarted().AddLambda(
			[this, DoSpawnOrReconcile](UWorld* InWorld)
			{
				if (GetWorld() == InWorld)
				{
					DoSpawnOrReconcile();
				}
			});
	}
}

void AArcEntitySpawner::BeginPlayStateTree()
{
	if (AgentComponent)
	{
		AgentComponent->Enable();
	}
}

void AArcEntitySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UMassSimulationSubsystem::GetOnSimulationStarted().Remove(SimulationStartedHandle);

	if (StreamingHandle.IsValid() && StreamingHandle->IsActive())
	{
		StreamingHandle->CancelHandle();
	}

	// Abort any running TQS query
	if (ActiveTQSQueryId != INDEX_NONE)
	{
		if (UArcTQSQuerySubsystem* TQS = UWorld::GetSubsystem<UArcTQSQuerySubsystem>(GetWorld()))
		{
			TQS->AbortQuery(ActiveTQSQueryId);
		}
		ActiveTQSQueryId = INDEX_NONE;
	}

	if (EndPlayReason != EEndPlayReason::EndPlayInEditor)
	{
		DoDespawning();	
	}
	
	if (UArcEntitySpawnerSubsystem* SpawnerSub = UWorld::GetSubsystem<UArcEntitySpawnerSubsystem>(GetWorld()))
	{
		SpawnerSub->UnregisterActorSpawner(SpawnerGuid);
	}

	Super::EndPlay(EndPlayReason);
}

void AArcEntitySpawner::BeginDestroy()
{
	//DoDespawning();

	if (StreamingHandle.IsValid() && StreamingHandle->IsActive())
	{
		StreamingHandle->CancelHandle();
	}

	Super::BeginDestroy();
}

#if WITH_EDITOR
void AArcEntitySpawner::DEBUG_Spawn()
{
	DoSpawning();
}

void AArcEntitySpawner::DEBUG_Clear()
{
	DoDespawning();
}
#endif

int32 AArcEntitySpawner::GetSpawnCount() const
{
	return FMath::Max(0, static_cast<int32>(SpawningCountScale * static_cast<float>(Count)));
}

void AArcEntitySpawner::DoSpawning()
{
	if (EntityTypes.Num() == 0)
	{
		UE_LOG(LogArcEntitySpawner, Warning, TEXT("%s: No EntityTypes configured."), *GetName());
		return;
	}

	if (!SpawnQuery)
	{
		UE_LOG(LogArcEntitySpawner, Warning, TEXT("%s: No SpawnQuery configured."), *GetName());
		return;
	}

	// Cancel any in-flight streaming
	if (StreamingHandle.IsValid() && StreamingHandle->IsActive())
	{
		StreamingHandle->CancelHandle();
	}

	// Collect assets that need loading
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const FMassSpawnedEntityType& EntityType : EntityTypes)
	{
		if (!EntityType.IsLoaded())
		{
			AssetsToLoad.Add(EntityType.EntityConfig.ToSoftObjectPath());
		}
	}

	auto RunTQSQuery = [this]()
	{
		UArcTQSQuerySubsystem* TQSSubsystem = UWorld::GetSubsystem<UArcTQSQuerySubsystem>(GetWorld());
		if (!TQSSubsystem)
		{
			UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: UArcTQSQuerySubsystem missing."), *GetName());
			return;
		}

		// Build query context: querier is the spawner actor at its location
		FArcTQSQueryContext QueryContext;
		QueryContext.QuerierLocation = GetActorLocation();
		QueryContext.QuerierForward = GetActorForwardVector();
		QueryContext.World = GetWorld();
		QueryContext.QuerierActor = this;

		if (UMassEntitySubsystem* MassSubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(GetWorld()))
		{
			QueryContext.EntityManager = &MassSubsystem->GetMutableEntityManager();
		}

		ActiveTQSQueryId = TQSSubsystem->RunQuery(
			SpawnQuery,
			QueryContext,
			FArcTQSQueryFinished::CreateUObject(this, &AArcEntitySpawner::OnTQSQueryCompleted));
	};

	if (AssetsToLoad.Num() > 0)
	{
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamingHandle = StreamableManager.RequestAsyncLoad(AssetsToLoad,
			FStreamableDelegate::CreateWeakLambda(this, RunTQSQuery));
	}
	else
	{
		RunTQSQuery();
	}
}

void AArcEntitySpawner::OnTQSQueryCompleted(FArcTQSQueryInstance& CompletedQuery)
{
	ActiveTQSQueryId = INDEX_NONE;

	if (UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown || World->IsCleanedUp())
		{
			return;
		}
	}

	if (CompletedQuery.Status != EArcTQSQueryStatus::Completed || CompletedQuery.Results.Num() == 0)
	{
		UE_LOG(LogArcEntitySpawner, Warning, TEXT("%s: TQS query returned no results."), *GetName());
		OnSpawningFinished.Broadcast();
		return;
	}

	SpawnEntitiesFromResults(CompletedQuery.Results);
}

void AArcEntitySpawner::SpawnEntitiesFromResults(TConstArrayView<FArcTQSTargetItem> Results)
{
	TArray<FVector> Locations;
	Locations.Reserve(Results.Num());

	for (const FArcTQSTargetItem& Item : Results)
	{
		Locations.Add(Item.Location);
	}

	const int32 DesiredCount = GetSpawnCount();
	const int32 ClampedCount = FMath::Min(DesiredCount, Locations.Num());

	if (ClampedCount < DesiredCount)
	{
		UE_LOG(LogArcEntitySpawner, Warning,
			TEXT("%s: TQS query returned %d results but spawn count is %d — clamping to %d. "
				 "Configure the query to return enough results (e.g. TopN or AllPassing mode)."),
			*GetName(), Locations.Num(), DesiredCount, ClampedCount);
	}

	SpawnEntitiesAtLocations(ClampedCount, Locations);
}

void AArcEntitySpawner::SpawnEntitiesAtLocations(int32 InCount, TConstArrayView<FVector> Locations)
{
	if (InCount <= 0 || EntityTypes.Num() == 0)
	{
		OnSpawningFinished.Broadcast();
		return;
	}

	SpawnEntitiesInternal(Locations);
}

void AArcEntitySpawner::SpawnEntitiesInternal(TConstArrayView<FVector> Locations)
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSystem == nullptr)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: UMassSpawnerSubsystem missing."), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	const int32 TotalCount = GetSpawnCount();
	const int32 NumLocations = Locations.Num();

	// Distribute entities across entity types proportionally
	for (int32 TypeIndex = 0; TypeIndex < EntityTypes.Num(); ++TypeIndex)
	{
		const FMassSpawnedEntityType& EntityType = EntityTypes[TypeIndex];
		const UMassEntityConfigAsset* EntityConfig = EntityType.GetEntityConfig();
		if (EntityConfig == nullptr)
		{
			continue;
		}

		const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
		if (!EntityTemplate.IsValid())
		{
			continue;
		}

		// For single entity type, spawn all. For multiple, distribute by proportion.
		const int32 SpawnCount = (EntityTypes.Num() == 1)
			? TotalCount
			: FMath::Max(1, FMath::RoundToInt(TotalCount * EntityType.Proportion));

		FSpawnedEntities& SpawnedEntities = AllSpawnedEntities.AddDefaulted_GetRef();
		SpawnedEntities.TemplateID = EntityTemplate.GetTemplateID();

		// Spawn entities — no SpawnData/Processor, we set transforms manually
		TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), SpawnCount,
				FConstStructView(), nullptr, SpawnedEntities.Entities);

		// Set transforms from location array while creation context is alive (strict 1:1)
		FMassEntityManager& EntityManager = SpawnerSystem->GetEntityManagerChecked();
		for (int32 i = 0; i < SpawnedEntities.Entities.Num(); ++i)
		{
			const FMassEntityHandle& Entity = SpawnedEntities.Entities[i];

			if (FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				FVector SpawnLocation = (i < NumLocations)
					? Locations[i]
					: GetActorLocation();

				const FArcAgentCapsuleFragment* CapsuleFrag =
					EntityManager.GetFragmentDataPtr<FArcAgentCapsuleFragment>(Entity);
				if (CapsuleFrag)
				{
					SpawnLocation.Z += CapsuleFrag->HalfHeight;
				}

				TransformFrag->GetMutableTransform().SetLocation(SpawnLocation);
			}
		}

		// Run initializers while the creation context is alive — observers haven't fired yet
		if (PostSpawnInitializers.Num() > 0 && SpawnedEntities.Entities.Num() > 0)
		{
			RunInitializers(EntityManager, SpawnedEntities.Entities);
		}

		// Stamp SpawnerGuid on spawned entities and track their PersistenceGuids
		TrackSpawnedEntities(EntityManager, SpawnedEntities.Entities);

		// CreationContext released here → deferred commands flush → observers fire
	}

	bHasSpawned = true;

	OnSpawningFinished.Broadcast();
}

void AArcEntitySpawner::RunInitializers(FMassEntityManager& EntityManager, TArrayView<const FMassEntityHandle> Entities)
{
	for (UArcEntityInitializer* Initializer : PostSpawnInitializers)
	{
		if (Initializer)
		{
			Initializer->InitializeEntities(EntityManager, Entities);
		}
	}
}

void AArcEntitySpawner::DoDespawning()
{
	if (AllSpawnedEntities.IsEmpty())
	{
		return;
	}

	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSystem == nullptr)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: UMassSpawnerSubsystem missing while despawning."), *GetName());
		return;
	}

	for (const FSpawnedEntities& SpawnedEntities : AllSpawnedEntities)
	{
		SpawnerSystem->DestroyEntities(SpawnedEntities.Entities);
	}
	AllSpawnedEntities.Reset();

	OnDespawningFinished.Broadcast();
}

void AArcEntitySpawner::DespawnEntities(TConstArrayView<FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle& Entity : Entities)
	{
		DespawnEntity(Entity);
	}
}

bool AArcEntitySpawner::DespawnEntity(const FMassEntityHandle Entity)
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSystem == nullptr)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: UMassSpawnerSubsystem missing while despawning entity."), *GetName());
		return false;
	}

	for (FSpawnedEntities& SpawnedEntities : AllSpawnedEntities)
	{
		const int32 Index = SpawnedEntities.Entities.Find(Entity);
		if (Index != INDEX_NONE)
		{
			SpawnerSystem->DestroyEntities(MakeArrayView(&Entity, 1));
			SpawnedEntities.Entities.RemoveAtSwap(Index, EAllowShrinking::No);
			return true;
		}
	}
	return false;
}

void AArcEntitySpawner::GetAllSpawnedEntities(TArray<FMassEntityHandle>& OutEntities) const
{
	for (const FSpawnedEntities& SpawnedEntities : AllSpawnedEntities)
	{
		OutEntities.Append(SpawnedEntities.Entities);
	}
}

void AArcEntitySpawner::TrackSpawnedEntities(FMassEntityManager& EntityManager,
	TArrayView<const FMassEntityHandle> Entities)
{
	for (const FMassEntityHandle& Entity : Entities)
	{
		if (FArcSpawnedByFragment* SpawnedBy =
			EntityManager.GetFragmentDataPtr<FArcSpawnedByFragment>(Entity))
		{
			SpawnedBy->SpawnerGuid = SpawnerGuid;
		}

		if (const FArcMassPersistenceFragment* PersistFrag =
			EntityManager.GetFragmentDataPtr<FArcMassPersistenceFragment>(Entity))
		{
			if (PersistFrag->PersistenceGuid.IsValid())
			{
				SpawnedEntityGuids.Add(PersistFrag->PersistenceGuid);
			}
		}
	}
}

void AArcEntitySpawner::OnSpawnedEntityDied(const FGuid& EntityGuid, FMassEntityHandle EntityHandle)
{
	SpawnedEntityGuids.Remove(EntityGuid);

	for (FSpawnedEntities& Bucket : AllSpawnedEntities)
	{
		const int32 Index = Bucket.Entities.Find(EntityHandle);
		if (Index != INDEX_NONE)
		{
			Bucket.Entities.RemoveAtSwap(Index, EAllowShrinking::No);
			break;
		}
	}

	UE_LOG(LogArcEntitySpawner, Log, TEXT("%s: Entity %s died, %d remaining"),
		*GetName(), *EntityGuid.ToString(), SpawnedEntityGuids.Num());
}
