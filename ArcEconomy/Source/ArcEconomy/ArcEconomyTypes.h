// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcEconomyTypes.generated.h"

UENUM(BlueprintType)
enum class EArcEconomyNPCRole : uint8
{
    Idle,
    Worker,
    Transporter,
    Gatherer
};

UENUM(BlueprintType)
enum class EArcTransporterTaskState : uint8
{
    Idle,
    SeekingTask,
    PickingUp,
    Delivering
};

namespace ArcEconomy::Signals
{
    const FName SupplyAvailable = FName(TEXT("ArcTransport.SupplyAvailable"));
    const FName DemandPosted = FName(TEXT("ArcTransport.DemandPosted"));
    const FName TransporterIdle = FName(TEXT("ArcTransport.TransporterIdle"));
}
