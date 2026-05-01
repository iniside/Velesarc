// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "ScalableFloat.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcMassAbilityCost.generated.h"

struct FMassEntityManager;
class UArcAbilityDefinition;
class UArcEffectDefinition;

struct ARCMASSABILITIES_API FArcMassAbilityCostContext
{
    FMassEntityManager& EntityManager;
    FMassEntityHandle Entity;
    FArcAbilityHandle AbilityHandle;
    const UArcAbilityDefinition* Definition = nullptr;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcMassAbilityCost
{
    GENERATED_BODY()
    virtual ~FArcMassAbilityCost() = default;
    virtual bool CheckCost(const FArcMassAbilityCostContext& Context) const PURE_VIRTUAL(FArcMassAbilityCost::CheckCost, return true;);
    virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const PURE_VIRTUAL(FArcMassAbilityCost::ApplyCost,);
};

USTRUCT(BlueprintType, meta=(DisplayName="Apply Effect"))
struct ARCMASSABILITIES_API FArcMassAbilityCost_ApplyEffect : public FArcMassAbilityCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TObjectPtr<UArcEffectDefinition> CostEffect = nullptr;

    virtual bool CheckCost(const FArcMassAbilityCostContext& Context) const override;
    virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;
};

USTRUCT(BlueprintType, meta=(DisplayName="Instant Modifier"))
struct ARCMASSABILITIES_API FArcMassAbilityCost_InstantModifier : public FArcMassAbilityCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FArcAttributeRef Attribute;

    UPROPERTY(EditAnywhere)
    FScalableFloat Amount;

    virtual bool CheckCost(const FArcMassAbilityCostContext& Context) const override;
    virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;
};
