// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Mass/EntityHandle.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassEntityReplicationSubsystem.generated.h"

class UArcMassEntityVesselClusterRoot;
class UMassEntityConfigAsset;

/**
 * World subsystem owning per-entity Iris replication for Mass entities.
 *
 * Maintains:
 *  - Protocol identifier maps (UMassEntityConfigAsset <-> FReplicationProtocolIdentifier)
 *    populated by a one-time warmup pass before the first client connects.
 *  - A pool of UArcMassEntityVessel objects (one per replicated entity), GC-clustered
 *    against a single root for cheap reachability.
 *  - Server-side bookkeeping (entity <-> vessel) used by the bridge/factory.
 *
 * The previous proxy-based replication path (proxy actor, FArcMassNetId, spatial grid,
 * descriptor sets, source-state tracking) is being removed in this refactor and is
 * fully cleaned up in Phase 7. This header intentionally does not preserve those APIs.
 */
UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Public entry point — drives a warmup registration cycle for a single config,
	 * captures the resulting FReplicationProtocolIdentifier, populates both maps.
	 *
	 * MUST be called before the first client connection joins. After that point
	 * this method logs a warning and returns false (see IsProtocolWarmupLocked).
	 *
	 * Asset registry sweep at startup calls this once per discovered config; tests
	 * call it directly with in-memory configs.
	 */
	bool RegisterConfigForReplication(UMassEntityConfigAsset* Config);

	/**
	 * Server-side: acquires a vessel from the pool, sets vessel state, calls
	 * StartReplicatingRootObject, returns the new FNetRefHandle.
	 */
	UE::Net::FNetRefHandle RegisterEntity(FMassEntityHandle Entity, UMassEntityConfigAsset* Config);

	/** Server-side: ends replication, returns vessel to pool. */
	void UnregisterEntity(FMassEntityHandle Entity);

	/** Lookup helpers (server-side). */
	UArcMassEntityVessel* FindVesselForEntity(FMassEntityHandle Entity) const;
	FMassEntityHandle FindEntityForVessel(UArcMassEntityVessel* Vessel) const;

	/** Protocol map accessors (used by the factory). */
	UMassEntityConfigAsset* FindConfigForProtocol(uint32 ProtocolId) const;
	uint32 FindProtocolForConfig(UMassEntityConfigAsset* Config) const;

	/**
	 * Client-side: stopgap lookup used by InstantiateReplicatedObjectFromHeader when
	 * FindConfigForProtocol misses. Walks PendingClientConfigs (registered via
	 * RegisterConfigForReplication on the client) and — pending a real
	 * descriptor-driven match — returns the first one. Caches the result in
	 * ProtocolToConfig so subsequent lookups hit. TODO: replace with deterministic
	 * matching once a public CalculateProtocolIdentifier path exists.
	 */
	UMassEntityConfigAsset* ResolveClientConfigForProtocol(uint32 ProtocolId);

	/** True after the first registration locks warmup; further RegisterConfigForReplication calls fail. */
	bool IsProtocolWarmupLocked() const { return bProtocolWarmupLocked; }

	/**
	 * Client-side: pool-acquire a vessel and bind it to a Config.
	 * Used by the factory's InstantiateReplicatedObjectFromHeader.
	 * Returns null if the pool is exhausted and growth fails.
	 * Vessel's EntityHandle is left invalid for the caller to populate
	 * (typically via the local Mass entity spawn).
	 */
	UArcMassEntityVessel* AcquireClientVessel(UMassEntityConfigAsset* Config);

	/**
	 * Client-side: pool-return a vessel that was acquired via AcquireClientVessel.
	 * Clears EntityHandle and Config before returning to pool. Does not interact
	 * with the EntityToVessel map (that map is server-side only).
	 */
	void ReleaseClientVessel(UArcMassEntityVessel* Vessel);

	/**
	 * Register a typed UArcMassEntityVessel subclass for a Config. Both server
	 * (RegisterEntity) and client (factory InstantiateReplicatedObjectFromHeader)
	 * will instantiate the subclass instead of the base UArcMassEntityVessel.
	 *
	 * Must be called before the corresponding RegisterEntity / first replication.
	 * Idempotent for the same (Config, VesselClass) pair.
	 *
	 * In the test-only path this is called explicitly. Future codegen modules
	 * register at StartupModule.
	 */
	void RegisterVesselClassForConfig(
		UMassEntityConfigAsset* Config,
		TSubclassOf<UArcMassEntityVessel> VesselClass);

	/** Returns the registered subclass for a Config, or null if none registered. */
	TSubclassOf<UArcMassEntityVessel> FindVesselClassForConfig(
		UMassEntityConfigAsset* Config) const;

