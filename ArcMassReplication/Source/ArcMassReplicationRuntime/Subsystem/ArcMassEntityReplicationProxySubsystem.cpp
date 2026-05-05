// Copyright Lukasz Baran. All Rights Reserved.

#include "Subsystem/ArcMassEntityReplicationProxySubsystem.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "NetSerializers/ArcMassEntityNetDataStore.h"
#include "NetSerializers/ArcIrisReplicatedArrayNetSerializer.h"
#include "Spatial/ArcMassSpatialGrid.h"
#include "Engine/NetDriver.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassEntityUtils.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"

void UArcMassEntityReplicationProxySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UNetDriver* NetDriver = World->GetNetDriver();
	if (NetDriver != nullptr && NetDriver->GetNetTokenStore() != nullptr)
	{
		RegisterNetDataStore(NetDriver);
	}
	else if (NetDriver != nullptr)
	{
		NetDriver->OnNetTokenStoreReady().AddUObject(this, &UArcMassEntityReplicationProxySubsystem::RegisterNetDataStore);
	}

}

void UArcMassEntityReplicationProxySubsystem::Deinitialize()
{
	UWorld* World = GetWorld();
	if (World)
	{
		UNetDriver* NetDriver = World->GetNetDriver();
		if (NetDriver != nullptr)
		{
			NetDriver->OnNetTokenStoreReady().RemoveAll(this);

			UE::Net::FNetTokenStore* TokenStore = NetDriver->GetNetTokenStore();
			if (TokenStore != nullptr)
			{
				TokenStore->UnRegisterDataStore(ArcMassReplication::FArcMassEntityNetDataStore::GetTokenStoreName());
			}
		}
	}

	for (TObjectPtr<AArcMassEntityReplicationProxy>& Proxy : OwnedProxies)
	{
		if (Proxy)
		{
			Proxy->Destroy();
		}
	}
	OwnedProxies.Empty();
	ArchetypeToProxies.Empty();
	ArchetypeGrids.Empty();
	DescriptorSets.Empty();
	NetIdToEntity.Empty();
	EntityToNetId.Empty();
	EntityToArchetypeKey.Empty();
	DirtyEntities.Empty();
	EntityOwnerConnectionId.Empty();
	SourceStates.Empty();
	ConnectionRelevantCells.Empty();
	DescriptorSetsByHash.Empty();
	Super::Deinitialize();
}

void UArcMassEntityReplicationProxySubsystem::RegisterNetDataStore(UNetDriver* NetDriver)
{
	if (bNetDataStoreRegistered || NetDriver == nullptr)
	{
		return;
	}

	UE::Net::FNetTokenStore* TokenStore = NetDriver->GetNetTokenStore();
	if (TokenStore == nullptr)
	{
		return;
	}

	if (TokenStore->GetDataStore<ArcMassReplication::FArcMassEntityNetDataStore>() == nullptr)
	{
		TokenStore->RegisterDataStore(
			MakeUnique<ArcMassReplication::FArcMassEntityNetDataStore>(*TokenStore, this),
			ArcMassReplication::FArcMassEntityNetDataStore::GetTokenStoreName());
	}

	bNetDataStoreRegistered = true;
}

void UArcMassEntityReplicationProxySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	if (bNetDataStoreRegistered)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UNetDriver* NetDriver = World->GetNetDriver();
	if (NetDriver != nullptr && NetDriver->GetNetTokenStore() != nullptr)
	{
		RegisterNetDataStore(NetDriver);
	}
	else if (NetDriver != nullptr)
	{
		NetDriver->OnNetTokenStoreReady().AddUObject(this, &UArcMassEntityReplicationProxySubsystem::RegisterNetDataStore);
	}
}

FArcMassNetId UArcMassEntityReplicationProxySubsystem::AllocateNetId()
{
	return FArcMassNetId(NextNetIdValue++);
}

