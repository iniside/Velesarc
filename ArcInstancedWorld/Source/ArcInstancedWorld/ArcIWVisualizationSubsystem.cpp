// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWVisualizationSubsystem.h"
#include "ArcIWTypes.h"
#include "ArcIWSettings.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcMass/ArcMassPhysicsEntityLink.h"
#include "ArcIWMassISMPartitionActor.h"

#include "MassEntitySubsystem.h"
#include "MassEntityFragments.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ---------------------------------------------------------------------------
// UArcIWVisualizationSubsystem — Lifecycle
// ---------------------------------------------------------------------------

bool UArcIWVisualizationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer();
}

void UArcIWVisualizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CellSize = 2000.f;
	SwapRadius = 10000.f;
	SwapRadiusCells = FMath::CeilToInt(SwapRadius / CellSize);
	ActorRadius = 3000.f;
	ActorRadiusCells = FMath::CeilToInt(ActorRadius / CellSize);
	DehydrationRadius = 8000.f;
	DehydrationRadiusCells = FMath::CeilToInt(DehydrationRadius / CellSize);
	LastPlayerCell = FIntVector(TNumericLimits<int32>::Max());
}

void UArcIWVisualizationSubsystem::Deinitialize()
{
	ISMComponentLookup.Empty();
	PartitionISMDataMap.Empty();
	EntityToPartitionActor.Empty();
	CellEntities.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------

FIntVector UArcIWVisualizationSubsystem::WorldToCell(const FVector& WorldPos) const
{
	const float InvCellSize = 1.f / CellSize;
	return FIntVector(
		FMath::FloorToInt(WorldPos.X * InvCellSize),
		FMath::FloorToInt(WorldPos.Y * InvCellSize),
		FMath::FloorToInt(WorldPos.Z * InvCellSize)
	);
}

void UArcIWVisualizationSubsystem::RegisterEntity(FMassEntityHandle Entity, const FVector& Position)
{
	const FIntVector Cell = WorldToCell(Position);
	CellEntities.FindOrAdd(Cell).Add(Entity);
}

void UArcIWVisualizationSubsystem::UnregisterEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	if (TArray<FMassEntityHandle>* Entities = CellEntities.Find(Cell))
	{
		Entities->RemoveSwap(Entity);
		if (Entities->IsEmpty())
		{
			CellEntities.Remove(Cell);
		}
	}
}

const TArray<FMassEntityHandle>* UArcIWVisualizationSubsystem::GetEntitiesInCell(const FIntVector& Cell) const
{
	return CellEntities.Find(Cell);
}

// ---------------------------------------------------------------------------
// Composite ISM Management
// ---------------------------------------------------------------------------

void UArcIWVisualizationSubsystem::RegisterPartitionActor(AActor* PartitionActor, const TArray<FMassEntityHandle>& OwnedEntities)
{
	if (!PartitionActor)
	{
		return;
	}

	FPartitionISMData& Data = PartitionISMDataMap.FindOrAdd(PartitionActor);
	Data.OwnerActor = PartitionActor;

	for (FMassEntityHandle Entity : OwnedEntities)
	{
		EntityToPartitionActor.Add(Entity, PartitionActor);
	}
}

void UArcIWVisualizationSubsystem::UnregisterPartitionActor(AActor* PartitionActor)
{
	if (!PartitionActor)
	{
		return;
	}

	// Remove entity mappings
	TArray<FMassEntityHandle> ToRemove;
	for (const TPair<FMassEntityHandle, TWeakObjectPtr<AActor>>& Pair : EntityToPartitionActor)
	{
		if (Pair.Value == PartitionActor)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (FMassEntityHandle Entity : ToRemove)
	{
		EntityToPartitionActor.Remove(Entity);
	}

	// Remove ISM component reverse lookups for this partition
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> ISMCsToRemove;
	for (const TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, FISMComponentInfo>& Pair : ISMComponentLookup)
	{
		if (Pair.Value.PartitionActor == PartitionActor)
		{
			ISMCsToRemove.Add(Pair.Key);
		}
	}
	for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& ISMC : ISMCsToRemove)
	{
		ISMComponentLookup.Remove(ISMC);
	}

	PartitionISMDataMap.Remove(PartitionActor);
}

