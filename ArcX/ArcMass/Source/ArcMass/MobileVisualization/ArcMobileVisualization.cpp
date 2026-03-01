// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisualization.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ---------------------------------------------------------------------------
// UArcMobileVisSubsystem
// ---------------------------------------------------------------------------

void UArcMobileVisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegionStride = 2 * RegionExtent + 1;
}

void UArcMobileVisSubsystem::Deinitialize()
{
	for (auto& Pair : RegionManagers)
	{
		if (Pair.Value.ManagerActor)
		{
			Pair.Value.ManagerActor->Destroy();
		}
	}
	RegionManagers.Empty();
	EntityCells.Empty();
	SourceCellPositions.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Cell size / region configuration
// ---------------------------------------------------------------------------

void UArcMobileVisSubsystem::SetCellSize(float NewCellSize)
{
	CellSize = FMath::Max(1.f, NewCellSize);
}

void UArcMobileVisSubsystem::SetRegionExtent(int32 NewExtent)
{
	RegionExtent = FMath::Max(1, NewExtent);
	RegionStride = 2 * RegionExtent + 1;
}

// ---------------------------------------------------------------------------
// Grid utilities
// ---------------------------------------------------------------------------

FIntVector UArcMobileVisSubsystem::WorldToCell(const FVector& WorldPos) const
{
	const float InvCellSize = 1.f / CellSize;
	return FIntVector(
		FMath::FloorToInt(WorldPos.X * InvCellSize),
		FMath::FloorToInt(WorldPos.Y * InvCellSize),
		FMath::FloorToInt(WorldPos.Z * InvCellSize)
	);
}

FIntVector UArcMobileVisSubsystem::CellToRegion(const FIntVector& CellCoord) const
{
	auto FloorDiv = [](int32 A, int32 B) -> int32
	{
		return (A >= 0) ? (A / B) : ((A - B + 1) / B);
	};

	return FIntVector(
		FloorDiv(CellCoord.X, RegionStride),
		FloorDiv(CellCoord.Y, RegionStride),
		FloorDiv(CellCoord.Z, RegionStride)
	);
}

// ---------------------------------------------------------------------------
// Entity grid
// ---------------------------------------------------------------------------

void UArcMobileVisSubsystem::RegisterEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	EntityCells.FindOrAdd(Cell).Add(Entity);
}

void UArcMobileVisSubsystem::UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	if (TArray<FMassEntityHandle>* Entities = EntityCells.Find(Cell))
	{
		Entities->RemoveSwap(Entity);
		if (Entities->IsEmpty())
		{
			EntityCells.Remove(Cell);
		}
	}
}

void UArcMobileVisSubsystem::MoveEntityCell(FMassEntityHandle Entity, const FIntVector& OldCell, const FIntVector& NewCell)
{
	UnregisterEntity(Entity, OldCell);
	RegisterEntity(Entity, NewCell);
}

const TArray<FMassEntityHandle>* UArcMobileVisSubsystem::GetEntitiesInCell(const FIntVector& Cell) const
{
	return EntityCells.Find(Cell);
}

// ---------------------------------------------------------------------------
// Source grid
// ---------------------------------------------------------------------------

void UArcMobileVisSubsystem::RegisterSource(FMassEntityHandle Entity, const FIntVector& Cell)
{
	SourceCellPositions.Add(Entity, Cell);
}

void UArcMobileVisSubsystem::UnregisterSource(FMassEntityHandle Entity, const FIntVector& Cell)
{
	SourceCellPositions.Remove(Entity);
}

void UArcMobileVisSubsystem::MoveSourceCell(FMassEntityHandle Entity, const FIntVector& OldCell, const FIntVector& NewCell)
{
	if (FIntVector* Existing = SourceCellPositions.Find(Entity))
	{
		*Existing = NewCell;
	}
}

// ---------------------------------------------------------------------------
// LOD evaluation
// ---------------------------------------------------------------------------

EArcMobileVisLOD UArcMobileVisSubsystem::EvaluateEntityLOD(
	const FIntVector& EntityCell,
	int32 ActorRadiusCells,
	int32 ISMRadiusCells,
	int32 ActorRadiusCellsLeave,
	int32 ISMRadiusCellsLeave,
	EArcMobileVisLOD CurrentLOD) const
{
	if (SourceCellPositions.IsEmpty())
	{
		return EArcMobileVisLOD::None;
	}

	// Find minimum Chebyshev distance to any source
	int32 MinDist = TNumericLimits<int32>::Max();
	for (const auto& Pair : SourceCellPositions)
	{
		const FIntVector& SourceCell = Pair.Value;
		const int32 DX = FMath::Abs(EntityCell.X - SourceCell.X);
		const int32 DY = FMath::Abs(EntityCell.Y - SourceCell.Y);
		const int32 DZ = FMath::Abs(EntityCell.Z - SourceCell.Z);
		const int32 Dist = FMath::Max3(DX, DY, DZ);
		MinDist = FMath::Min(MinDist, Dist);
	}

	// Apply hysteresis: use leave thresholds when already at a higher LOD
	const int32 EffectiveActorRadius = (CurrentLOD == EArcMobileVisLOD::Actor)
		? ActorRadiusCellsLeave
		: ActorRadiusCells;

	const int32 EffectiveISMRadius = (CurrentLOD == EArcMobileVisLOD::Actor || CurrentLOD == EArcMobileVisLOD::StaticMesh)
		? ISMRadiusCellsLeave
		: ISMRadiusCells;

	if (MinDist <= EffectiveActorRadius)
	{
		return EArcMobileVisLOD::Actor;
	}
	if (MinDist <= EffectiveISMRadius)
	{
		return EArcMobileVisLOD::StaticMesh;
	}
	return EArcMobileVisLOD::None;
}

