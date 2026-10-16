// Copyright Lukasz Baran. All Rights Reserved.

#include "Costs/ArcRPGItemCost.h"

#include "ArcRPGMechanicsLogs.h"
#include "ArcRPGMechanicsTags.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "ArcMassDamageSystem/ArcAttributeSet_Health.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEnergy.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Attributes/ArcAttributeSet_RPGHealthExtension.h"
#include "Attributes/ArcAttributeSet_RPGStamina.h"
#include "Attributes/ArcAttributeSet_RPGSkills.h"
#include "Attributes/Handlers/ArcRPGCoreDerivation.h"
#include "Costs/ArcRPGAttributeSet_CostAffixes.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcRPGItemFragment_AbilityStats.h"
#include "Fragments/ArcRPGItemFragment_CascadingCost.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Mass/EntityManager.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Sources/ArcSourceItemResolver.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcRPGItemCostPrivate
{
	const FArcRPGCost_Ability* FindAbilityCost(const FInstancedStruct& Cost)
	{
		if (const FArcRPGCost_Ability* AbilityCost = Cost.GetPtr<FArcRPGCost_Ability>())
		{
			return AbilityCost;
		}

		if (const FArcCost_Set* Set = Cost.GetPtr<FArcCost_Set>())
		{
			for (const FInstancedStruct& Entry : Set->Costs)
			{
				if (const FArcRPGCost_Ability* AbilityCost = FindAbilityCost(Entry))
				{
					return AbilityCost;
				}
			}
		}
		return nullptr;
	}

	FArcRPGCostSkillEfficiency MakeEfficiency(const FArcAttributeRef& Skill, const FArcAttributeRef& AttributeA, const FArcAttributeRef& AttributeB, float Divisor)
	{
		FArcRPGCostSkillEfficiency Efficiency;
		Efficiency.Skill = Skill;
		Efficiency.AttributeA = AttributeA;
		Efficiency.AttributeB = AttributeB;
		Efficiency.Divisor = Divisor;
		return Efficiency;
	}

	float ApplyAffix(const FArcMassAbilityCostContext& Context, const FArcAttributeRef& Affix, float Amount)
	{
		return FMath::Max(0.f, Amount * (1.f + ArcAttributes::GetValue(Context.EntityManager, Context.Entity, Affix)));
	}

	bool IsWoven(const FArcItemData& Item)
	{
		const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(&Item);
		return TagsFragment && TagsFragment->ItemTags.HasTag(ArcRPGMechanicsTags::SpellWoven);
	}

	const FArcItemData* FindPayableItem(const FArcMassAbilityCostContext& Context)
	{
		const FArcItemData* Item = ArcAbilityKit::FindSourceItem(Context.EntityManager, Context.Entity, Context.AbilityHandle);
		if (!Item)
		{
			UE_LOGF(LogArcRPGMechanics, Verbose, "RPG ability cost %ls on %ls: no source item", *GetNameSafe(Context.Definition),
				*Context.Entity.DebugGetDescription());
			return nullptr;
		}

		const float EntropyCost = Item->GetValue(FArcRPGItemFragment_AbilityStats::GetEntropyCostData());
		const float EntropyGenerated = Item->GetValue(FArcRPGItemFragment_AbilityStats::GetEntropyGeneratedData());
		if (EntropyCost > 0.f && EntropyGenerated > 0.f)
		{
			UE_LOGF(LogArcRPGMechanics, Error, "RPG ability cost %ls on %ls: source item %ls has both Entropy Cost %.3f and Entropy Generated %.3f",
				*GetNameSafe(Context.Definition), *Context.Entity.DebugGetDescription(), *Item->GetItemDefinitionId().ToString(),
				EntropyCost, EntropyGenerated);
			return nullptr;
		}
		return Item;
	}
}

namespace ArcRPGItemCost
{
	float GetEntropyGenerated(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle)
	{
		const FArcActiveAbility* Ability = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
		if (!Ability || !Ability->Definition)
		{
			return 0.f;
		}

		const FArcRPGCost_Ability* Cost = ArcRPGItemCostPrivate::FindAbilityCost(Ability->Definition->Cost);
		if (!Cost)
		{
			return 0.f;
		}

		const FArcMassAbilityCostContext Context{EntityManager, Entity, Handle, Ability->Definition};
		const FArcItemData* Item = ArcRPGItemCostPrivate::FindPayableItem(Context);
		return Item ? Cost->GetEntropyGenerated(Context, *Item) : 0.f;
	}
}