AArcMassEntityReplicationProxy* UArcMassEntityReplicationProxySubsystem::GetOrCreateProxy(
	const TArray<const UScriptStruct*>& FragmentTypes,
	float CullDistance,
	UMassEntityConfigAsset* InEntityConfigAsset)
{
	FArchetypeKey Key;
	Key.SortedTypes = FragmentTypes;
	Algo::Sort(Key.SortedTypes, [](const UScriptStruct* A, const UScriptStruct* B) { return A->GetFName().LexicalLess(B->GetFName()); });

	TArray<TObjectPtr<AArcMassEntityReplicationProxy>>& Proxies = ArchetypeToProxies.FindOrAdd(Key);

	if (Proxies.Num() > 0)
	{
		AArcMassEntityReplicationProxy* Last = Proxies.Last();
		if (Last->GetItemCount() < MaxEntitiesPerProxy)
		{
			return Last;
		}
	}

	UWorld* World = GetWorld();
	AArcMassEntityReplicationProxy* Proxy = World->SpawnActor<AArcMassEntityReplicationProxy>();
	Proxy->Init(InEntityConfigAsset);

	Proxies.Add(Proxy);
	OwnedProxies.Add(Proxy);
	return Proxy;
}

void UArcMassEntityReplicationProxySubsystem::RegisterEntityNetId(FArcMassNetId NetId, FMassEntityHandle Entity)
{
	NetIdToEntity.Add(NetId, Entity);
	EntityToNetId.Add(Entity, NetId);
}

void UArcMassEntityReplicationProxySubsystem::UnregisterEntityNetId(FArcMassNetId NetId)
{
	FMassEntityHandle Entity;
	if (NetIdToEntity.RemoveAndCopyValue(NetId, Entity))
	{
		EntityToNetId.Remove(Entity);
		EntityToArchetypeKey.Remove(Entity);
	}
}

FMassEntityHandle UArcMassEntityReplicationProxySubsystem::FindEntityByNetId(FArcMassNetId NetId) const
{
	const FMassEntityHandle* Found = NetIdToEntity.Find(NetId);
	return Found ? *Found : FMassEntityHandle();
}

FArcMassNetId UArcMassEntityReplicationProxySubsystem::FindNetId(FMassEntityHandle Entity) const
{
	const FArcMassNetId* Found = EntityToNetId.Find(Entity);
	return Found ? *Found : FArcMassNetId();
}

void UArcMassEntityReplicationProxySubsystem::MarkEntityDirty(FMassEntityHandle Entity)
{
	DirtyEntities.Add(Entity);
}

TSet<FMassEntityHandle> UArcMassEntityReplicationProxySubsystem::TakeDirtyEntities()
{
	TSet<FMassEntityHandle> Result = MoveTemp(DirtyEntities);
	DirtyEntities.Reset();
	return Result;
}

AArcMassEntityReplicationProxy* UArcMassEntityReplicationProxySubsystem::FindProxyForEntity(FMassEntityHandle Entity) const
{
	const FArchetypeKey* Key = EntityToArchetypeKey.Find(Entity);
	if (!Key)
	{
		return nullptr;
	}

	const FArcMassNetId* NetId = EntityToNetId.Find(Entity);
	if (!NetId)
	{
		return nullptr;
	}

	const TArray<TObjectPtr<AArcMassEntityReplicationProxy>>* Proxies = ArchetypeToProxies.Find(*Key);
	if (!Proxies)
	{
		return nullptr;
	}

	for (const TObjectPtr<AArcMassEntityReplicationProxy>& Proxy : *Proxies)
	{
		if (Proxy && Proxy->FindItemIndexByNetId(*NetId) != INDEX_NONE)
		{
			return Proxy;
		}
	}

	return nullptr;
}

void UArcMassEntityReplicationProxySubsystem::RegisterDescriptorSet(uint32 InHash, const ArcMassReplication::FArcMassReplicationDescriptorSet* InDescriptorSet)
{
	if (InHash != 0 && InDescriptorSet != nullptr)
	{
		DescriptorSetsByHash.Add(InHash, InDescriptorSet);
	}
}

