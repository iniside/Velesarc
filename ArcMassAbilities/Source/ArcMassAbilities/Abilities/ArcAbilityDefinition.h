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
    /** The StateTree graph that drives this ability's logic each tick while active (allocated per activation, ticked by ArcAbilityStateTreeTickProcessor). An ability with no StateTree still activates and grants its tags, but nothing ever ends it and its cooldown is never applied - both live in StateTree tasks. */
    UPROPERTY(EditAnywhere, Category = "Ability", meta=(Schema="/Script/ArcMassAbilities.ArcAbilityStateTreeSchema"))
    TObjectPtr<UStateTree> StateTree;

    /** Identity tags describing this ability. Used to match the ability against other abilities' CancelAbilitiesWithTags / BlockAbilitiesWithTags, and by external systems (e.g. quick-bar input bindings) to select it. These are NOT granted to the owner — see GrantTagsWhileActive. */
    UPROPERTY(EditAnywhere, Category = "Tags", meta = (Categories = "Ability"))
    FGameplayTagContainer AbilityTags;

    /** The owner must have ALL of these tags for the ability to activate. Empty = no requirement. (Checked against the entity's owned tags via HasAll.) */
    UPROPERTY(EditAnywhere, Category = "Activation")
    FGameplayTagContainer ActivationRequiredTags;

    /** Activation is blocked if the owner has ANY of these tags. Empty = never blocked. (Checked against the entity's owned tags via HasAny.) */
    UPROPERTY(EditAnywhere, Category = "Activation")
    FGameplayTagContainer ActivationBlockedTags;

    /** When true, the input processor skips its built-in cost/cooldown GATE and Commit skips its cooldown APPLY — the ability's StateTree is expected to gate/apply via the CheckCost/CheckCooldown/ApplyCooldown tasks instead. */
    UPROPERTY(EditAnywhere, Category = "Activation")
    bool bGateInStateTree = false;

    /** Tags granted to the owner's owned-tag set while this ability is active, and removed automatically when it ends or is cancelled. */
    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer GrantTagsWhileActive;

    /** On activation, any already-active ability whose AbilityTags match ANY of these tags is cancelled (its GrantTagsWhileActive are removed and its StateTree stopped). */
    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer CancelAbilitiesWithTags;

    /** While this ability is active, it prevents activation of any other ability whose AbilityTags match ANY of these tags. */
    UPROPERTY(EditAnywhere, Category = "Tags")
    FGameplayTagContainer BlockAbilitiesWithTags;

    /** Optional resource cost (a concrete FArcMassAbilityCost subclass). Checked before activation and committed by ArcAbilityTask_Commit; insufficient resources block activation. Empty = free. */
    UPROPERTY(EditAnywhere, Category = "Cost", meta=(BaseStruct="/Script/ArcMassAbilities.ArcMassAbilityCost", ExcludeBaseStruct))
    FInstancedStruct Cost;

    /** Optional cooldown (a concrete FArcAbilityCooldown subclass). An active cooldown blocks activation; applied on commit. Empty = no cooldown. */
    UPROPERTY(EditAnywhere, Category = "Cooldown", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAbilityCooldown", ExcludeBaseStruct))
    FInstancedStruct Cooldown;

    /** Pluggable, additive activation gates (concrete FArcAbilityActivationRequirement subclasses; external plugins may subclass). Each must return true for the ability to activate. These run AFTER the built-in tag/cost/cooldown gates and cannot gate before cost (a deliberate v1 constraint). Empty = no extra requirements. */
    UPROPERTY(EditAnywhere, Category="Activation", meta=(BaseStruct="/Script/ArcMassAbilities.ArcAbilityActivationRequirement", ExcludeBaseStruct))
    TArray<FInstancedStruct> ActivationRequirements;
};
