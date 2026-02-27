// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityManager.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectFactory.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/UnrealNetwork.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassReplication.generated.h"

class UArcMassReplicationProxy;
class UMassSignalSubsystem;

// ---------------------------------------------------------------------------
// Signals
// ---------------------------------------------------------------------------

namespace UE::ArcMass::Signals
{
	const FName ReplicationDirty = FName(TEXT("ReplicationDirty"));
}

// ---------------------------------------------------------------------------
// FArcMassNetId — Stable network identifier for Mass entities.
// Assigned on the server, replicated to clients.
// Uses a monotonically increasing uint32 counter (0 = invalid).
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassNetId
{
	GENERATED_BODY()

	FArcMassNetId() = default;
	explicit FArcMassNetId(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcMassNetId& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcMassNetId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcMassNetId& Id) { return ::GetTypeHash(Id.Value); }

	FString ToString() const { return FString::Printf(TEXT("NetId[%u]"), Value); }

	friend FArchive& operator<<(FArchive& Ar, FArcMassNetId& Id)
	{
		Ar << Id.Value;
		return Ar;
	}

private:
	UPROPERTY(EditAnywhere)
	uint32 Value = 0;
};

// ---------------------------------------------------------------------------
// Fragments & Tags
// ---------------------------------------------------------------------------

/** Per-entity fragment holding the stable network id. */
USTRUCT()
struct ARCMASS_API FArcMassNetIdFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassNetId NetId;
};

/** Tag to mark entities that should be replicated. Added by the replication trait. */
USTRUCT()
struct ARCMASS_API FArcMassReplicatedTag : public FMassTag
{
	GENERATED_BODY()
};

// ---------------------------------------------------------------------------
// Replication Config (Const Shared Fragment)
// ---------------------------------------------------------------------------

/** Describes which fragment types to replicate for a given archetype group. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassReplicationConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** List of fragment types whose data should be replicated to clients.
	 *  Each entry must be a UScriptStruct derived from FMassFragment. */
	UPROPERTY(EditAnywhere, Category = "Replication", meta = (BaseStruct = "/Script/MassEntity.MassFragment"))
	TArray<TObjectPtr<const UScriptStruct>> ReplicatedFragments;
};

template<>
struct TMassFragmentTraits<FArcMassReplicationConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// FastArray item — one entry per replicated entity.
// ---------------------------------------------------------------------------

USTRUCT()
struct FArcMassReplicatedEntityItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassNetId NetId;

	/** Raw blob: all replicated fragments serialized back-to-back.
	 *  Format: [FragmentTypePathHash:uint32][DataSize:int32][Data...] per fragment. */
	UPROPERTY()
	TArray<uint8> Payload;
};

// ---------------------------------------------------------------------------
// FastArray serializer — wraps the item array and routes callbacks.
// ---------------------------------------------------------------------------

USTRUCT()
struct FArcMassReplicatedEntityArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassReplicatedEntityItem> Items;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FArcMassReplicatedEntityItem, FArcMassReplicatedEntityArray>(Items, DeltaParms, *this);
	}

	// Client callbacks — implemented in cpp, forwarded to subsystem.
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	/** Backpointer to owning proxy — set after construction. */
	UPROPERTY(NotReplicated)
	TObjectPtr<UArcMassReplicationProxy> OwnerProxy = nullptr;
};

template<>
struct TStructOpsTypeTraits<FArcMassReplicatedEntityArray> : public TStructOpsTypeTraitsBase2<FArcMassReplicatedEntityArray>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

// ---------------------------------------------------------------------------
// UArcMassReplicationProxyFactory
// Custom factory for replication proxies. Provides world location info
// so the Iris spatial (grid) filter can cull proxies per-connection.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassReplicationProxyFactory : public UNetRootObjectFactory
{
	GENERATED_BODY()

public:
	static FName GetFactoryName();

	virtual TOptional<FWorldInfoData> GetWorldInfo(const FWorldInfoContext& Context) const override;
};

