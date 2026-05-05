// Copyright Lukasz Baran. All Rights Reserved.
//
// TEST-ONLY. Hand-written stand-in for what the future codegen will emit per
// replicated Mass schema. This vessel covers the schema { FArcMassTestPayloadFragment }.
//
// Production equivalents will be generated, not handwritten, and will live in
// dedicated runtime modules — not in the test module.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassTestPayloadFragment.h"
#include "Replication/ArcMassEntityVessel.h"
#include "ArcMassRepProxy_TestPayload.generated.h"

UCLASS()
class UArcMassRepProxy_TestPayload : public UArcMassEntityVessel
{
    GENERATED_BODY()

public:
    UArcMassRepProxy_TestPayload();

    /** Mirrored copy of the live Mass entity's payload fragment. */
    UPROPERTY(ReplicatedUsing = OnRep_Payload)
    FArcMassTestPayloadFragment Payload;

    UFUNCTION()
    void OnRep_Payload();

    // UObject
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // UArcMassEntityVessel
    virtual void CaptureEntityStateForReplication(FMassEntityManager& EntityManager) override;
    virtual void ApplyReplicatedStateToEntity(FMassEntityManager& EntityManager) override;
};
