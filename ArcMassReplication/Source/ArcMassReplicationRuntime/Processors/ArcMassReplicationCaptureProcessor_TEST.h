// Copyright Lukasz Baran. All Rights Reserved.
//
// TEST-ONLY — DO NOT SHIP.
//
// Polling capture: every tick, copy live Mass fragment memory into each
// replicated entity's vessel UPROPERTY fields by calling
// UArcMassEntityVessel::CaptureEntityStateForReplication.
//
// This processor exists solely to get smoke tests green while the per-fragment
// custom FReplicationFragment dead-end is removed and the typed-vessel path is
// proven. PRODUCTION REPLICATION MUST BE SIGNAL-DRIVEN: Mass observers / signals
// fire on fragment write and the handler invokes Capture on only the affected
// vessel and only the dirtied fragment. Polling every entity every frame does
// not scale.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "ArcMassReplicationCaptureProcessor_TEST.generated.h"

UCLASS()
class UArcMassReplicationCaptureProcessor_TEST : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcMassReplicationCaptureProcessor_TEST();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery EntityQuery{*this};
};
