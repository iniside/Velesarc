// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSubsystemBase.h"
#include "MassStateTreeSubsystem.h"
#include "StateTreeInstanceData.h"
#include "ArcAbilityStateTreeSubsystem.generated.h"

class UStateTree;

USTRUCT()
struct FArcAbilityInstanceDataItem
{
    GENERATED_BODY()

    UPROPERTY()
    FStateTreeInstanceData InstanceData;

    UPROPERTY()
    int32 Generation = 0;
};

UCLASS()
class ARCMASSABILITIES_API UArcAbilityStateTreeSubsystem : public UMassSubsystemBase
{
    GENERATED_BODY()

public:
    FMassStateTreeInstanceHandle AllocateInstanceData(const UStateTree* StateTree);
    void FreeInstanceData(FMassStateTreeInstanceHandle Handle);
    FStateTreeInstanceData* GetInstanceData(FMassStateTreeInstanceHandle Handle);
    bool IsValidHandle(FMassStateTreeInstanceHandle Handle) const;

private:
    UPROPERTY(Transient)
    TArray<FArcAbilityInstanceDataItem> InstanceDataArray;

    TArray<int32> Freelist;
};