void UArcIWVisualizationSubsystem::AddCompositeISMInstances(
	const FIntVector& Cell,
	FMassEntityHandle EntityHandle,
	const FTransform& WorldTransform,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	TArray<int32>& InOutISMInstanceIds,
	int32 InClassIndex,
	int32 InEntityIndex)
{
	InOutISMInstanceIds.SetNum(MeshDescriptors.Num());

	FPartitionISMData* ISMData = FindISMDataForEntity(EntityHandle);
	if (!ISMData || !ISMData->OwnerActor.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIW: AddCompositeISMInstances - no ISMData for entity %d. EntityToPartitionActor has %d entries, PartitionISMDataMap has %d entries."),
			EntityHandle.Index, EntityToPartitionActor.Num(), PartitionISMDataMap.Num());
		for (int32& Id : InOutISMInstanceIds)
		{
			Id = INDEX_NONE;
		}
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("ArcIW: AddCompositeISMInstances - entity %d, %d mesh descriptors, owner %s"),
		EntityHandle.Index, MeshDescriptors.Num(), *ISMData->OwnerActor->GetName());

	for (int32 MeshIdx = 0; MeshIdx < MeshDescriptors.Num(); ++MeshIdx)
	{
		const FArcIWMeshEntry& MeshEntry = MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			InOutISMInstanceIds[MeshIdx] = INDEX_NONE;
			continue;
		}

		UInstancedStaticMeshComponent* ISMC = GetOrCreateISMComponent(
			*ISMData, MeshEntry.Mesh, MeshEntry.Materials, MeshEntry.bCastShadows);
		if (!ISMC)
		{
			InOutISMInstanceIds[MeshIdx] = INDEX_NONE;
			continue;
		}

		const FTransform InstanceTransform = MeshEntry.RelativeTransform * WorldTransform;
		const int32 NewIndex = ISMC->AddInstance(InstanceTransform, /*bWorldSpace=*/true);

		InOutISMInstanceIds[MeshIdx] = NewIndex;

		// Track ownership for swap-fixup
		const FObjectKey MeshKey(MeshEntry.Mesh);
		TArray<FInstanceOwnerEntry>& Owners = ISMData->InstanceOwners.FindOrAdd(MeshKey);
		if (Owners.Num() <= NewIndex)
		{
			Owners.SetNum(NewIndex + 1);
		}
		Owners[NewIndex] = FInstanceOwnerEntry{EntityHandle, MeshIdx, InClassIndex, InEntityIndex};
	}
}

void UArcIWVisualizationSubsystem::RemoveCompositeISMInstances(
	FMassEntityHandle EntityHandle,
	const TArray<FArcIWMeshEntry>& MeshDescriptors,
	const TArray<int32>& ISMInstanceIds,
	FMassEntityManager& EntityManager)
{
	FPartitionISMData* ISMData = FindISMDataForEntity(EntityHandle);
	if (!ISMData)
	{
		return;
	}

	for (int32 MeshIdx = 0; MeshIdx < MeshDescriptors.Num(); ++MeshIdx)
	{
		if (MeshIdx >= ISMInstanceIds.Num())
		{
			break;
		}

		const int32 InstanceId = ISMInstanceIds[MeshIdx];
		if (InstanceId == INDEX_NONE)
		{
			continue;
		}

		const FArcIWMeshEntry& MeshEntry = MeshDescriptors[MeshIdx];
		if (!MeshEntry.Mesh)
		{
			continue;
		}

		const FObjectKey MeshKey(MeshEntry.Mesh);
		TObjectPtr<UInstancedStaticMeshComponent>* FoundISMC = ISMData->ISMComponents.Find(MeshKey);
		if (!FoundISMC || !*FoundISMC)
		{
			continue;
		}

		UInstancedStaticMeshComponent* ISMC = *FoundISMC;
		const int32 InstanceCount = ISMC->GetInstanceCount();
		if (InstanceId >= InstanceCount)
		{
			continue;
		}

		TArray<FInstanceOwnerEntry>* Owners = ISMData->InstanceOwners.Find(MeshKey);

		// Swap-fixup: RemoveInstance swaps the last instance into the removed slot.
		const int32 LastIndex = InstanceCount - 1;
		if (Owners && InstanceId != LastIndex && LastIndex >= 0)
		{
			const FInstanceOwnerEntry SwappedOwner = (*Owners)[LastIndex];
			const FMassEntityHandle SwappedEntity = SwappedOwner.Entity;
			const int32 SwappedMeshEntryIndex = SwappedOwner.MeshEntryIndex;

			if (EntityManager.IsEntityValid(SwappedEntity))
			{
				FArcIWInstanceFragment* SwappedFragment =
					EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(SwappedEntity);
				if (SwappedFragment
					&& !SwappedFragment->bIsActorRepresentation
					&& SwappedMeshEntryIndex >= 0
					&& SwappedMeshEntryIndex < SwappedFragment->ISMInstanceIds.Num())
				{
					SwappedFragment->ISMInstanceIds[SwappedMeshEntryIndex] = InstanceId;
				}
			}

			// Move swapped owner into the removed slot
			(*Owners)[InstanceId] = SwappedOwner;
		}

		// Shrink owner array
		if (Owners && Owners->Num() > 0)
		{
			Owners->RemoveAt(LastIndex);
		}

		ISMC->RemoveInstance(InstanceId);
	}
}

