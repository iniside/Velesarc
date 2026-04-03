// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcInstancedWorld.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "ArcIWMassISMPartitionActor.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityConfigAsset.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "MassRenderStateHelper.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ---------------------------------------------------------------------------
// Template Cache Helpers
// ---------------------------------------------------------------------------

namespace ArcIWTemplateCache
{

enum class EVisType : uint8
{
	SimpleVisStatic,
	SimpleVisStaticWithMats,
	SimpleVisSkinned,
	SimpleVisSkinnedWithMats,
	Composite
};

EVisType DetermineVisType(const FArcIWActorClassData& ClassData)
{
	const bool bIsSingleMesh = (ClassData.MeshDescriptors.Num() == 1);
	if (!bIsSingleMesh)
	{
		return EVisType::Composite;
	}

	const FArcIWMeshEntry& Entry = ClassData.MeshDescriptors[0];
	bool bHasOverrideMats = false;
	for (const TObjectPtr<UMaterialInterface>& Mat : Entry.Materials)
	{
		if (Mat) { bHasOverrideMats = true; break; }
	}

	if (Entry.IsSkinned())
	{
		return bHasOverrideMats ? EVisType::SimpleVisSkinnedWithMats : EVisType::SimpleVisSkinned;
	}
	return bHasOverrideMats ? EVisType::SimpleVisStaticWithMats : EVisType::SimpleVisStatic;
}

} // namespace ArcIWTemplateCache

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

	const UArcIWSettings* Settings = UArcIWSettings::Get();

	// Mesh grid
	MeshCellSize = Settings->MeshCellSize;
	MeshAddRadius = Settings->MeshAddRadius;
	MeshAddRadiusCells = FMath::CeilToInt32(MeshAddRadius / MeshCellSize);
	MeshRemoveRadius = Settings->MeshRemoveRadius;
	MeshRemoveRadiusCells = FMath::CeilToInt32(MeshRemoveRadius / MeshCellSize);
	LastMeshPlayerCell = FIntVector(TNumericLimits<int32>::Max());

	// Physics grid
	PhysicsCellSize = Settings->PhysicsCellSize;
	PhysicsAddRadius = Settings->PhysicsAddRadius;
	PhysicsAddRadiusCells = FMath::CeilToInt32(PhysicsAddRadius / PhysicsCellSize);
	PhysicsRemoveRadius = Settings->PhysicsRemoveRadius;
	PhysicsRemoveRadiusCells = FMath::CeilToInt32(PhysicsRemoveRadius / PhysicsCellSize);
	LastPhysicsPlayerCell = FIntVector(TNumericLimits<int32>::Max());

	// Actor grid (conditional)
	bActorGridEnabled = !Settings->bDisableActorHydration;
	ActorCellSize = Settings->ActorCellSize;
	ActorHydrationRadius = Settings->ActorHydrationRadius;
	ActorHydrationRadiusCells = FMath::CeilToInt32(ActorHydrationRadius / ActorCellSize);
	ActorDehydrationRadius = Settings->ActorDehydrationRadius;
	ActorDehydrationRadiusCells = FMath::CeilToInt32(ActorDehydrationRadius / ActorCellSize);
	LastActorPlayerCell = FIntVector(TNumericLimits<int32>::Max());
}

