// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcAbilityHandle.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAbilityHandle
{
    GENERATED_BODY()

    FArcAbilityHandle() = default;

    static FArcAbilityHandle Make(int32 InIndex, int32 InGeneration);

    bool IsValid() const { return Index != INDEX_NONE; }
    int32 GetIndex() const { return Index; }
    int32 GetGeneration() const { return Generation; }

    bool operator==(const FArcAbilityHandle& Other) const;
    bool operator!=(const FArcAbilityHandle& Other) const;

    friend uint32 GetTypeHash(const FArcAbilityHandle& Handle);

    FString ToString() const;

private:
    FArcAbilityHandle(int32 InIndex, int32 InGeneration)
        : Index(InIndex), Generation(InGeneration) {}

    UPROPERTY()
    int32 Index = INDEX_NONE;

    UPROPERTY()
    int32 Generation = 0;
};
