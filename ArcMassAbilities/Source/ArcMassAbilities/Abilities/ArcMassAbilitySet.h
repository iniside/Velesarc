// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcMassAbilitySet.generated.h"

class UArcAbilityDefinition;

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcMassAbilitySetEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TObjectPtr<UArcAbilityDefinition> AbilityDefinition;

    UPROPERTY(EditAnywhere, meta = (Categories = "InputTag"))
    FGameplayTag InputTag;
};

UCLASS(BlueprintType)
class ARCMASSABILITIES_API UArcMassAbilitySet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Abilities")
    TArray<FArcMassAbilitySetEntry> Abilities;
};
