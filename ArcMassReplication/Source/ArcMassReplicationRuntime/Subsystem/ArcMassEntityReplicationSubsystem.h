// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationSystem/NetObjectFactoryRegistry.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Mass/EntityHandle.h"
#include "Replication/ArcMassEntityVessel.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassEntityReplicationSubsystem.generated.h"

class UMassEntityConfigAsset;

/**
 * World subsystem owning per-entity Iris replication for Mass entities.
 *
 * Maintains:
 *  - A pool of UArcMassEntityVessel objects (one per replicated entity).
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
	 * Server-side: acquires a vessel from the pool, sets vessel state, calls
	 * StartReplicatingRootObject, returns the new FNetRefHandle.
	 */
	UE::Net::FNetRefHandle RegisterEntity(FMassEntityHandle Entity, UMassEntityConfigAsset* Config);

	/** Server-side: ends replication, returns vessel to pool. */
	void UnregisterEntity(FMassEntityHandle Entity);

	/** Lookup helpers (server-side). */
	UArcMassEntityVessel* FindVesselForEntity(FMassEntityHandle Entity) const;

	/**
	 * Client-side: allocate a fresh vessel and bind it to a Config. Client vessels
	 * are NEVER pooled — Iris owns their lifetime (AllowDestroyInstanceFromRemote),
	 * so reuse would race remote destruction. Tracked in ClientVessels for GC
	 * rooting until ReleaseClientVessel.
	 * Used by the factory's InstantiateReplicatedObjectFromHeader.
	 * Vessel's EntityHandle is left invalid for the caller to populate
	 * (typically via the local Mass entity spawn).
	 */
	UArcMassEntityVessel* AcquireClientVessel(UMassEntityConfigAsset* Config);

	/**
	 * Client-side: release a vessel acquired via AcquireClientVessel. Removes it
	 * from ClientVessels and marks it garbage — client vessels never return to the
	 * server VesselPool. Does not interact with the EntityToVessel map (that map is
	 * server-side only).
	 */
	void ReleaseClientVessel(UArcMassEntityVessel* Vessel);

	/**
	 * Destroys the local Mass entity backing a vessel (defers DestroyEntity on
	 * Vessel->EntityHandle if set), resolving the world from the vessel itself.
	 * Called by the factory's DetachedFromReplication and by the Deinitialize
	 * client safety-net sweep.
	 */
	void DestroyLocalEntityForVessel(UArcMassEntityVessel* Vessel);

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
	TArray<TObjectPtr<UArcMassEntityVessel>> VesselPool;

	UPROPERTY()
	TMap<FMassEntityHandle, TObjectPtr<UArcMassEntityVessel>> EntityToVessel;

	/**
	 * Client-side live vessels. GC rooting — being outered to the subsystem alone
	 * does not keep a UObject alive. Entries are removed in ReleaseClientVessel;
	 * Deinitialize sweeps any survivors (detach-never-fires teardown windows).
	 */
	UPROPERTY()
	TSet<TObjectPtr<UArcMassEntityVessel>> ClientVessels;

	/** Resolved lazily from the bridge once the replication system exists. */
	UE::Net::FNetObjectFactoryId FactoryId = UE::Net::InvalidNetObjectFactoryId;
};