const ArcMassReplication::FArcMassReplicationDescriptorSet* UArcMassEntityReplicationProxySubsystem::FindDescriptorSetByHash(uint32 InHash) const
{
	const ArcMassReplication::FArcMassReplicationDescriptorSet* const* Found = DescriptorSetsByHash.Find(InHash);
	return Found ? *Found : nullptr;
}

void UArcMassEntityReplicationProxySubsystem::OnClientEntityAdded(
	FArcMassNetId NetId,
	const TArray<FInstancedStruct>& FragmentSlots,
	const ArcMassReplication::FArcMassReplicationDescriptorSet* DescriptorSet,
	UMassEntityConfigAsset* InEntityConfigAsset)
{
	if (!DescriptorSet || DescriptorSet->FragmentTypes.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager* EntityManager = UE::Mass::Utils::GetEntityManager(World);
	if (!EntityManager)
	{
		return;
	}

	FMassEntityHandle Entity;
	TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext;

	if (InEntityConfigAsset)
	{
		const FMassEntityTemplate& EntityTemplate = InEntityConfigAsset->GetOrCreateEntityTemplate(*World);

		UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
		if (SpawnerSubsystem)
		{
			TArray<FMassEntityHandle> SpawnedEntities;
			CreationContext = SpawnerSubsystem->SpawnEntities(EntityTemplate, 1, SpawnedEntities);
			if (SpawnedEntities.Num() > 0)
			{
				Entity = SpawnedEntities[0];
			}
		}

		if (Entity.IsSet())
		{
			for (const UScriptStruct* FragType : DescriptorSet->FragmentTypes)
			{
				if (UE::Mass::IsSparse(FragType))
				{
					(void)EntityManager->AddSparseElementToEntity(Entity, FragType);
				}
			}
		}
	}
	else
	{
		FMassFragmentBitSet FragmentBitSet;
		TArray<const UScriptStruct*> SparseFragmentTypes;
		for (const UScriptStruct* FragType : DescriptorSet->FragmentTypes)
		{
			if (UE::Mass::IsSparse(FragType))
			{
				SparseFragmentTypes.Add(FragType);
			}
			else
			{
				FragmentBitSet.Add(FragType);
			}
		}

		FMassArchetypeHandle Archetype = EntityManager->CreateArchetype(FragmentBitSet, {});
		TArray<FMassEntityHandle> SpawnedEntities;
		CreationContext = EntityManager->BatchCreateEntities(Archetype, 1, SpawnedEntities);

		if (SpawnedEntities.Num() > 0)
		{
			Entity = SpawnedEntities[0];
		}

		for (const UScriptStruct* SparseType : SparseFragmentTypes)
		{
			(void)EntityManager->AddSparseElementToEntity(Entity, SparseType);
		}
	}

	if (!Entity.IsSet())
	{
		return;
	}

	RegisterEntityNetId(NetId, Entity);

	FMassEntityView EntityView(*EntityManager, Entity);
	for (int32 SlotIdx = 0; SlotIdx < FragmentSlots.Num() && SlotIdx < DescriptorSet->FragmentTypes.Num(); ++SlotIdx)
	{
		const FInstancedStruct& Slot = FragmentSlots[SlotIdx];
		if (!Slot.IsValid())
		{
			continue;
		}

		const UScriptStruct* StructType = DescriptorSet->FragmentTypes[SlotIdx];
		uint8* DstMemory = nullptr;
		if (UE::Mass::IsSparse(StructType))
		{
			FStructView SparseView = EntityManager->GetMutableSparseElementDataForEntity(StructType, Entity);
			DstMemory = SparseView.GetMemory();
		}
		else
		{
			FStructView FragmentView = EntityView.GetFragmentDataStruct(StructType);
			DstMemory = FragmentView.GetMemory();
		}

		if (DstMemory)
		{
			const UE::Net::FReplicationStateDescriptor* SlotDesc =
				(DescriptorSet->Descriptors.IsValidIndex(SlotIdx) && DescriptorSet->Descriptors[SlotIdx].IsValid())
					? DescriptorSet->Descriptors[SlotIdx].GetReference()
					: nullptr;
			ArcMassReplication::ApplyReplicatedFragmentToTarget(DstMemory, Slot.GetMemory(), StructType, SlotDesc);
		}
	}
}

void UArcMassEntityReplicationProxySubsystem::OnClientEntityChanged(
	FArcMassNetId NetId,
	const TArray<FInstancedStruct>& FragmentSlots,
	uint32 ChangedFragmentMask,
	const ArcMassReplication::FArcMassReplicationDescriptorSet* DescriptorSet)
{
	if (!DescriptorSet)
	{
		return;
	}

	FMassEntityHandle Entity = FindEntityByNetId(NetId);
	if (!Entity.IsSet())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	FMassEntityView EntityView(EntityManager, Entity);

	for (int32 SlotIdx = 0; SlotIdx < FragmentSlots.Num() && SlotIdx < DescriptorSet->FragmentTypes.Num(); ++SlotIdx)
	{
		if ((ChangedFragmentMask & (1U << SlotIdx)) == 0)
		{
			continue;
		}

		const FInstancedStruct& Slot = FragmentSlots[SlotIdx];
		if (!Slot.IsValid())
		{
			continue;
		}

		const UScriptStruct* StructType = DescriptorSet->FragmentTypes[SlotIdx];
		uint8* DstMemory = nullptr;
		if (UE::Mass::IsSparse(StructType))
		{
			FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(StructType, Entity);
			DstMemory = SparseView.GetMemory();
		}
		else
		{
			FStructView FragmentView = EntityView.GetFragmentDataStruct(StructType);
			DstMemory = FragmentView.GetMemory();
		}

		if (DstMemory)
		{
			const UE::Net::FReplicationStateDescriptor* SlotDesc =
				(DescriptorSet->Descriptors.IsValidIndex(SlotIdx) && DescriptorSet->Descriptors[SlotIdx].IsValid())
					? DescriptorSet->Descriptors[SlotIdx].GetReference()
					: nullptr;
			ArcMassReplication::ApplyReplicatedFragmentToTarget(DstMemory, Slot.GetMemory(), StructType, SlotDesc);
		}
	}
}

void UArcMassEntityReplicationProxySubsystem::OnClientEntityRemoved(FArcMassNetId NetId)
{
	FMassEntityHandle Entity = FindEntityByNetId(NetId);
	if (!Entity.IsSet())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
		EntityManager.DestroyEntity(Entity);
	}

	UnregisterEntityNetId(NetId);
}

