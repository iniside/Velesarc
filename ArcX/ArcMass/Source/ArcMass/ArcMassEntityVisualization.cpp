// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualization.h"

#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "MassCommonFragments.h"

TAutoConsoleVariable<bool> CVarArcDebugDrawVisualizationGrid(
	TEXT("arc.mass.DebugDrawVisualizationGrid"),
	false,
	TEXT("Toggles debug drawing for entity visualization grid active cells (0 = off, 1 = on)"));

// ---------------------------------------------------------------------------
// UArcEntityVisualizationSubsystem
// ---------------------------------------------------------------------------

void UArcEntityVisualizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Grid.CellSize = 1000.f;
	SwapRadius = 5000.f;
	RecomputeSwapRadiusCells();
	LastPlayerCell = FIntVector(TNumericLimits<int32>::Max());
}

void UArcEntityVisualizationSubsystem::Deinitialize()
{
	for (auto& Pair : CellManagers)
	{
		if (Pair.Value.ManagerActor)
		{
			Pair.Value.ManagerActor->Destroy();
		}
	}
	CellManagers.Empty();

	if (GlobalManager.ManagerActor)
	{
		GlobalManager.ManagerActor->Destroy();
	}
	GlobalManager = FCellManager();
	Grid.Clear();

	Super::Deinitialize();
}

void UArcEntityVisualizationSubsystem::SetCellSize(float NewCellSize)
{
	Grid.CellSize = FMath::Max(1.f, NewCellSize);
	RecomputeSwapRadiusCells();
}

void UArcEntityVisualizationSubsystem::SetSwapRadius(float NewRadius)
{
	SwapRadius = FMath::Max(0.f, NewRadius);
	RecomputeSwapRadiusCells();
}

void UArcEntityVisualizationSubsystem::RecomputeSwapRadiusCells()
{
	SwapRadiusCells = FMath::CeilToInt(SwapRadius / Grid.CellSize);
}

// --- Entity Registration ---

void UArcEntityVisualizationSubsystem::RegisterEntity(FMassEntityHandle Entity, const FVector& Position)
{
	Grid.AddEntity(Entity, Position);
}

void UArcEntityVisualizationSubsystem::UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	Grid.RemoveEntity(Entity, Cell);
}

// --- ISM Management ---

UArcEntityVisualizationSubsystem::FCellManager& UArcEntityVisualizationSubsystem::GetOrCreateCellManager(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass)
{
	const FCellManagerKey Key{Cell, ManagerClass.Get()};
	if (FCellManager* Existing = CellManagers.Find(Key))
	{
		return *Existing;
	}

	FCellManager& NewManager = CellManagers.Add(Key);

	UWorld* World = GetWorld();
	if (World)
	{
		const float CellSize = Grid.CellSize;
		const FVector CellCenter(
			Cell.X * CellSize + CellSize * 0.5f,
			Cell.Y * CellSize + CellSize * 0.5f,
			Cell.Z * CellSize + CellSize * 0.5f
		);

		UClass* MgrClass = ManagerClass.Get() ? ManagerClass.Get() : AActor::StaticClass();
		FActorSpawnParameters SpawnParams;
		NewManager.ManagerActor = World->SpawnActor<AActor>(MgrClass, FTransform(CellCenter), SpawnParams);
		if (NewManager.ManagerActor)
		{
			NewManager.ManagerActor->SetActorHiddenInGame(false);
		}
	}

	return NewManager;
}

UInstancedStaticMeshComponent* UArcEntityVisualizationSubsystem::GetOrCreateISMComponent(
	FCellManager& Manager,
	UStaticMesh* Mesh,
	const TArray<TObjectPtr<UMaterialInterface>>& Materials,
	bool bCastShadows)
{
	if (!Mesh || !Manager.ManagerActor)
	{
		return nullptr;
	}

	const FObjectKey Key(Mesh);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = Manager.ISMComponents.Find(Key))
	{
		return *Existing;
	}

	UInstancedStaticMeshComponent* NewISMC = NewObject<UInstancedStaticMeshComponent>(Manager.ManagerActor);
	NewISMC->SetStaticMesh(Mesh);
	NewISMC->SetCastShadow(bCastShadows);

	for (int32 i = 0; i < Materials.Num(); i++)
	{
		if (Materials[i])
		{
			NewISMC->SetMaterial(i, Materials[i]);
		}
	}

	NewISMC->SetWorldTransform(FTransform::Identity);
	NewISMC->RegisterComponent();
	Manager.ManagerActor->AddInstanceComponent(NewISMC);

	Manager.ISMComponents.Add(Key, NewISMC);
	return NewISMC;
}

int32 UArcEntityVisualizationSubsystem::AddISMInstance(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	const TArray<TObjectPtr<UMaterialInterface>>& Materials,
	bool bCastShadows,
	const FTransform& Transform,
	FMassEntityHandle OwnerEntity)
{
	FCellManager& Manager = GetOrCreateCellManager(Cell, ManagerClass);
	UInstancedStaticMeshComponent* ISMC = GetOrCreateISMComponent(Manager, Mesh, Materials, bCastShadows);
	if (!ISMC)
	{
		return INDEX_NONE;
	}

	const int32 NewIndex = ISMC->AddInstance(Transform, true);

	// Track which entity owns which instance index
	const FObjectKey Key(Mesh);
	TArray<FMassEntityHandle>& Owners = Manager.InstanceOwners.FindOrAdd(Key);
	if (Owners.Num() <= NewIndex)
	{
		Owners.SetNum(NewIndex + 1);
	}
	Owners[NewIndex] = OwnerEntity;

	return NewIndex;
}