void UArcIWVisualizationSubsystem::Deinitialize()
{
	ISMComponentLookup.Empty();
	PartitionISMDataMap.Empty();
	EntityToPartitionActor.Empty();
	MeshCellEntities.Empty();
	PhysicsCellEntities.Empty();
	ActorCellEntities.Empty();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Domain Grids
// ---------------------------------------------------------------------------

namespace UE::ArcIW::VisGrid
{

FIntVector WorldToCell(const FVector& WorldPos, float InCellSize)
{
	const float InvCellSize = 1.f / InCellSize;
	return FIntVector(
		FMath::FloorToInt(WorldPos.X * InvCellSize),
		FMath::FloorToInt(WorldPos.Y * InvCellSize),
		FMath::FloorToInt(WorldPos.Z * InvCellSize)
	);
}

void RegisterEntity(TMap<FIntVector, TArray<FMassEntityHandle>>& CellMap, FMassEntityHandle Entity, const FVector& Position, float InCellSize)
{
	const FIntVector Cell = WorldToCell(Position, InCellSize);
	CellMap.FindOrAdd(Cell).Add(Entity);
}

void UnregisterEntity(TMap<FIntVector, TArray<FMassEntityHandle>>& CellMap, FMassEntityHandle Entity, const FIntVector& Cell)
{
	if (TArray<FMassEntityHandle>* Entities = CellMap.Find(Cell))
	{
		Entities->RemoveSwap(Entity);
		if (Entities->IsEmpty())
		{
			CellMap.Remove(Cell);
		}
	}
}

} // namespace UE::ArcIW::VisGrid

FIntVector UArcIWVisualizationSubsystem::WorldToMeshCell(const FVector& WorldPos) const
{
	return UE::ArcIW::VisGrid::WorldToCell(WorldPos, MeshCellSize);
}

FIntVector UArcIWVisualizationSubsystem::WorldToPhysicsCell(const FVector& WorldPos) const
{
	return UE::ArcIW::VisGrid::WorldToCell(WorldPos, PhysicsCellSize);
}

FIntVector UArcIWVisualizationSubsystem::WorldToActorCell(const FVector& WorldPos) const
{
	return UE::ArcIW::VisGrid::WorldToCell(WorldPos, ActorCellSize);
}

void UArcIWVisualizationSubsystem::RegisterMeshEntity(FMassEntityHandle Entity, const FVector& Position)
{
	UE::ArcIW::VisGrid::RegisterEntity(MeshCellEntities, Entity, Position, MeshCellSize);
}

void UArcIWVisualizationSubsystem::UnregisterMeshEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	UE::ArcIW::VisGrid::UnregisterEntity(MeshCellEntities, Entity, Cell);
}

const TArray<FMassEntityHandle>* UArcIWVisualizationSubsystem::GetMeshEntitiesInCell(const FIntVector& Cell) const
{
	return MeshCellEntities.Find(Cell);
}

void UArcIWVisualizationSubsystem::RegisterPhysicsEntity(FMassEntityHandle Entity, const FVector& Position)
{
	UE::ArcIW::VisGrid::RegisterEntity(PhysicsCellEntities, Entity, Position, PhysicsCellSize);
}

void UArcIWVisualizationSubsystem::UnregisterPhysicsEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	UE::ArcIW::VisGrid::UnregisterEntity(PhysicsCellEntities, Entity, Cell);
}

const TArray<FMassEntityHandle>* UArcIWVisualizationSubsystem::GetPhysicsEntitiesInCell(const FIntVector& Cell) const
{
	return PhysicsCellEntities.Find(Cell);
}

void UArcIWVisualizationSubsystem::RegisterActorEntity(FMassEntityHandle Entity, const FVector& Position)
{
	if (!bActorGridEnabled)
	{
		return;
	}
	UE::ArcIW::VisGrid::RegisterEntity(ActorCellEntities, Entity, Position, ActorCellSize);
}

void UArcIWVisualizationSubsystem::UnregisterActorEntity(FMassEntityHandle Entity, const FIntVector& Cell)
{
	if (!bActorGridEnabled)
	{
		return;
	}
	UE::ArcIW::VisGrid::UnregisterEntity(ActorCellEntities, Entity, Cell);
}