float FArcRPGCostSkillEfficiency::Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const
{
	const float SkillValue = ArcAttributes::GetValue(EntityManager, Entity, Skill);
	const float A = ArcAttributes::GetValue(EntityManager, Entity, AttributeA);
	const float B = ArcAttributes::GetValue(EntityManager, Entity, AttributeB);
	return SkillValue * (A + B) / FMath::Max(Divisor, SMALL_NUMBER);
}

float FArcRPGCostChannelingFactor::Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const
{
	const float Modifier = ArcRPGCoreDerivation::CalculateModifier(ArcAttributes::GetValue(EntityManager, Entity, Attribute));
	return FMath::Max(Floor, 1.f - Efficiency.Evaluate(EntityManager, Entity) * SkillK - Modifier * AttributeK);
}

float FArcRPGCostWeavingFactor::Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Entity) const
{
	return FMath::Max(Floor, Start - Efficiency.Evaluate(EntityManager, Entity) * SkillK);
}

FArcRPGCost_Ability::FArcRPGCost_Ability()
{
	EnergyChanneling.Efficiency = ArcRPGItemCostPrivate::MakeEfficiency(FArcAttributeSet_RPGSkills::GetChannelingAttribute(),
		FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute(), FArcAttributeSet_RPGCoreStats::GetIntelligenceAttribute(), 20.f);
	EnergyChanneling.Floor = 0.4f;
	EnergyChanneling.SkillK = 0.005f;
	EnergyChanneling.Attribute = FArcAttributeSet_RPGCoreStats::GetIntelligenceAttribute();
	EnergyChanneling.AttributeK = 0.04f;

	EntropyChanneling.Efficiency = ArcRPGItemCostPrivate::MakeEfficiency(FArcAttributeSet_RPGSkills::GetChannelingAttribute(),
		FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute(), FArcAttributeSet_RPGCoreStats::GetIntelligenceAttribute(), 20.f);
	EntropyChanneling.Floor = 0.5f;
	EntropyChanneling.SkillK = 0.003f;
	EntropyChanneling.Attribute = FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute();
	EntropyChanneling.AttributeK = 0.03f;

	Weaving.Efficiency = ArcRPGItemCostPrivate::MakeEfficiency(FArcAttributeSet_RPGSkills::GetWeavingAttribute(),
		FArcAttributeSet_RPGCoreStats::GetWillpowerAttribute(), FArcAttributeSet_RPGCoreStats::GetIntelligenceAttribute(), 20.f);
	Weaving.Floor = 1.f;
	Weaving.Start = 1.5f;
	Weaving.SkillK = 0.005f;
}

bool FArcRPGCost_Ability::CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult) const
{
	const FArcItemData* Item = ArcRPGItemCostPrivate::FindPayableItem(Context);
	if (!Item)
	{
		if (OutResult)
		{
			*OutResult = FArcCostCheckResult();
			OutResult->bCanAfford = false;
			OutResult->Reason = EArcCostFailReason::Unsatisfied;
		}
		return false;
	}

	if (!ArcCosts::CheckResource(Context, FArcAttributeSet_RPGEnergy::GetEnergyAttribute(), FArcAttributeSet_RPGEnergy::GetRemoveEnergyAttribute(),
		GetEnergyCost(Context, *Item), OutResult))
	{
		return false;
	}

	if (!ArcCosts::CheckResource(Context, FArcAttributeSet_RPGEntropy::GetEntropyAttribute(), FArcAttributeSet_RPGEntropy::GetRemoveEntropyAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetEntropyCostData()), OutResult))
	{
		return false;
	}

	if (!ArcCosts::CheckResource(Context, FArcAttributeSet_Health::GetHealthAttribute(), FArcAttributeSet_RPGHealthExtension::GetDegenDamageAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetHealthCostData()), OutResult))
	{
		return false;
	}

	if (!ArcCosts::CheckResource(Context, FArcAttributeSet_RPGStamina::GetStaminaAttribute(), FArcAttributeSet_RPGStamina::GetRemoveStaminaAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetStaminaCostData()), OutResult))
	{
		return false;
	}

	if (OutResult)
	{
		*OutResult = FArcCostCheckResult();
	}
	return true;
}

