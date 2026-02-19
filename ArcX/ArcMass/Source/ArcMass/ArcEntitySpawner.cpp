// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntitySpawner.h"

#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSimulationSubsystem.h"
#include "MassSpawnerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntitySpawner)

AArcEntitySpawner::AArcEntitySpawner()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent->Mobility = EComponentMobility::Static;

	SetCanBeDamaged(false);
	bAutoSpawnOnBeginPlay = true;

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	SetActorHiddenInGame(true);
#endif
}

void AArcEntitySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoSpawnOnBeginPlay)
	{
		return;
	}

	check(GEngine);
	const ENetMode NetMode = GEngine->GetNetMode(GetWorld());
	if (NetMode == NM_Client)
	{
		return;
	}

	const UMassSimulationSubsystem* SimSubsystem = UWorld::GetSubsystem<UMassSimulationSubsystem>(GetWorld());
	if (SimSubsystem == nullptr || SimSubsystem->IsSimulationStarted())
	{
		DoSpawning();
	}
	else
	{
		SimulationStartedHandle = UMassSimulationSubsystem::GetOnSimulationStarted().AddLambda([this](UWorld* InWorld)
		{
			if (GetWorld() == InWorld)
			{
				DoSpawning();
			}
		});
	}
}

void AArcEntitySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UMassSimulationSubsystem::GetOnSimulationStarted().Remove(SimulationStartedHandle);

	if (StreamingHandle.IsValid() && StreamingHandle->IsActive())
	{
		StreamingHandle->CancelHandle();
	}

	DoDespawning();

	Super::EndPlay(EndPlayReason);
}

void AArcEntitySpawner::BeginDestroy()
{
	DoDespawning();

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
	if (SpawnDataGenerators.Num() == 0)
	{
		UE_LOG(LogArcEntitySpawner, Warning, TEXT("%s: No Spawn Data Generators configured."), *GetName());
		return;
	}

	if (EntityTypes.Num() == 0)
	{
		UE_LOG(LogArcEntitySpawner, Warning, TEXT("%s: No EntityTypes configured."), *GetName());
		return;
	}

	AllGeneratedResults.Reset();

	float TotalProportion = 0.0f;
	for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
	{
		if (Generator.GeneratorInstance)
		{
			Generator.bDataGenerated = false;
			TotalProportion += Generator.Proportion;
		}
	}

	if (TotalProportion <= 0.0f)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: Total generator proportion must be > 0."), *GetName());
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

	const int32 TotalSpawnCount = GetSpawnCount();

	auto GenerateSpawnData = [this, TotalSpawnCount, TotalProportion]()
	{
		int32 SpawnCountRemaining = TotalSpawnCount;
		float ProportionRemaining = TotalProportion;

		for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
		{
			if (Generator.Proportion == 0.0f || ProportionRemaining <= 0.0f)
			{
				Generator.bDataGenerated = true;
				continue;
			}

			if (Generator.GeneratorInstance)
			{
				const float ProportionRatio = FMath::Min(Generator.Proportion / ProportionRemaining, 1.0f);
				const int32 SpawnCount = FMath::CeilToInt(static_cast<float>(SpawnCountRemaining) * ProportionRatio);

				FFinishedGeneratingSpawnDataSignature Delegate = FFinishedGeneratingSpawnDataSignature::CreateUObject(
					this, &AArcEntitySpawner::OnSpawnDataGenerationFinished, &Generator);
				Generator.GeneratorInstance->Generate(*this, EntityTypes, SpawnCount, Delegate);

				SpawnCountRemaining -= SpawnCount;
				ProportionRemaining -= Generator.Proportion;
			}
		}
	};

	if (AssetsToLoad.Num() > 0)
	{
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamingHandle = StreamableManager.RequestAsyncLoad(AssetsToLoad,
			FStreamableDelegate::CreateWeakLambda(this, GenerateSpawnData));
	}
	else
	{
		GenerateSpawnData();
	}
}

void AArcEntitySpawner::OnSpawnDataGenerationFinished(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results, FMassSpawnDataGenerator* FinishedGenerator)
{
	if (UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown || World->IsCleanedUp())
		{
			return;
		}
	}

	AllGeneratedResults.Append(Results.GetData(), Results.Num());

	bool bAllGenerated = true;
	for (FMassSpawnDataGenerator& Generator : SpawnDataGenerators)
	{
		if (&Generator == FinishedGenerator)
		{
			Generator.bDataGenerated = true;
		}
		bAllGenerated &= Generator.bDataGenerated;
	}

	if (bAllGenerated)
	{
		SpawnGeneratedEntities(AllGeneratedResults);
		AllGeneratedResults.Reset();
	}
}

void AArcEntitySpawner::SpawnGeneratedEntities(TConstArrayView<FMassEntitySpawnDataGeneratorResult> Results)
{
	UMassSpawnerSubsystem* SpawnerSystem = UWorld::GetSubsystem<UMassSpawnerSubsystem>(GetWorld());
	if (SpawnerSystem == nullptr)
	{
		UE_LOG(LogArcEntitySpawner, Error, TEXT("%s: UMassSpawnerSubsystem missing."), *GetName());
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	for (const FMassEntitySpawnDataGeneratorResult& Result : Results)
	{
		if (Result.NumEntities <= 0)
		{
			continue;
		}

		check(EntityTypes.IsValidIndex(Result.EntityConfigIndex));

		const FMassSpawnedEntityType& EntityType = EntityTypes[Result.EntityConfigIndex];
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

		FSpawnedEntities& SpawnedEntities = AllSpawnedEntities.AddDefaulted_GetRef();
		SpawnedEntities.TemplateID = EntityTemplate.GetTemplateID();

		// Spawn entities and capture the creation context
		TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
			SpawnerSystem->SpawnEntities(EntityTemplate.GetTemplateID(), Result.NumEntities,
				Result.SpawnData, Result.SpawnDataProcessor, SpawnedEntities.Entities);

		// Run initializers while the creation context is alive — observers haven't fired yet
		if (PostSpawnInitializers.Num() > 0 && SpawnedEntities.Entities.Num() > 0)
		{
			RunInitializers(SpawnerSystem->GetEntityManagerChecked(), SpawnedEntities.Entities);
		}

		// CreationContext released here → deferred commands flush → observers fire
	}

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
