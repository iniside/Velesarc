// ArcMassEntityVessel.h
// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