void FArcRPGCost_Ability::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
	const FArcItemData* Item = ArcRPGItemCostPrivate::FindPayableItem(Context);
	if (!Item)
	{
		return;
	}

	ArcCosts::PayIntoMeta(Context, FArcAttributeSet_RPGEnergy::GetRemoveEnergyAttribute(), GetEnergyCost(Context, *Item));
	ArcCosts::PayIntoMeta(Context, FArcAttributeSet_RPGEntropy::GetRemoveEntropyAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetEntropyCostData()));
	ArcCosts::PayIntoMeta(Context, FArcAttributeSet_RPGHealthExtension::GetDegenDamageAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetHealthCostData()));
	ArcCosts::PayIntoMeta(Context, FArcAttributeSet_RPGStamina::GetRemoveStaminaAttribute(),
		Item->GetValue(FArcRPGItemFragment_AbilityStats::GetStaminaCostData()));

	const FArcAttributeRef AddEntropy = FArcAttributeSet_RPGEntropy::GetAddEntropyAttribute();
	if (ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, AddEntropy))
	{
		ArcCosts::PayIntoMeta(Context, AddEntropy, GetEntropyGenerated(Context, *Item));
	}
}

float FArcRPGCost_Ability::GetEntropyGenerated(const FArcMassAbilityCostContext& Context, const FArcItemData& Item) const
{
	const float Base = Item.GetValue(FArcRPGItemFragment_AbilityStats::GetEntropyGeneratedData());
	if (Base <= 0.f)
	{
		return 0.f;
	}

	const float WeavingFactor = ArcRPGItemCostPrivate::IsWoven(Item) ? Weaving.Evaluate(Context.EntityManager, Context.Entity) : 1.f;
	const float Channeling = EntropyChanneling.Evaluate(Context.EntityManager, Context.Entity);
	const float Amount = ArcRPGItemCostPrivate::ApplyAffix(Context, FArcRPGAttributeSet_CostAffixes::GetEntropyGenerationPctAttribute(),
		Base * WeavingFactor * Channeling);
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG entropy generated %ls on %ls: base %.3f, weaving x%.4f, channeling x%.4f, after affix %.3f",
		*GetNameSafe(Context.Definition), *Context.Entity.DebugGetDescription(), Base, WeavingFactor, Channeling, Amount);
	return Amount;
}

float FArcRPGCost_Ability::GetEnergyCost(const FArcMassAbilityCostContext& Context, const FArcItemData& Item) const
{
	const float Base = Item.GetValue(FArcRPGItemFragment_AbilityStats::GetEnergyCostData());
	if (Base <= 0.f)
	{
		return 0.f;
	}

	int32 Stacks = 0;
	if (const FArcRPGItemInstance_CascadingCost* Cascade = ArcItemsHelper::FindInstance<FArcRPGItemInstance_CascadingCost>(&Item))
	{
		Stacks = Cascade->GetStacks();
	}

	const float CascadeMultiplier = FMath::Max(1.f, Item.GetValue(FArcRPGItemFragment_AbilityStats::GetCascadeMultiplierData()));
	const float CascadeFactor = FMath::Pow(CascadeMultiplier, static_cast<float>(Stacks));
	const float Channeling = EnergyChanneling.Evaluate(Context.EntityManager, Context.Entity);
	const float Amount = ArcRPGItemCostPrivate::ApplyAffix(Context, FArcRPGAttributeSet_CostAffixes::GetEnergyCostPctAttribute(),
		Base * Channeling * CascadeFactor);
	UE_LOGF(LogArcRPGMechanics, Verbose, "RPG energy cost %ls on %ls: base %.3f, channeling x%.4f, cascade x%.4f (%d stacks), after affix %.3f",
		*GetNameSafe(Context.Definition), *Context.Entity.DebugGetDescription(), Base, Channeling, CascadeFactor, Stacks, Amount);
	return Amount;
}