void UArcEntityVisualizationSubsystem::RemoveISMInstance(
	const FIntVector& Cell,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	int32 InstanceId,
	FMassEntityManager& EntityManager)
{
	if (!Mesh || InstanceId == INDEX_NONE)
	{
		return;
	}

	FCellManager* Manager = nullptr;
	if (ManagerClass.Get())
	{
		const FCellManagerKey Key{Cell, ManagerClass.Get()};
		Manager = CellManagers.Find(Key);
	}
	else
	{
		Manager = GlobalManager.ManagerActor ? &GlobalManager : nullptr;
	}
	if (!Manager)
	{
		return;
	}

	const FObjectKey MeshKey(Mesh);
	TObjectPtr<UInstancedStaticMeshComponent>* Found = Manager->ISMComponents.Find(MeshKey);
	if (!Found || !*Found)
	{
		return;
	}

	UInstancedStaticMeshComponent* ISMC = *Found;
	const int32 InstanceCount = ISMC->GetInstanceCount();
	if (InstanceId >= InstanceCount)
	{
		return;
	}

	TArray<FMassEntityHandle>* Owners = Manager->InstanceOwners.Find(MeshKey);

	// RemoveInstance swaps the last instance into the removed slot.
	// We need to update the swapped entity's stored ISMInstanceId.
	const int32 LastIndex = InstanceCount - 1;
	if (Owners && InstanceId != LastIndex && LastIndex >= 0)
	{
		const FMassEntityHandle SwappedEntity = (*Owners)[LastIndex];
		if (EntityManager.IsEntityValid(SwappedEntity))
		{
			FArcVisRepresentationFragment* SwappedRep = EntityManager.GetFragmentDataPtr<FArcVisRepresentationFragment>(SwappedEntity);
			if (SwappedRep && SwappedRep->ISMInstanceId == LastIndex)
			{
				SwappedRep->ISMInstanceId = InstanceId;
			}
		}

		// Update owner tracking
		(*Owners)[InstanceId] = SwappedEntity;
	}

	// Shrink owner array
	if (Owners && Owners->Num() > 0)
	{
		Owners->RemoveAt(LastIndex);
	}

	ISMC->RemoveInstance(InstanceId);
}

AActor* UArcEntityVisualizationSubsystem::GetCellManagerActor(const FIntVector& Cell, TSubclassOf<AActor> ManagerClass) const
{
	if (!ManagerClass.Get())
	{
		return GlobalManager.ManagerActor;
	}

	const FCellManagerKey Key{Cell, ManagerClass.Get()};
	if (const FCellManager* Manager = CellManagers.Find(Key))
	{
		return Manager->ManagerActor;
	}
	return nullptr;
}

// --- Cell Tracking ---

void UArcEntityVisualizationSubsystem::UpdatePlayerCell(const FIntVector& NewCell)
{
	LastPlayerCell = NewCell;
}

bool UArcEntityVisualizationSubsystem::IsActiveCellCoord(const FIntVector& Cell) const
{
	const int32 DX = FMath::Abs(Cell.X - LastPlayerCell.X);
	const int32 DY = FMath::Abs(Cell.Y - LastPlayerCell.Y);
	const int32 DZ = FMath::Abs(Cell.Z - LastPlayerCell.Z);
	return DX <= SwapRadiusCells && DY <= SwapRadiusCells && DZ <= SwapRadiusCells;
}

void UArcEntityVisualizationSubsystem::GetActiveCells(TArray<FIntVector>& OutCells) const
{
	Grid.GetCellsInRadius(LastPlayerCell, SwapRadiusCells, OutCells);
}

// ---------------------------------------------------------------------------
// UArcVisEntitySpawnLibrary
// ---------------------------------------------------------------------------

FMassEntityHandle UArcVisEntitySpawnLibrary::SpawnVisEntity(UObject* WorldContextObject, const UMassEntityConfigAsset* EntityConfig, FTransform Transform)
{
	if (!WorldContextObject || !EntityConfig)
	{
		return FMassEntityHandle();
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FMassEntityHandle();
	}

	UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
	if (!SpawnerSubsystem)
	{
		return FMassEntityHandle();
	}

	const FMassEntityTemplate& EntityTemplate = EntityConfig->GetOrCreateEntityTemplate(*World);
	if (!EntityTemplate.IsValid())
	{
		return FMassEntityHandle();
	}

	TArray<FMassEntityHandle> SpawnedEntities;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext = SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);

	if (SpawnedEntities.IsEmpty())
	{
		return FMassEntityHandle();
	}

	// Set the transform on the spawned entity
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(SpawnedEntities[0]);
	if (TransformFragment)
	{
		TransformFragment->SetTransform(Transform);
	}

	return SpawnedEntities[0];
}