const TArray<FMassEntityHandle>* UArcIWVisualizationSubsystem::GetActorEntitiesInCell(const FIntVector& Cell) const
{
	return ActorCellEntities.Find(Cell);
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
		UE_LOG(LogArcIW, Warning, TEXT("AddCompositeISMInstances: no ISMData for entity %d (EntityToPartitionActor=%d, PartitionISMDataMap=%d)"),
			EntityHandle.Index, EntityToPartitionActor.Num(), PartitionISMDataMap.Num());
		for (int32& Id : InOutISMInstanceIds)
		{
			Id = INDEX_NONE;
		}
		return;
	}
	UE_LOG(LogArcIW, Verbose, TEXT("AddCompositeISMInstances: entity %d, %d meshes, owner %s"),
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

void UArcIWVisualizationSubsystem::UpdateMeshPlayerCell(const FIntVector& NewCell)
{
	LastMeshPlayerCell = NewCell;
}

void UArcIWVisualizationSubsystem::UpdatePhysicsPlayerCell(const FIntVector& NewCell)
{
	LastPhysicsPlayerCell = NewCell;
}

void UArcIWVisualizationSubsystem::UpdateActorPlayerCell(const FIntVector& NewCell)
{
	LastActorPlayerCell = NewCell;
}

bool UArcIWVisualizationSubsystem::IsMeshCell(const FIntVector& MeshCell) const
{
	if (LastMeshPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = MeshCell - LastMeshPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= MeshAddRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsWithinMeshRemoveRadius(const FIntVector& MeshCell) const
{
	if (LastMeshPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = MeshCell - LastMeshPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= MeshRemoveRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsPhysicsCell(const FIntVector& PhysicsCell) const
{
	if (LastPhysicsPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = PhysicsCell - LastPhysicsPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= PhysicsAddRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsWithinPhysicsRemoveRadius(const FIntVector& PhysicsCell) const
{
	if (LastPhysicsPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = PhysicsCell - LastPhysicsPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= PhysicsRemoveRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsActorCell(const FIntVector& ActorCell) const
{
	if (!bActorGridEnabled || LastActorPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = ActorCell - LastActorPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= ActorHydrationRadiusCells;
}

bool UArcIWVisualizationSubsystem::IsWithinDehydrationRadius(const FIntVector& ActorCell) const
{
	if (!bActorGridEnabled || LastActorPlayerCell.X == TNumericLimits<int32>::Max())
	{
		return false;
	}
	FIntVector Delta = ActorCell - LastActorPlayerCell;
	int32 MaxDist = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	return MaxDist <= ActorDehydrationRadiusCells;
}

void UArcIWVisualizationSubsystem::GetCellsInRadius(const FIntVector& Center, int32 RadiusCells, TArray<FIntVector>& OutCells)
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

	// Signal physics body release
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntity(UE::ArcMass::Signals::PhysicsBodyReleased, EntityHandle);
		}
	}

	// Acquire actor from pool
	UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
	if (Pool)
	{
		AActor* NewActor = Pool->AcquireActor(Config->ActorClass, TransformFrag->GetTransform(), EntityHandle);
		Instance->HydratedActor = NewActor;
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

// ---------------------------------------------------------------------------
// Entity Template Cache
// ---------------------------------------------------------------------------

const UArcIWVisualizationSubsystem::FCachedEntityTemplate& UArcIWVisualizationSubsystem::EnsureEntityTemplate(
	const FArcIWActorClassData& ClassData, UWorld& World)
{
	using namespace ArcIWTemplateCache;

	UClass* ActorClass = ClassData.ActorClass.Get();
	check(ActorClass);

	const EVisType VisType = DetermineVisType(ClassData);

	// Select cache map by vis type
	TMap<TObjectPtr<UClass>, FCachedEntityTemplate>* CacheMap = nullptr;
	switch (VisType)
	{
	case EVisType::SimpleVisStatic:         CacheMap = &SimpleVisStaticTemplates; break;
	case EVisType::SimpleVisStaticWithMats: CacheMap = &SimpleVisStaticWithMatsTemplates; break;
	case EVisType::SimpleVisSkinned:        CacheMap = &SimpleVisSkinnedTemplates; break;
	case EVisType::SimpleVisSkinnedWithMats:CacheMap = &SimpleVisSkinnedWithMatsTemplates; break;
	case EVisType::Composite:               CacheMap = &CompositeVisTemplates; break;
	}

	// Cache hit
	FCachedEntityTemplate* Existing = CacheMap->Find(ActorClass);
	if (Existing)
	{
		return *Existing;
	}

	// Cache miss — build template
	FMassEntityTemplateData TemplateData;

	// Merge gameplay config if present
	if (ClassData.AdditionalEntityConfig != nullptr)
	{
		FMassEntityTemplateData* CachedGameplay = GameplayTemplateData.Find(ActorClass);
		if (CachedGameplay)
		{
			TemplateData = *CachedGameplay;
		}
		else
		{
			FMassEntityTemplateBuildContext BuildContext(TemplateData);
			const FMassEntityConfig& Config = ClassData.AdditionalEntityConfig->GetConfig();
			BuildContext.BuildFromTraits(Config.GetTraits(), World);
			GameplayTemplateData.Add(ActorClass, TemplateData);
		}
	}

	// Common fragments
	TemplateData.AddFragment<FArcIWInstanceFragment>();
	TemplateData.AddFragment<FTransformFragment>();
	TemplateData.AddFragment<FArcMassPhysicsBodyFragment>();

	// Const shared: vis config (always present)
	FArcIWVisConfigFragment ConfigFragment;
	ConfigFragment.ActorClass = ClassData.ActorClass;
	ConfigFragment.MeshDescriptors = ClassData.MeshDescriptors;
	TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(ConfigFragment));

	if (ClassData.CollisionBodySetup)
	{
		FArcMassPhysicsBodyConfigFragment PhysicsConfig;
		PhysicsConfig.BodySetup = ClassData.CollisionBodySetup;
		PhysicsConfig.BodyTemplate = ClassData.BodyTemplate;
		PhysicsConfig.BodyType = ClassData.PhysicsBodyType;
		TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(PhysicsConfig));
	}

	// Vis-type-specific tags and shared fragments + holder archetype
	FMassArchetypeHandle HolderArchetype;
	UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
	check(EntitySubsystem);
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	switch (VisType)
	{
	case EVisType::SimpleVisStatic:
	case EVisType::SimpleVisStaticWithMats:
	{
		TemplateData.AddTag<FArcIWSimpleVisEntityTag>();

		if (VisType == EVisType::SimpleVisStaticWithMats)
		{
			FMassOverrideMaterialsFragment OverrideMats;
			OverrideMats.OverrideMaterials = ClassData.MeshDescriptors[0].Materials;
			TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(OverrideMats));

			FMassElementBitSet Composition;
			Composition.Add<FTransformFragment>();
			Composition.Add<FMassRenderStateFragment>();
			Composition.Add<FMassRenderPrimitiveFragment>();
			Composition.Add<FMassRenderISMFragment>();
			Composition.Add<FMassOverrideMaterialsFragment>();
			HolderArchetype = EntityManager.CreateArchetype(Composition,
				FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolderMats")});
		}
		else
		{
			FMassElementBitSet Composition;
			Composition.Add<FTransformFragment>();
			Composition.Add<FMassRenderStateFragment>();
			Composition.Add<FMassRenderPrimitiveFragment>();
			Composition.Add<FMassRenderISMFragment>();
			HolderArchetype = EntityManager.CreateArchetype(Composition,
				FMassArchetypeCreationParams{TEXT("ArcIWMassISMHolder")});
		}
		break;
	}
	case EVisType::SimpleVisSkinned:
	case EVisType::SimpleVisSkinnedWithMats:
	{
		TemplateData.AddTag<FArcIWSimpleVisSkinnedTag>();

		FArcMassSkinnedMeshFragment SkinnedMeshFrag;
		SkinnedMeshFrag.SkinnedAsset = ClassData.MeshDescriptors[0].SkinnedAsset;
		SkinnedMeshFrag.TransformProvider = ClassData.MeshDescriptors[0].TransformProvider;
		TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(SkinnedMeshFrag));

		FArcMassVisualizationMeshConfigFragment VisMeshConfigFrag;
		VisMeshConfigFrag.CastShadow = ClassData.MeshDescriptors[0].bCastShadows;
		TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(VisMeshConfigFrag));

		if (VisType == EVisType::SimpleVisSkinnedWithMats)
		{
			FMassOverrideMaterialsFragment OverrideMats;
			OverrideMats.OverrideMaterials = ClassData.MeshDescriptors[0].Materials;
			TemplateData.AddConstSharedFragment(FConstSharedStruct::Make(OverrideMats));

			FMassElementBitSet Composition;
			Composition.Add<FTransformFragment>();
			Composition.Add<FArcMassRenderISMSkinnedFragment>();
			Composition.Add<FMassRenderPrimitiveFragment>();
			Composition.Add<FMassOverrideMaterialsFragment>();
			HolderArchetype = EntityManager.CreateArchetype(Composition,
				FMassArchetypeCreationParams{TEXT("ArcIWSkinnedISMHolderMats")});
		}
		else
		{
			FMassElementBitSet Composition;
			Composition.Add<FTransformFragment>();
			Composition.Add<FArcMassRenderISMSkinnedFragment>();
			Composition.Add<FMassRenderPrimitiveFragment>();
			HolderArchetype = EntityManager.CreateArchetype(Composition,
				FMassArchetypeCreationParams{TEXT("ArcIWSkinnedISMHolder")});
		}
		break;
	}
	case EVisType::Composite:
	{
		TemplateData.AddTag<FArcIWEntityTag>();
		// Composite uses per-mesh-slot holders resolved at activation time.
		// HolderArchetype left invalid.
		break;
	}
	}

	// Register with spawner subsystem
	FGuid ClassGuid = FGuid::NewDeterministicGuid(ActorClass->GetPathName());
	FMassEntityTemplateID TemplateID = FMassEntityTemplateIDFactory::Make(ClassGuid);

	UMassSpawnerSubsystem* SpawnerSubsystem = World.GetSubsystem<UMassSpawnerSubsystem>();
	check(SpawnerSubsystem);
	FMassEntityTemplateRegistry& TemplateRegistry = SpawnerSubsystem->GetMutableTemplateRegistryInstance();
	const TSharedRef<FMassEntityTemplate>& FinalTemplate = TemplateRegistry.FindOrAddTemplate(TemplateID, MoveTemp(TemplateData));

	FCachedEntityTemplate& NewEntry = CacheMap->Add(ActorClass,
		FCachedEntityTemplate{ FinalTemplate, HolderArchetype });
	return NewEntry;
}