// --- Task 9: Grid management ---

FArcMassSpatialGrid* UArcMassEntityReplicationProxySubsystem::GetOrCreateGrid(const FArchetypeKey& Key, float CellSize)
{
	FArcMassSpatialGrid* Existing = ArchetypeGrids.Find(Key);
	if (Existing)
	{
		return Existing;
	}

	FArcMassSpatialGrid& NewGrid = ArchetypeGrids.Add(Key);
	NewGrid.CellSize = CellSize;
	return &NewGrid;
}

FArcMassSpatialGrid* UArcMassEntityReplicationProxySubsystem::FindGrid(const FArchetypeKey& Key)
{
	return ArchetypeGrids.Find(Key);
}

void UArcMassEntityReplicationProxySubsystem::AddEntityToGrid(FMassEntityHandle Entity, FVector Position)
{
	const FArcMassNetId* NetId = EntityToNetId.Find(Entity);
	const FArchetypeKey* Key = EntityToArchetypeKey.Find(Entity);
	if (!NetId || !Key)
	{
		return;
	}

	FArcMassSpatialGrid* Grid = ArchetypeGrids.Find(*Key);
	if (Grid)
	{
		Grid->AddEntity(*NetId, Position);
	}
}

void UArcMassEntityReplicationProxySubsystem::UpdateEntityInGrid(FMassEntityHandle Entity, FVector Position)
{
	const FArcMassNetId* NetId = EntityToNetId.Find(Entity);
	const FArchetypeKey* Key = EntityToArchetypeKey.Find(Entity);
	if (!NetId || !Key)
	{
		return;
	}

	FArcMassSpatialGrid* Grid = ArchetypeGrids.Find(*Key);
	if (Grid)
	{
		Grid->UpdateEntity(*NetId, Position);
	}
}

