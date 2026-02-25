// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectAdapter.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Net/UnrealNetwork.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassReplication.generated.h"

class UArcMassReplicationProxy;

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
// UArcMassReplicationProxy
// Lightweight UObject (not AActor) that carries multiple Mass entities'
// replicated data through Iris via FNetRootObjectAdapter + FastArray.
// One proxy can handle many entities. Future: spatial partitioning via
// multiple proxies.
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

	/** Start replicating this proxy through Iris. Server only. */
	void StartReplication(ULevel* Level);

	/** Stop replicating and clean up. */
	void StopReplication();

	bool IsReplicating() const { return Adapter.IsReplicating(); }

	// -- Server helpers -------------------------------------------------------

	/** Find item index by NetId, or INDEX_NONE. */
	int32 FindItemIndex(FArcMassNetId NetId) const;

	/** Add or update an entity's payload. */
	void SetEntityPayload(FArcMassNetId NetId, TArray<uint8>&& Payload);

	/** Remove an entity from the array. */
	void RemoveEntity(FArcMassNetId NetId);

private:
	UE::Net::FNetRootObjectAdapter Adapter{UE::Net::FRootObjectSettings{.bIsAlwaysRelevant = true}};
};

// ---------------------------------------------------------------------------
// UArcMassReplicationSubsystem
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Server API -----------------------------------------------------------

	/** Allocate the next unique NetId. Server only. */
	FArcMassNetId AllocateNetId();

	/** Register an entity with its NetId. */
	void RegisterEntity(FArcMassNetId NetId, FMassEntityHandle Entity);

	/** Unregister an entity. */
	void UnregisterEntity(FArcMassNetId NetId);

	// -- Lookup ---------------------------------------------------------------

	/** Resolve a NetId to a local entity handle. Works on both server and client. */
	FMassEntityHandle FindEntity(FArcMassNetId NetId) const;

	/** Resolve a local entity handle to its NetId. */
	FArcMassNetId FindNetId(FMassEntityHandle Entity) const;

	/** Get the replication proxy. Created lazily on server. */
	UArcMassReplicationProxy* GetOrCreateProxy();

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

	UPROPERTY()
	TObjectPtr<UArcMassReplicationProxy> Proxy = nullptr;
};

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
// UArcMassReplicationCollectProcessor
// Server-side: serializes replicated fragments into proxy FastArray.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcMassReplicationCollectProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcMassReplicationCollectProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
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
