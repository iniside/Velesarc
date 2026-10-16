// ArcMassEntityVessel.h
// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Mass/EntityHandle.h"
#include "UObject/Object.h"
#include "ArcMassEntityVessel.generated.h"

struct FMassEntityManager;
class UMassEntityConfigAsset;

UCLASS(NotBlueprintable)
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityVessel : public UObject
{
    GENERATED_BODY()

public:
    /** Required for the bridge to accept this UObject as a replicated root. */
    virtual bool IsSupportedForNetworking() const override { return true; }

    /** Local Mass entity this vessel represents. Plain value, no UObject ref. */
    FMassEntityHandle EntityHandle;

    /** The config the vessel was bound to. UPROPERTY for GC. */
    UPROPERTY()
    TObjectPtr<UMassEntityConfigAsset> Config;

    /**
     * Handle Iris assigned to this vessel. Set by RegisterEntity after a successful
     * StartReplicatingRootObject; cleared in ReleaseVessel. Lets teardown stop
     * replication by handle without touching Mass or assuming instance liveness.
     */
    UE::Net::FNetRefHandle NetRefHandle;

    /**
     * True between a successful StartReplicatingRootObject and ReleaseVessel. Lets the
     * subsystem stop Iris replication for exactly the vessels it actually registered —
     * pooled vessels were reset and never registered. Server-only semantics.
     */
    bool bRegisteredWithIris = false;

    /**
     * O(1) double-free guard: true while this vessel sits in the subsystem's
     * VesselPool, false the moment AcquireVessel pops it. ReleaseVessel refuses a
     * vessel whose flag is already set — a double release would otherwise hand the
     * same vessel to two entities and cross-contaminate their replicated state.
     */
    bool bInPool = false;

    /**
     * Forces the next CaptureEntityStateForReplication to mark ALL replicated
     * properties dirty even when the captured values match the vessel's stale
     * UPROPERTY state, so a reused vessel always sends frame 1 for its new entity.
     * Set true on acquire; the first capture must honor it and clear it. The base
     * no-op capture ignores it today — this is the contract for the future codegen
     * capture path.
     */
    bool bForceDirty = false;

    /**
     * Server-side: copy live Mass fragment memory into this vessel's UPROPERTY
     * fragment fields, then mark them dirty for Iris.
     *
     * Base implementation is a no-op — base vessels carry no fragment UPROPERTYs.
     * Typed subclasses (eventually generated) override this per schema.
     */
    virtual void CaptureEntityStateForReplication(FMassEntityManager& EntityManager) {}

    /**
     * Client-side: write this vessel's UPROPERTY fragment fields back into the
     * live Mass entity.
     *
     * Called from per-property OnRep_<Field> handlers on typed subclasses.
     * Base implementation is a no-op.
     */
    virtual void ApplyReplicatedStateToEntity(FMassEntityManager& EntityManager) {}
};
