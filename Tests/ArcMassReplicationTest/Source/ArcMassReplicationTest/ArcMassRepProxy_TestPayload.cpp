// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassRepProxy_TestPayload.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Net/UnrealNetwork.h"

UArcMassRepProxy_TestPayload::UArcMassRepProxy_TestPayload()
{
}

void UArcMassRepProxy_TestPayload::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UArcMassRepProxy_TestPayload, Payload);
}

void UArcMassRepProxy_TestPayload::CaptureEntityStateForReplication(FMassEntityManager& EntityManager)
{
    if (!EntityHandle.IsSet() || !EntityManager.IsEntityValid(EntityHandle))
    {
        UE_LOGF(LogTemp, Verbose,"ArcMassReplication[Vessel.Capture]: %ls skipped (EntityHandle not valid: Idx=%d Ser=%d Set=%d)",
            *GetName(), EntityHandle.Index, EntityHandle.SerialNumber, EntityHandle.IsSet() ? 1 : 0);
        return;
    }
    const FArcMassTestPayloadFragment* Live =
        EntityManager.GetFragmentDataPtr<FArcMassTestPayloadFragment>(EntityHandle);
    if (Live == nullptr)
    {
        UE_LOGF(LogTemp, Warning,"ArcMassReplication[Vessel.Capture]: %ls entity has no payload fragment (Idx=%d Ser=%d)",
            *GetName(), EntityHandle.Index, EntityHandle.SerialNumber);
        return;
    }
    if (Payload.Payload != Live->Payload)
    {
        UE_LOGF(LogTemp, Log,"ArcMassReplication[Vessel.Capture]: %ls Live=%u -> Payload (was %u) Entity=(Idx=%d,Ser=%d)",
            *GetName(), Live->Payload, Payload.Payload, EntityHandle.Index, EntityHandle.SerialNumber);
        Payload = *Live;
        // Iris: dirtying happens automatically when the property changes via
        // standard replication path (UObject UPROPERTY mutation).
    }
}

void UArcMassRepProxy_TestPayload::OnRep_Payload()
{
    UWorld* World = GetWorld();
    UE_LOGF(LogTemp, Log,"ArcMassReplication[Vessel.OnRep]: %ls World=%ls Payload=%u Entity=(Idx=%d,Ser=%d)",
        *GetName(),
        World != nullptr ? *World->GetName() : TEXT("<null>"),
        Payload.Payload, EntityHandle.Index, EntityHandle.SerialNumber);
    UMassEntitySubsystem* Subsystem = World != nullptr ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
    if (Subsystem == nullptr)
    {
        UE_LOGF(LogTemp, Warning,"ArcMassReplication[Vessel.OnRep]: %ls no MassEntitySubsystem in world", *GetName());
        return;
    }
    FMassEntityManager& EntityManager = Subsystem->GetMutableEntityManager();
    ApplyReplicatedStateToEntity(EntityManager);
}

void UArcMassRepProxy_TestPayload::ApplyReplicatedStateToEntity(FMassEntityManager& EntityManager)
{
    if (!EntityHandle.IsSet() || !EntityManager.IsEntityValid(EntityHandle))
    {
        UE_LOGF(LogTemp, Warning,"ArcMassReplication[Vessel.Apply]: %ls skipped (EntityHandle not valid: Idx=%d Ser=%d Set=%d)",
            *GetName(), EntityHandle.Index, EntityHandle.SerialNumber, EntityHandle.IsSet() ? 1 : 0);
        return;
    }
    FArcMassTestPayloadFragment* Live =
        EntityManager.GetFragmentDataPtr<FArcMassTestPayloadFragment>(EntityHandle);
    if (Live == nullptr)
    {
        UE_LOGF(LogTemp, Warning,"ArcMassReplication[Vessel.Apply]: %ls entity has no payload fragment (Idx=%d Ser=%d)",
            *GetName(), EntityHandle.Index, EntityHandle.SerialNumber);
        return;
    }
    UE_LOGF(LogTemp, Log,"ArcMassReplication[Vessel.Apply]: %ls Payload=%u -> Live (was %u) Entity=(Idx=%d,Ser=%d)",
        *GetName(), Payload.Payload, Live->Payload, EntityHandle.Index, EntityHandle.SerialNumber);
    *Live = Payload;
}