void UArcMassEntityReplicationProxySubsystem::RemoveEntityFromGrid(FMassEntityHandle Entity)
{
	const FArcMassNetId* NetId = EntityToNetId.Find(Entity);
	const FArchetypeKey* Key = EntityToArchetypeKey.Find(Entity);
	if (!NetId || !Key)
	{
		return;
	}

	FArcMassSpatialGrid* Grid = ArchetypeGrids.Find(*Key);
	if (Grid)
	{
		Grid->RemoveEntity(*NetId);
	}
}

void UArcMassEntityReplicationProxySubsystem::RegisterEntityArchetypeKey(FMassEntityHandle Entity, const FArchetypeKey& Key)
{
	EntityToArchetypeKey.Add(Entity, Key);
}

// --- Task 10: Tickable subsystem + player cell tracking ---

TStatId UArcMassEntityReplicationProxySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcMassEntityReplicationProxySubsystem, STATGROUP_Game);
}

void UArcMassEntityReplicationProxySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FVector UArcMassEntityReplicationProxySubsystem::GetPlayerPosition(uint32 ConnectionId) const
{
	const FSourceState* State = SourceStates.Find(ConnectionId);
	return State ? State->Position : FVector::ZeroVector;
}

bool UArcMassEntityReplicationProxySubsystem::IsEntityRelevantToConnection(FArcMassNetId NetId, uint32 ConnectionId) const
{
	FMassEntityHandle Entity = FindEntityByNetId(NetId);
	if (!Entity.IsSet())
	{
		return true;
	}

	const FArchetypeKey* Key = EntityToArchetypeKey.Find(Entity);
	if (!Key)
	{
		return true;
	}

	const FArcMassSpatialGrid* Grid = ArchetypeGrids.Find(*Key);
	if (!Grid)
	{
		return true;
	}

	const FIntVector2* EntityCell = Grid->EntityToCell.Find(NetId);
	if (!EntityCell)
	{
		return true;
	}

	return IsCellRelevantForConnection(ConnectionId, *EntityCell);
}

bool UArcMassEntityReplicationProxySubsystem::IsConnectionOwnerOf(FArcMassNetId NetId, uint32 ConnectionId) const
{
	const uint32* Owner = EntityOwnerConnectionId.Find(NetId);
	return Owner == nullptr || *Owner == ConnectionId;
}

void UArcMassEntityReplicationProxySubsystem::SetEntityOwner(FArcMassNetId NetId, uint32 ConnectionId)
{
	EntityOwnerConnectionId.Add(NetId, ConnectionId);
}

void UArcMassEntityReplicationProxySubsystem::UpdateSourceState(uint32 ConnectionId, FVector Position, FIntVector2 Cell)
{
	FSourceState& State = SourceStates.FindOrAdd(ConnectionId);
	State.Position = Position;
	State.Cell = Cell;
}

const UArcMassEntityReplicationProxySubsystem::FSourceState* UArcMassEntityReplicationProxySubsystem::GetSourceState(uint32 ConnectionId) const
{
	return SourceStates.Find(ConnectionId);
}

TArray<uint32> UArcMassEntityReplicationProxySubsystem::GetAllSourceConnectionIds() const
{
	TArray<uint32> Result;
	SourceStates.GetKeys(Result);
	return Result;
}

void UArcMassEntityReplicationProxySubsystem::SetCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell)
{
	ConnectionRelevantCells.FindOrAdd(ConnectionId).Add(Cell);
}

void UArcMassEntityReplicationProxySubsystem::SetCellIrrelevantForConnection(uint32 ConnectionId, FIntVector2 Cell)
{
	TSet<FIntVector2>* Cells = ConnectionRelevantCells.Find(ConnectionId);
	if (Cells)
	{
		Cells->Remove(Cell);
	}
}

bool UArcMassEntityReplicationProxySubsystem::IsCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell) const
{
	const TSet<FIntVector2>* Cells = ConnectionRelevantCells.Find(ConnectionId);
	return Cells && Cells->Contains(Cell);
}
