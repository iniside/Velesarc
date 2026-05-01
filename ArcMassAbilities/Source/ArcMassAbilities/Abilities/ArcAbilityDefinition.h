// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcAbilityDefinition.generated.h"

class UStateTree;

UCLASS(BlueprintType)
class ARCMASSABILITIES_API UArcAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Ability", meta=(Schema="/Script/ArcMassAbilities.ArcAbilityStateTreeSchema"))
    TObjectPtr<UStateTree> StateTree;

    UPROPERTY(EditAnywhere, Category = "Tags", meta = (Categories = "Ability"))
    FGameplayTagContainer AbilityTags;

    UPROPERTY(EditAnywhere, Category = "Activation")
    FGameplayTagContainer ActivationRequiredTags;

    UPROPERTY(EditAnywhere, Category = "Activation")
    FGameplayTagContainer ActivationBlockedTags;

    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer GrantTagsWhileActive;

    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer CancelAbilitiesWithTags;

    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer BlockAbilitiesWithTags;

    UPROPERTY(EditAnywhere, Category = "Cost", meta=(BaseStruct="/Script/ArcMassAbilities.ArcMassAbilityCost", ExcludeBaseStruct))
    FInstancedStruct Cost;

    UPROPERTY(EditAnywhere, Category = "Cooldown", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAbilityCooldown", ExcludeBaseStruct))
    FInstancedStruct Cooldown;
};
