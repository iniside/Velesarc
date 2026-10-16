// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcMassAbilityCost.generated.h"

struct FMassEntityManager;
class UArcAbilityDefinition;
class UArcEffectDefinition;

UENUM(BlueprintType)
enum class EArcCostFailReason : uint8 { Insufficient = 0, Unsatisfied = 1 };

struct ARCMASSABILITIES_API FArcCostCheckResult
{
    bool  bCanAfford = true;
    float RequiredAmount = 0.f;
    float CurrentAmount = 0.f;
    float MissingAmount = 0.f;   // FMath::Max(0, RequiredAmount - CurrentAmount)
    EArcCostFailReason Reason = EArcCostFailReason::Insufficient;
};

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
    virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const PURE_VIRTUAL(FArcMassAbilityCost::CheckCost, return true;);
    virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const PURE_VIRTUAL(FArcMassAbilityCost::ApplyCost,);
};

USTRUCT(BlueprintType, meta=(DisplayName="Apply Effect"))
struct ARCMASSABILITIES_API FArcMassAbilityCost_ApplyEffect : public FArcMassAbilityCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TObjectPtr<UArcEffectDefinition> CostEffect = nullptr;

    virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const override;
    virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;
};
