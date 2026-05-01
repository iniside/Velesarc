// Copyright Lukasz Baran. All Rights Reserved.

#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "NetSerializers/ArcMassEntityNetDataStore.h"
#include "Spatial/ArcMassSpatialGrid.h"
#include "Engine/NetDriver.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
#include "MassEntityUtils.h"
#include "Mass/EntityElementTypes.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"

void UArcMassEntityReplicationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
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
		NetDriver->OnNetTokenStoreReady().AddUObject(this, &UArcMassEntityReplicationSubsystem::RegisterNetDataStore);
	}

	if (!bNetDataStoreRegistered)
	{
		World->OnWorldBeginPlay.AddUObject(this, &UArcMassEntityReplicationSubsystem::OnWorldBeginPlay);
	}
}

void UArcMassEntityReplicationSubsystem::Deinitialize()
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

void UArcMassEntityReplicationSubsystem::RegisterNetDataStore(UNetDriver* NetDriver)
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

void UArcMassEntityReplicationSubsystem::OnWorldBeginPlay()
{
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
		NetDriver->OnNetTokenStoreReady().AddUObject(this, &UArcMassEntityReplicationSubsystem::RegisterNetDataStore);
	}
}

FArcMassNetId UArcMassEntityReplicationSubsystem::AllocateNetId()
{
	return FArcMassNetId(NextNetIdValue++);
}

AArcMassEntityReplicationProxy* UArcMassEntityReplicationSubsystem::GetOrCreateProxy(
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

void UArcMassEntityReplicationSubsystem::RegisterEntityNetId(FArcMassNetId NetId, FMassEntityHandle Entity)
{
	NetIdToEntity.Add(NetId, Entity);
	EntityToNetId.Add(Entity, NetId);
}

void UArcMassEntityReplicationSubsystem::UnregisterEntityNetId(FArcMassNetId NetId)
{
	FMassEntityHandle Entity;
	if (NetIdToEntity.RemoveAndCopyValue(NetId, Entity))
	{
		EntityToNetId.Remove(Entity);
		EntityToArchetypeKey.Remove(Entity);
	}
}

FMassEntityHandle UArcMassEntityReplicationSubsystem::FindEntityByNetId(FArcMassNetId NetId) const
{
	const FMassEntityHandle* Found = NetIdToEntity.Find(NetId);
	return Found ? *Found : FMassEntityHandle();
}

FArcMassNetId UArcMassEntityReplicationSubsystem::FindNetId(FMassEntityHandle Entity) const
{
	const FArcMassNetId* Found = EntityToNetId.Find(Entity);
	return Found ? *Found : FArcMassNetId();
}

void UArcMassEntityReplicationSubsystem::MarkEntityDirty(FMassEntityHandle Entity)
{
	DirtyEntities.Add(Entity);
}

TSet<FMassEntityHandle> UArcMassEntityReplicationSubsystem::TakeDirtyEntities()
{
	TSet<FMassEntityHandle> Result = MoveTemp(DirtyEntities);
	DirtyEntities.Reset();
	return Result;
}

AArcMassEntityReplicationProxy* UArcMassEntityReplicationSubsystem::FindProxyForEntity(FMassEntityHandle Entity) const
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

void UArcMassEntityReplicationSubsystem::RegisterDescriptorSet(uint32 InHash, const ArcMassReplication::FArcMassReplicationDescriptorSet* InDescriptorSet)
{
	if (InHash != 0 && InDescriptorSet != nullptr)
	{
		DescriptorSetsByHash.Add(InHash, InDescriptorSet);
	}
}

const ArcMassReplication::FArcMassReplicationDescriptorSet* UArcMassEntityReplicationSubsystem::FindDescriptorSetByHash(uint32 InHash) const
{
	const ArcMassReplication::FArcMassReplicationDescriptorSet* const* Found = DescriptorSetsByHash.Find(InHash);
	return Found ? *Found : nullptr;
}

void UArcMassEntityReplicationSubsystem::OnClientEntityAdded(
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
				FragmentBitSet.Add(*FragType);
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
			StructType->CopyScriptStruct(DstMemory, Slot.GetMemory());
		}
	}
}

