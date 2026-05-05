// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "Fragments/ArcMassNetId.h"
#include "StructUtils/InstancedStruct.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "Spatial/ArcMassSpatialGrid.h"

class AArcMassEntityReplicationProxy;
class UNetDriver;
class UMassEntityConfigAsset;

#include "ArcMassEntityReplicationProxySubsystem.generated.h"

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationProxySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	struct FArchetypeKey
	{
		TArray<const UScriptStruct*> SortedTypes;

		bool operator==(const FArchetypeKey& Other) const { return SortedTypes == Other.SortedTypes; }
		friend uint32 GetTypeHash(const FArchetypeKey& Key)
		{
			uint32 Hash = 0;
			for (const UScriptStruct* Type : Key.SortedTypes)
			{
				Hash = HashCombine(Hash, ::GetTypeHash(Type));
			}
			return Hash;
		}
	};

	struct FSourceState
	{
		FVector Position = FVector::ZeroVector;
		FIntVector2 Cell = FIntVector2(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
	};

	static constexpr int32 MaxEntitiesPerProxy = 4000;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	FArcMassNetId AllocateNetId();

	AArcMassEntityReplicationProxy* GetOrCreateProxy(
		const TArray<const UScriptStruct*>& FragmentTypes,
		float CullDistance,
		UMassEntityConfigAsset* InEntityConfigAsset = nullptr);

	template<typename TProxyClass>
	TProxyClass* SpawnProxyOfClass()
	{
		UWorld* World = GetWorld();
		TProxyClass* Proxy = World->SpawnActor<TProxyClass>();
		OwnedProxies.Add(Proxy);
		return Proxy;
	}

	void RegisterEntityNetId(FArcMassNetId NetId, FMassEntityHandle Entity);
	void UnregisterEntityNetId(FArcMassNetId NetId);
	FMassEntityHandle FindEntityByNetId(FArcMassNetId NetId) const;
	FArcMassNetId FindNetId(FMassEntityHandle Entity) const;

	void MarkEntityDirty(FMassEntityHandle Entity);
	TSet<FMassEntityHandle> TakeDirtyEntities();

	AArcMassEntityReplicationProxy* FindProxyForEntity(FMassEntityHandle Entity) const;

	void RegisterDescriptorSet(uint32 InHash, const ArcMassReplication::FArcMassReplicationDescriptorSet* InDescriptorSet);
	const ArcMassReplication::FArcMassReplicationDescriptorSet* FindDescriptorSetByHash(uint32 InHash) const;

	void OnClientEntityAdded(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots,
		const ArcMassReplication::FArcMassReplicationDescriptorSet* DescriptorSet,
		UMassEntityConfigAsset* InEntityConfigAsset = nullptr);
	void OnClientEntityChanged(FArcMassNetId NetId, const TArray<FInstancedStruct>& FragmentSlots,
		uint32 ChangedFragmentMask,
		const ArcMassReplication::FArcMassReplicationDescriptorSet* DescriptorSet);
	void OnClientEntityRemoved(FArcMassNetId NetId);

	FArcMassSpatialGrid* GetOrCreateGrid(const FArchetypeKey& Key, float CellSize);
	FArcMassSpatialGrid* FindGrid(const FArchetypeKey& Key);
	void AddEntityToGrid(FMassEntityHandle Entity, FVector Position);
	void UpdateEntityInGrid(FMassEntityHandle Entity, FVector Position);
	void RemoveEntityFromGrid(FMassEntityHandle Entity);
	void RegisterEntityArchetypeKey(FMassEntityHandle Entity, const FArchetypeKey& Key);

	FVector GetPlayerPosition(uint32 ConnectionId) const;
	bool IsEntityRelevantToConnection(FArcMassNetId NetId, uint32 ConnectionId) const;
	bool IsConnectionOwnerOf(FArcMassNetId NetId, uint32 ConnectionId) const;
	void SetEntityOwner(FArcMassNetId NetId, uint32 ConnectionId);

	void UpdateSourceState(uint32 ConnectionId, FVector Position, FIntVector2 Cell);
	const FSourceState* GetSourceState(uint32 ConnectionId) const;
	TArray<uint32> GetAllSourceConnectionIds() const;

	void SetCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell);
	void SetCellIrrelevantForConnection(uint32 ConnectionId, FIntVector2 Cell);
	bool IsCellRelevantForConnection(uint32 ConnectionId, FIntVector2 Cell) const;

	const TMap<FArchetypeKey, FArcMassSpatialGrid>& GetArchetypeGrids() const { return ArchetypeGrids; }

	float EnterRadiusCells = 2.f;
	float ExitRadiusCells = 3.f;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void RegisterNetDataStore(UNetDriver* NetDriver);

	bool bNetDataStoreRegistered = false;

	uint32 NextNetIdValue = 1;

	TMap<FArcMassNetId, FMassEntityHandle> NetIdToEntity;
	TMap<FMassEntityHandle, FArcMassNetId> EntityToNetId;

	TSet<FMassEntityHandle> DirtyEntities;

	TMap<FArchetypeKey, ArcMassReplication::FArcMassReplicationDescriptorSet> DescriptorSets;

	TMap<FArchetypeKey, TArray<TObjectPtr<AArcMassEntityReplicationProxy>>> ArchetypeToProxies;
	TMap<FMassEntityHandle, FArchetypeKey> EntityToArchetypeKey;

	TMap<FArchetypeKey, FArcMassSpatialGrid> ArchetypeGrids;

	TMap<FArcMassNetId, uint32> EntityOwnerConnectionId;

	TMap<uint32, FSourceState> SourceStates;
	TMap<uint32, TSet<FIntVector2>> ConnectionRelevantCells;

	TMap<uint32, const ArcMassReplication::FArcMassReplicationDescriptorSet*> DescriptorSetsByHash;

	UPROPERTY()
	TArray<TObjectPtr<AArcMassEntityReplicationProxy>> OwnedProxies;
};