// ---------------------------------------------------------------------------
// UArcMassReplicationProxy
// Lightweight UObject (not AActor) that carries multiple Mass entities'
// replicated data through Iris via FastArray.
// One proxy sits at a fixed world position with a cull distance. Iris's
// spatial grid filter handles per-connection distance culling automatically.
// Future: multiple proxies for spatial partitioning.
// ---------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class ARCMASS_API UArcMassReplicationProxy : public UObject
{
	GENERATED_BODY()

public:
	UArcMassReplicationProxy();

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The replicated entity array. */
	UPROPERTY(Replicated)
	FArcMassReplicatedEntityArray ReplicatedEntities;

	/** World position of this proxy (used for spatial filtering). */
	FVector WorldPosition = FVector::ZeroVector;

	/** Max distance from proxy position at which clients receive data. */
	float CullDistance = 15000.0f;

	/** Start replicating this proxy through Iris with spatial filtering. Server only. */
	void StartReplication(ULevel* Level);

	/** Stop replicating and clean up. */
	void StopReplication();

	bool IsReplicating() const { return bIsReplicating; }

	// -- Server helpers -------------------------------------------------------

	/** Find item index by NetId, or INDEX_NONE. */
	int32 FindItemIndex(FArcMassNetId NetId) const;

	/** Add or update an entity's payload. */
	void SetEntityPayload(FArcMassNetId NetId, TArray<uint8>&& Payload);

	/** Remove an entity from the array. */
	void RemoveEntity(FArcMassNetId NetId);

private:
	bool bIsReplicating = false;
};

// ---------------------------------------------------------------------------
// UArcMassReplicationSubsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassReplicationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// -- Server API -----------------------------------------------------------

	/** Allocate the next unique NetId. Server only. */
	FArcMassNetId AllocateNetId();

	/** Register an entity with its NetId. */
	void RegisterEntity(FArcMassNetId NetId, FMassEntityHandle Entity);

	/** Unregister an entity. */
	void UnregisterEntity(FArcMassNetId NetId);

	/** Mark an entity as dirty so its replicated fragments are re-serialized
	 *  and pushed to the proxy this frame. Accumulated and batch-signaled in Tick(). */
	void MarkEntityDirty(FMassEntityHandle Entity);

	/** Convenience: modify a fragment and mark the entity dirty in one call.
	 *  Usage: Subsystem->ModifyAndReplicate<FMyFragment>(Entity, [](FMyFragment& F) { F.Value = 42; }); */
	template<typename TFragment, typename TFunc>
	void ModifyAndReplicate(FMassEntityHandle Entity, TFunc&& Func);

	// -- Lookup ---------------------------------------------------------------

	/** Resolve a NetId to a local entity handle. Works on both server and client. */
	FMassEntityHandle FindEntity(FArcMassNetId NetId) const;

	/** Resolve a local entity handle to its NetId. */
	FArcMassNetId FindNetId(FMassEntityHandle Entity) const;

	/** Get the replication proxy. Created lazily on server. */
	UArcMassReplicationProxy* GetOrCreateProxy();

	// -- Flush (called by signal processor) -----------------------------------

	/** Consume accumulated dirty entities. Returns the set and clears it. */
	TSet<FMassEntityHandle> TakeDirtyEntities() { return MoveTemp(DirtyEntities); }

	// -- Client callbacks from FastArray --------------------------------------

	void OnClientEntityAdded(FArcMassNetId NetId, const TArray<uint8>& Payload);
	void OnClientEntityChanged(FArcMassNetId NetId, const TArray<uint8>& Payload);
	void OnClientEntityRemoved(FArcMassNetId NetId);

private:
	/** Deserialize payload into an entity's fragments. */
	void ApplyPayloadToEntity(FMassEntityHandle Entity, const TArray<uint8>& Payload);

	uint32 NextNetIdValue = 1;

	TMap<FArcMassNetId, FMassEntityHandle> NetIdToEntity;
	TMap<FMassEntityHandle, FArcMassNetId> EntityToNetId;

	/** Entities marked dirty this frame, waiting for the flush processor. */
	TSet<FMassEntityHandle> DirtyEntities;

	UPROPERTY()
	TObjectPtr<UArcMassReplicationProxy> Proxy = nullptr;
};

// ---------------------------------------------------------------------------
// Template implementation
// ---------------------------------------------------------------------------

template<typename TFragment, typename TFunc>
void UArcMassReplicationSubsystem::ModifyAndReplicate(FMassEntityHandle Entity, TFunc&& Func)
{
	UWorld* World = GetWorld();
	check(World);
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
	check(EntityManager.IsEntityValid(Entity));

	TFragment& Fragment = EntityManager.GetFragmentDataChecked<TFragment>(Entity);
	Func(Fragment);
	MarkEntityDirty(Entity);
}

// ---------------------------------------------------------------------------
// UArcMassNetIdAssignObserver
// Observes FArcMassReplicatedTag Add on the server, allocates a NetId.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassNetIdAssignObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassNetIdAssignObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// UArcMassReplicationFlushProcessor
// Signal-based: flushes dirty entities into the proxy FastArray.
// Only runs when entities have been marked dirty via MarkEntityDirty().
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassReplicationFlushProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassReplicationFlushProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Trait — opts entities into replication
// ---------------------------------------------------------------------------

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Mass Replication"))
class ARCMASS_API UArcMassReplicationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replication")
	FArcMassReplicationConfigFragment ReplicationConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