private:
	friend struct FArcMassEntityReplicationSubsystemTestAccess;

	void RunAssetRegistrySweep();

	/**
	 * Lazy guard. The replication system / NetDriver may not yet exist when the
	 * subsystem initializes. Any code path that needs the protocol map populated
	 * (RegisterEntity, RegisterConfigForReplication) calls this first; it runs the
	 * sweep at most once and only when the bridge is actually available.
	 */
	void EnsureSweepRun();

	/** FWorldDelegates::OnNetDriverCreated handler — runs the sweep when the NetDriver appears. */
	void HandleNetDriverCreated(UWorld* InWorld, UNetDriver* InNetDriver);

	void AllocateVesselPool(int32 InitialSize);

	/**
	 * Pool ops. Private — the pool/EntityToVessel invariant is maintained by
	 * RegisterEntity/UnregisterEntity. Calling these directly will produce a
	 * stale EntityToVessel map. Tests use FArcMassEntityReplicationSubsystemTestAccess.
	 */
	UArcMassEntityVessel* AcquireVessel();
	void ReleaseVessel(UArcMassEntityVessel* Vessel);

	/**
	 * Allocate (or pool-acquire) a vessel of the appropriate class for Config.
	 * If a typed subclass is registered, NewObjects an instance of that subclass.
	 * Otherwise falls back to AcquireVessel (base class, may come from the pool).
	 *
	 * Note: typed-subclass instances bypass the pre-allocated pool. This is fine
	 * for the smoke test (one entity). Future work: per-class pooling.
	 */
	UArcMassEntityVessel* AcquireVesselForConfig(UMassEntityConfigAsset* Config);

	UPROPERTY()
	TMap<TObjectPtr<UMassEntityConfigAsset>, TSubclassOf<UArcMassEntityVessel>> ConfigToVesselClass;

	UPROPERTY()
	TMap<TObjectPtr<UMassEntityConfigAsset>, uint32> ConfigToProtocol;

	UPROPERTY()
	TMap<uint32, TObjectPtr<UMassEntityConfigAsset>> ProtocolToConfig;

	UPROPERTY()
	TArray<TObjectPtr<UArcMassEntityVessel>> VesselPool;

	UPROPERTY()
	TMap<FMassEntityHandle, TObjectPtr<UArcMassEntityVessel>> EntityToVessel;

	UPROPERTY()
	TObjectPtr<UArcMassEntityVesselClusterRoot> VesselClusterRoot;

	/** Client-side fallback: configs registered via RegisterConfigForReplication on the client.
	 *  Used by ResolveClientConfigForProtocol because StartReplicatingRootObject is server-only. */
	UPROPERTY()
	TArray<TObjectPtr<UMassEntityConfigAsset>> PendingClientConfigs;

	bool bProtocolWarmupLocked = false;

	bool bAssetRegistrySweepRun = false;

	FDelegateHandle OnNetDriverCreatedHandle;

	/** Resolved lazily from the bridge once the replication system exists. */
	UE::Net::FNetObjectFactoryId FactoryId = UE::Net::InvalidNetObjectFactoryId;
};
