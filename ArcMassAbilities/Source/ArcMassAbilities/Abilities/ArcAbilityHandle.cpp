// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcAbilityHandle.h"

FArcAbilityHandle FArcAbilityHandle::Make(const int32 InIndex, const int32 InGeneration)
{
    return FArcAbilityHandle(InIndex, InGeneration);
}

bool FArcAbilityHandle::operator==(const FArcAbilityHandle& Other) const
{
    return Index == Other.Index && Generation == Other.Generation;
}

bool FArcAbilityHandle::operator!=(const FArcAbilityHandle& Other) const
{
    return !(*this == Other);
}

uint32 GetTypeHash(const FArcAbilityHandle& Handle)
{
    return HashCombine(::GetTypeHash(Handle.Index), ::GetTypeHash(Handle.Generation));
}

FString FArcAbilityHandle::ToString() const
{
    return FString::Printf(TEXT("[%d:%d]"), Index, Generation);
}
