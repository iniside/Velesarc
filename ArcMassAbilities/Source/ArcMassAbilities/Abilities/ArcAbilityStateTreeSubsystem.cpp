// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "StateTree.h"

FMassStateTreeInstanceHandle UArcAbilityStateTreeSubsystem::AllocateInstanceData(const UStateTree* StateTree)
{
    check(StateTree);

    int32 Index;
    if (Freelist.Num() > 0)
    {
        Index = Freelist.Pop();
    }
    else
    {
        Index = InstanceDataArray.AddDefaulted();
    }

    FArcAbilityInstanceDataItem& Item = InstanceDataArray[Index];
    Item.Generation++;
    Item.InstanceData.Reset();

    return FMassStateTreeInstanceHandle::Make(Index, Item.Generation);
}

void UArcAbilityStateTreeSubsystem::FreeInstanceData(const FMassStateTreeInstanceHandle Handle)
{
    if (!IsValidHandle(Handle))
    {
        return;
    }

    FArcAbilityInstanceDataItem& Item = InstanceDataArray[Handle.GetIndex()];
    Item.InstanceData.Reset();
    Freelist.Add(Handle.GetIndex());
}

FStateTreeInstanceData* UArcAbilityStateTreeSubsystem::GetInstanceData(const FMassStateTreeInstanceHandle Handle)
{
    if (!IsValidHandle(Handle))
    {
        return nullptr;
    }
    return &InstanceDataArray[Handle.GetIndex()].InstanceData;
}

bool UArcAbilityStateTreeSubsystem::IsValidHandle(const FMassStateTreeInstanceHandle Handle) const
{
    return InstanceDataArray.IsValidIndex(Handle.GetIndex())
        && InstanceDataArray[Handle.GetIndex()].Generation == Handle.GetGeneration();
}
