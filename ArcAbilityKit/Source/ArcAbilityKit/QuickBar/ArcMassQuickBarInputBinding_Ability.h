// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "QuickBar/ArcMassQuickBarInputBinding.h"

#include "ArcMassQuickBarInputBinding_Ability.generated.h"

/**
 * Ability-flavoured quickbar input binding.
 *
 * Stamps the inherited InputTag onto the FArcActiveAbility entries of every
 * Mass ability granted by the slot's item. If BindingRequiredTags is non-empty,
 * only abilities whose UArcAbilityDefinition::AbilityTags satisfies HasAll on
 * the filter are bound (lets one item grant several abilities and route them
 * to distinct input slots).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Bar Input Bind (Ability)"))
struct ARCABILITYKIT_API FArcMassQuickBarInputBinding_Ability : public FArcMassQuickBarInputBinding
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "Ability"))
    FGameplayTagContainer BindingRequiredTags;

    virtual void Bind(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData) const override;
    virtual void Unbind(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData) const override;
};
