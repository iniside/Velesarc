// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ArcAbilityHandle.h"
#include "ArcMassMechanics/Costs/ArcCostTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/EntityHandle.h"
#include "ArcRPGItemCost.generated.h"

struct FArcItemData;
struct FMassEntityManager;

namespace ArcRPGItemCost
{
	ARCRPGMECHANICS_API float GetEntropyGenerated(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle);
}

USTRUCT(BlueprintType, meta = (ToolTip = "magic.yaml skill_efficiency: Skill * ((Attribute A + Attribute B) / Divisor), read live from the caster."))
struct ARCRPGMECHANICS_API FArcRPGCostSkillEfficiency
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Efficiency")
	FArcAttributeRef Skill;

	UPROPERTY(EditAnywhere, Category = "Efficiency")
	FArcAttributeRef AttributeA;

	UPROPERTY(EditAnywhere, Category = "Efficiency")
	FArcAttributeRef AttributeB;

	UPROPERTY(EditAnywhere, Category = "Efficiency", meta = (ClampMin = "0", ToolTip = "magic.yaml skill_efficiency.divisor."))
	float Divisor = 0.f;

	float Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const;
};

USTRUCT(BlueprintType, meta = (ToolTip = "magic.yaml cost_formulas: max(Floor, 1 - Efficiency * Skill K - Attribute modifier * Attribute K)."))
struct ARCRPGMECHANICS_API FArcRPGCostChannelingFactor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Channeling")
	FArcRPGCostSkillEfficiency Efficiency;

	UPROPERTY(EditAnywhere, Category = "Channeling", meta = (ClampMin = "0"))
	float Floor = 0.f;

	UPROPERTY(EditAnywhere, Category = "Channeling")
	float SkillK = 0.f;

	UPROPERTY(EditAnywhere, Category = "Channeling")
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, Category = "Channeling")
	float AttributeK = 0.f;

	float Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const;
};

USTRUCT(BlueprintType, meta = (ToolTip = "magic.yaml weaving.clean_entropy: max(Floor, Start - Efficiency * Skill K)."))
struct ARCRPGMECHANICS_API FArcRPGCostWeavingFactor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Weaving")
	FArcRPGCostSkillEfficiency Efficiency;

	UPROPERTY(EditAnywhere, Category = "Weaving", meta = (ClampMin = "0"))
	float Floor = 0.f;

	UPROPERTY(EditAnywhere, Category = "Weaving")
	float Start = 0.f;

	UPROPERTY(EditAnywhere, Category = "Weaving")
	float SkillK = 0.f;

	float Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Ability Cost (RPG)", ToolTip = "Source item Ability Stats at the ability level; 0 does nothing. Energy Cost * Energy Channeling * max(1, Cascade Multiplier) ^ stacks * max(0, 1 + Energy Cost Pct): needs Energy, paid through RemoveEnergy. Entropy Cost (flat): needs Entropy, paid through RemoveEntropy. Stamina Cost: needs Stamina, paid through RemoveStamina. Health Cost: needs Health, paid through DegenDamage. Entropy Generated * Weaving (item has ArcSpell.Woven) * Entropy Channeling * max(0, 1 + Entropy Generation Pct): added through AddEntropy, never blocks. Reports the first failure in the order Energy, Entropy, Health, Stamina. No source item, or both Entropy Cost and Entropy Generated, fails the cost. Surge predicts with the generated amount."))
struct ARCRPGMECHANICS_API FArcRPGCost_Ability : public FArcMassAbilityCost
{
	GENERATED_BODY()

	FArcRPGCost_Ability();

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (ToolTip = "magic.yaml cost_formulas.energy channeling factor, multiplies the Energy Cost."))
	FArcRPGCostChannelingFactor EnergyChanneling;

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (ToolTip = "magic.yaml cost_formulas.entropy channeling factor, multiplies the Entropy Generated. Entropy Cost has no channeling factor."))
	FArcRPGCostChannelingFactor EntropyChanneling;

	UPROPERTY(EditAnywhere, Category = "Cost", meta = (ToolTip = "magic.yaml weaving.clean_entropy (D-92 10b), multiplies the Entropy Generated when the source item has ArcSpell.Woven."))
	FArcRPGCostWeavingFactor Weaving;

	virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const override;
	virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;

	float GetEntropyGenerated(const FArcMassAbilityCostContext& Context, const FArcItemData& Item) const;

private:
	float GetEnergyCost(const FArcMassAbilityCostContext& Context, const FArcItemData& Item) const;
};