void UArcMassEntityReplicationSubsystem::OnClientEntityChanged(
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
			StructType->CopyScriptStruct(DstMemory, Slot.GetMemory());
		}
	}
}

void UArcMassEntityReplicationSubsystem::OnClientEntityRemoved(FArcMassNetId NetId)
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

FArcMassSpatialGrid* UArcMassEntityReplicationSubsystem::GetOrCreateGrid(const FArchetypeKey& Key, float CellSize)
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

FArcMassSpatialGrid* UArcMassEntityReplicationSubsystem::FindGrid(const FArchetypeKey& Key)
{
	return ArchetypeGrids.Find(Key);
}

void UArcMassEntityReplicationSubsystem::AddEntityToGrid(FMassEntityHandle Entity, FVector Position)
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

void UArcMassEntityReplicationSubsystem::UpdateEntityInGrid(FMassEntityHandle Entity, FVector Position)
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

void UArcMassEntityReplicationSubsystem::RemoveEntityFromGrid(FMassEntityHandle Entity)
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

void UArcMassEntityReplicationSubsystem::RegisterEntityArchetypeKey(FMassEntityHandle Entity, const FArchetypeKey& Key)
{
	EntityToArchetypeKey.Add(Entity, Key);
}

// --- Task 10: Tickable subsystem + player cell tracking ---

TStatId UArcMassEntityReplicationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArcMassEntityReplicationSubsystem, STATGROUP_Game);
}

void UArcMassEntityReplicationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FVector UArcMassEntityReplicationSubsystem::GetPlayerPosition(uint32 ConnectionId) const
{
	const FSourceState* State = SourceStates.Find(ConnectionId);
	return State ? State->Position : FVector::ZeroVector;
}

bool UArcMassEntityReplicationSubsystem::IsEntityRelevantToConnection(FArcMassNetId NetId, uint32 ConnectionId) const
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

bool UArcMassEntityReplicationSubsystem::IsConnectionOwnerOf(FArcMassNetId NetId, uint32 ConnectionId) const
{
	const uint32* Owner = EntityOwnerConnectionId.Find(NetId);
	return Owner == nullptr || *Owner == ConnectionId;
}

void UArcMassEntityReplicationSubsystem::SetEntityOwner(FArcMassNetId NetId, uint32 ConnectionId)
{
	EntityOwnerConnectionId.Add(NetId, ConnectionId);
}

void UArcMassEntityReplicationSubsystem::UpdateSourceState(uint32 ConnectionId, FVector Position, FIntVector2 Cell)
{
	FSourceState& State = SourceStates.FindOrAdd(ConnectionId);
	State.Position = Position;
	State.Cell = Cell;
}

const UArcMassEntityReplicationSubsystem::FSourceState* UArcMassEntityReplicationSubsystem::GetSourceState(uint32 ConnectionId) const
{
	return SourceStates.Find(ConnectionId);
}

TArray<uint32> UArcMassEntityReplicationSubsystem::GetAllSourceConnectionIds() const
{
	TArray<uint32> Result;
	SourceStates.GetKeys(Result);
	return Result;
}

void UArcMassEntityReplicationSubsystem::SetCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell)
{
	ConnectionRelevantCells.FindOrAdd(ConnectionId).Add(Cell);
}

void UArcMassEntityReplicationSubsystem::SetCellIrrelevantForConnection(uint32 ConnectionId, FIntVector2 Cell)
{
	TSet<FIntVector2>* Cells = ConnectionRelevantCells.Find(ConnectionId);
	if (Cells)
	{
		Cells->Remove(Cell);
	}
}

bool UArcMassEntityReplicationSubsystem::IsCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell) const
{
	const TSet<FIntVector2>* Cells = ConnectionRelevantCells.Find(ConnectionId);
	return Cells && Cells->Contains(Cell);
}