// ---------------------------------------------------------------------------
// Cell Tracking
// ---------------------------------------------------------------------------

void UArcIWVisualizationSubsystem::UpdatePlayerCell(const FIntVector& NewCell)
{
	LastPlayerCell = NewCell;
}

bool UArcIWVisualizationSubsystem::IsCellActive(const FIntVector& Cell) const
{
	// If player cell has never been set, nothing is active.
	if (LastPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}

	return FMath::Abs(Cell.X - LastPlayerCell.X) <= SwapRadiusCells
		&& FMath::Abs(Cell.Y - LastPlayerCell.Y) <= SwapRadiusCells
		&& FMath::Abs(Cell.Z - LastPlayerCell.Z) <= SwapRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsActorCell(const FIntVector& Cell) const
{
	if (LastPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}

	return FMath::Abs(Cell.X - LastPlayerCell.X) <= ActorRadiusCells
		&& FMath::Abs(Cell.Y - LastPlayerCell.Y) <= ActorRadiusCells
		&& FMath::Abs(Cell.Z - LastPlayerCell.Z) <= ActorRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsWithinDehydrationRadius(const FIntVector& Cell) const
{
	if (LastPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}

	return FMath::Abs(Cell.X - LastPlayerCell.X) <= DehydrationRadiusCells
		&& FMath::Abs(Cell.Y - LastPlayerCell.Y) <= DehydrationRadiusCells
		&& FMath::Abs(Cell.Z - LastPlayerCell.Z) <= DehydrationRadiusCells;
}

void UArcIWVisualizationSubsystem::GetActiveCells(TArray<FIntVector>& OutCells) const
{
	if (LastPlayerCell.X == TNumericLimits<int32>::Max())
	{
		OutCells.Reset();
		return;
	}

	GetCellsInRadius(LastPlayerCell, SwapRadiusCells, OutCells);
}

void UArcIWVisualizationSubsystem::GetCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells) const
{
	OutCells.Reset();
	for (int32 X = Center.X - RadiusCells; X <= Center.X + RadiusCells; ++X)
	{
		for (int32 Y = Center.Y - RadiusCells; Y <= Center.Y + RadiusCells; ++Y)
		{
			for (int32 Z = Center.Z - RadiusCells; Z <= Center.Z + RadiusCells; ++Z)
			{
				OutCells.Add(FIntVector(X, Y, Z));
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Trace Resolution
// ---------------------------------------------------------------------------

FMassEntityHandle UArcIWVisualizationSubsystem::ResolveHitToEntity(const FHitResult& HitResult)
{
	// MassISM path: resolve via Chaos user data (no component needed)
	FMassEntityHandle PhysicsEntity = ArcMassPhysicsEntityLink::ResolveHitFromPhysicsObject(HitResult);
	if (PhysicsEntity.IsValid())
	{
		if (UArcIWSettings::Get()->bHydrateOnHit)
		{
			HydrateEntity(PhysicsEntity);
		}
		return PhysicsEntity;
	}

	UInstancedStaticMeshComponent* ISMComp = Cast<UInstancedStaticMeshComponent>(HitResult.GetComponent());
	if (!ISMComp)
	{
		return FMassEntityHandle();
	}

	FMassEntityHandle Entity = ResolveISMInstanceToEntity(ISMComp, HitResult.Item);

	if (Entity.IsValid() && UArcIWSettings::Get()->bHydrateOnHit)
	{
		HydrateEntity(Entity);
	}

	return Entity;
}

void UArcIWVisualizationSubsystem::HydrateEntity(FMassEntityHandle EntityHandle)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(EntityHandle))
	{
		return;
	}

	FArcIWInstanceFragment* Instance = EntityManager.GetFragmentDataPtr<FArcIWInstanceFragment>(EntityHandle);
	if (!Instance || Instance->bIsActorRepresentation)
	{
		return;
	}

	const FArcIWVisConfigFragment* Config = EntityManager.GetConstSharedFragmentDataPtr<FArcIWVisConfigFragment>(EntityHandle);
	if (!Config || !Config->ActorClass)
	{
		return;
	}

	const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(EntityHandle);
	if (!TransformFrag)
	{
		return;
	}

	// Remove ISM instances
	bool bHasISM = false;
	for (int32 Id : Instance->ISMInstanceIds)
	{
		if (Id != INDEX_NONE)
		{
			bHasISM = true;
			break;
		}
	}
	if (bHasISM)
	{
		RemoveCompositeISMInstances(EntityHandle, Config->MeshDescriptors, Instance->ISMInstanceIds, EntityManager);
		for (int32& Id : Instance->ISMInstanceIds)
		{
			Id = INDEX_NONE;
		}
	}

	// Acquire actor from pool
	UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
	if (Pool)
	{
		AActor* NewActor = Pool->AcquireActor(Config->ActorClass, TransformFrag->GetTransform(), EntityHandle);
		Instance->HydratedActor = NewActor;
	}

	AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance->PartitionActor.Get());
	if (MassISMPartition)
	{
		MassISMPartition->HydrateEntity(EntityHandle);
	}

	Instance->bIsActorRepresentation = true;
	Instance->bKeepHydrated = true;
}

FMassEntityHandle UArcIWVisualizationSubsystem::ResolveISMInstanceToEntity(
	const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex) const
{
	if (!ISMComponent || InstanceIndex == INDEX_NONE)
	{
		return FMassEntityHandle();
	}

	const FISMComponentInfo* Info = ISMComponentLookup.Find(const_cast<UInstancedStaticMeshComponent*>(ISMComponent));
	if (!Info || !Info->PartitionActor.IsValid())
	{
		return FMassEntityHandle();
	}

	const FPartitionISMData* ISMData = PartitionISMDataMap.Find(Info->PartitionActor);
	if (!ISMData)
	{
		return FMassEntityHandle();
	}

	const TArray<FInstanceOwnerEntry>* Owners = ISMData->InstanceOwners.Find(Info->MeshKey);
	if (!Owners || InstanceIndex < 0 || InstanceIndex >= Owners->Num())
	{
		return FMassEntityHandle();
	}

	return (*Owners)[InstanceIndex].Entity;
}

bool UArcIWVisualizationSubsystem::ResolveISMInstanceToOwner(
	const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex,
	FMassEntityHandle& OutEntity, int32& OutClassIndex, int32& OutEntityIndex) const
{
	if (!ISMComponent || InstanceIndex == INDEX_NONE)
	{
		return false;
	}

	const FISMComponentInfo* Info = ISMComponentLookup.Find(const_cast<UInstancedStaticMeshComponent*>(ISMComponent));
	if (!Info || !Info->PartitionActor.IsValid())
	{
		return false;
	}

	const FPartitionISMData* ISMData = PartitionISMDataMap.Find(Info->PartitionActor);
	if (!ISMData)
	{
		return false;
	}

	const TArray<FInstanceOwnerEntry>* Owners = ISMData->InstanceOwners.Find(Info->MeshKey);
	if (!Owners || InstanceIndex < 0 || InstanceIndex >= Owners->Num())
	{
		return false;
	}

	const FInstanceOwnerEntry& Owner = (*Owners)[InstanceIndex];
	OutEntity = Owner.Entity;
	OutClassIndex = Owner.ClassIndex;
	OutEntityIndex = Owner.EntityIndex;
	return OutEntity.IsValid();
}

// ---------------------------------------------------------------------------
// Private Helpers
// ---------------------------------------------------------------------------

UArcIWVisualizationSubsystem::FPartitionISMData* UArcIWVisualizationSubsystem::FindISMDataForEntity(
	FMassEntityHandle Entity)
{
	TWeakObjectPtr<AActor>* FoundActor = EntityToPartitionActor.Find(Entity);
	if (!FoundActor || !FoundActor->IsValid())
	{
		return nullptr;
	}

	return PartitionISMDataMap.Find(*FoundActor);
}

UInstancedStaticMeshComponent* UArcIWVisualizationSubsystem::GetOrCreateISMComponent(
	FPartitionISMData& Data,
	UStaticMesh* Mesh,
	const TArray<TObjectPtr<UMaterialInterface>>& Materials,
	bool bCastShadows)
{
	AActor* OwnerActor = Data.OwnerActor.Get();
	if (!Mesh || !OwnerActor)
	{
		return nullptr;
	}

	const FObjectKey Key(Mesh);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Existing = Data.ISMComponents.Find(Key))
	{
		return *Existing;
	}

	UInstancedStaticMeshComponent* NewISMC = NewObject<UInstancedStaticMeshComponent>(OwnerActor);
	NewISMC->SetStaticMesh(Mesh);
	NewISMC->SetCastShadow(bCastShadows);
	NewISMC->SetMobility(EComponentMobility::Stationary);
	NewISMC->SetAbsolute(true, true, true);

	for (int32 MatIdx = 0; MatIdx < Materials.Num(); ++MatIdx)
	{
		if (Materials[MatIdx])
		{
			NewISMC->SetMaterial(MatIdx, Materials[MatIdx]);
		}
	}

	NewISMC->RegisterComponent();
	NewISMC->SetWorldTransform(FTransform::Identity);
	OwnerActor->AddInstanceComponent(NewISMC);

	Data.ISMComponents.Add(Key, NewISMC);

	// Register reverse lookup for trace resolution
	FISMComponentInfo Info;
	Info.PartitionActor = OwnerActor;
	Info.MeshKey = Key;
	ISMComponentLookup.Add(NewISMC, Info);

	return NewISMC;
}
