// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "Net/Iris/ReplicationSystem/NetRootObjectAdapter.h"
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
// UArcMassReplicationProxy
// Lightweight UObject (not AActor) that carries a single Mass entity's
// replicated data through Iris via FNetRootObjectAdapter.
// One proxy per replicated entity. Created on server, replicated to clients.
// ---------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class ARCMASS_API UArcMassReplicationProxy : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassNetId NetId;

	/** Raw blob: all replicated fragments serialized back-to-back.
	 *  Format: [FragmentTypePathHash:uint32][DataSize:int32][Data...] per fragment. */
	UPROPERTY(Replicated)
	TArray<uint8> Payload;

	/** Start replicating this proxy through Iris. Server only. */
	void StartReplication(ULevel* Level);

	/** Stop replicating and clean up. */
	void StopReplication();

	bool IsReplicating() const { return Adapter.IsReplicating(); }

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

	/** Register an entity with its NetId and create a replication proxy. */
	void RegisterEntity(FArcMassNetId NetId, FMassEntityHandle Entity);

	/** Unregister an entity and destroy its proxy. */
	void UnregisterEntity(FArcMassNetId NetId);

	// -- Lookup ---------------------------------------------------------------

	/** Resolve a NetId to a local entity handle. Works on both server and client. */
	FMassEntityHandle FindEntity(FArcMassNetId NetId) const;

	/** Resolve a local entity handle to its NetId. */
	FArcMassNetId FindNetId(FMassEntityHandle Entity) const;

	/** Get the replication proxy for a given NetId. Server only. */
	UArcMassReplicationProxy* FindProxy(FArcMassNetId NetId) const;

	// -- Client callbacks -----------------------------------------------------

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
	TMap<uint32, TObjectPtr<UArcMassReplicationProxy>> Proxies;
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
// Server-side: serializes replicated fragments into proxy payloads.
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