void UArcMobileVisSubsystem::GetEntityCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells) const
{
	OutCells.Reset();
	for (const auto& Pair : EntityCells)
	{
		const FIntVector& Cell = Pair.Key;
		const int32 DX = FMath::Abs(Cell.X - Center.X);
		const int32 DY = FMath::Abs(Cell.Y - Center.Y);
		const int32 DZ = FMath::Abs(Cell.Z - Center.Z);
		const int32 Dist = FMath::Max3(DX, DY, DZ);
		if (Dist <= RadiusCells)
		{
			OutCells.Add(Cell);
		}
	}
}

// ---------------------------------------------------------------------------
// Region manager
// ---------------------------------------------------------------------------

UArcMobileVisSubsystem::FRegionManager& UArcMobileVisSubsystem::GetOrCreateRegionManager(
	const FIntVector& RegionCoord,
	TSubclassOf<AActor> ManagerClass)
{
	const FRegionKey Key{RegionCoord, ManagerClass.Get()};
	if (FRegionManager* Existing = RegionManagers.Find(Key))
	{
		return *Existing;
	}

	FRegionManager& NewManager = RegionManagers.Add(Key);

	UWorld* World = GetWorld();
	if (World)
	{
		// Region center: (RegionCoord * RegionStride + RegionExtent) * CellSize + CellSize*0.5 for X/Y, 0 for Z
		const FVector RegionCenter(
			(RegionCoord.X * RegionStride + RegionExtent) * CellSize + CellSize * 0.5f,
			(RegionCoord.Y * RegionStride + RegionExtent) * CellSize + CellSize * 0.5f,
			0.f
		);

		UClass* MgrClass = ManagerClass.Get() ? ManagerClass.Get() : AActor::StaticClass();
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		NewManager.ManagerActor = World->SpawnActor<AActor>(MgrClass, FTransform(RegionCenter), SpawnParams);
		if (NewManager.ManagerActor)
		{
			NewManager.ManagerActor->SetActorHiddenInGame(false);
		}
	}

	return NewManager;
}

// ---------------------------------------------------------------------------
// ISM component management
// ---------------------------------------------------------------------------

UInstancedStaticMeshComponent* UArcMobileVisSubsystem::GetOrCreateISMComponent(
	FRegionManager& Manager,
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
	NewISMC->SetMobility(EComponentMobility::Stationary);
	NewISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	for (int32 i = 0; i < Materials.Num(); i++)
	{
		if (Materials[i])
		{
			NewISMC->SetMaterial(i, Materials[i]);
		}
	}

	NewISMC->RegisterComponent();
	Manager.ManagerActor->AddInstanceComponent(NewISMC);

	Manager.ISMComponents.Add(Key, NewISMC);
	Manager.InstanceOwners.FindOrAdd(Key);

	return NewISMC;
}

// ---------------------------------------------------------------------------
// ISM instance management
// ---------------------------------------------------------------------------

int32 UArcMobileVisSubsystem::AddISMInstance(
	const FIntVector& RegionCoord,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	const TArray<TObjectPtr<UMaterialInterface>>& Materials,
	bool bCastShadows,
	const FTransform& Transform,
	FMassEntityHandle OwnerEntity)
{
	FRegionManager& Manager = GetOrCreateRegionManager(RegionCoord, ManagerClass);
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

void UArcMobileVisSubsystem::RemoveISMInstance(
	const FIntVector& RegionCoord,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	int32 InstanceId,
	FMassEntityManager& EntityManager)
{
	if (!Mesh || InstanceId == INDEX_NONE)
	{
		return;
	}

	const FRegionKey Key{RegionCoord, ManagerClass.Get()};
	FRegionManager* Manager = RegionManagers.Find(Key);
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
			FArcMobileVisRepFragment* SwappedRep = EntityManager.GetFragmentDataPtr<FArcMobileVisRepFragment>(SwappedEntity);
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

void UArcMobileVisSubsystem::UpdateISMTransform(
	const FIntVector& RegionCoord,
	TSubclassOf<AActor> ManagerClass,
	UStaticMesh* Mesh,
	int32 InstanceId,
	const FTransform& NewTransform)
{
	if (!Mesh || InstanceId == INDEX_NONE)
	{
		return;
	}

	const FRegionKey Key{RegionCoord, ManagerClass.Get()};
	FRegionManager* Manager = RegionManagers.Find(Key);
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
	ISMC->UpdateInstanceTransform(InstanceId, NewTransform, true, true, true);
}
