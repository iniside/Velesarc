// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Costs/ArcCostTypes.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Attributes/ArcInstantModifier.h"
#include "Effects/ArcEffectTypes.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Mass/EntityManager.h"

namespace ArcCosts
{
	bool CheckResource(const FArcMassAbilityCostContext& Context, const FArcAttributeRef& Resource, const FArcAttributeRef& Meta, float Required, FArcCostCheckResult* OutResult)
	{
		if (Required <= 0.f)
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Cost %ls %ls on %ls: free, amount %.3f",
				*GetNameSafe(Context.Definition), *Resource.PropertyName.ToString(), *Context.Entity.DebugGetDescription(), Required);
			if (OutResult)
			{
				*OutResult = FArcCostCheckResult();
			}
			return true;
		}

		const FArcAttribute* ResourceAttribute = ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Resource);
		const FArcAttribute* MetaAttribute = ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Meta);
		if (!ResourceAttribute || !MetaAttribute)
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Cost %ls %ls on %ls: unsatisfied, amount %.3f, entity lacks the %ls attribute",
				*GetNameSafe(Context.Definition), *Resource.PropertyName.ToString(), *Context.Entity.DebugGetDescription(), Required,
				ResourceAttribute ? TEXT("meta") : TEXT("resource"));
			if (OutResult)
			{
				*OutResult = FArcCostCheckResult();
				OutResult->bCanAfford = false;
				OutResult->Reason = EArcCostFailReason::Unsatisfied;
			}
			return false;
		}

		const float Current = ResourceAttribute->GetCurrentValue();
		const bool bCanAfford = Current >= Required;
		UE_LOGF(LogArcMassMechanics, Verbose, "Cost %ls %ls on %ls: %ls, amount %.3f, has %.3f",
			*GetNameSafe(Context.Definition), *Resource.PropertyName.ToString(), *Context.Entity.DebugGetDescription(),
			bCanAfford ? TEXT("affordable") : TEXT("insufficient"), Required, Current);
		if (OutResult)
		{
			OutResult->bCanAfford = bCanAfford;
			OutResult->RequiredAmount = Required;
			OutResult->CurrentAmount = Current;
			OutResult->MissingAmount = FMath::Max(0.f, Required - Current);
			OutResult->Reason = EArcCostFailReason::Insufficient;
		}
		return bCanAfford;
	}

	void PayIntoMeta(const FArcMassAbilityCostContext& Context, const FArcAttributeRef& Meta, float Amount)
	{
		if (!Meta.IsValid() || Amount <= 0.f)
		{
			return;
		}

		UE_LOGF(LogArcMassMechanics, Verbose, "Cost %ls on %ls: paid %.3f into %ls",
			*GetNameSafe(Context.Definition), *Context.Entity.DebugGetDescription(), Amount, *Meta.PropertyName.ToString());
		ArcAttributes::EnqueueInstantModifier(Context.EntityManager, Context.Entity, Meta, EArcModifierOp::Add, Amount,
			EArcAttributeOpType::Execute, Context.Entity);
	}
}

bool FArcCost_Set::CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult) const
{
	for (const FInstancedStruct& Entry : Costs)
	{
		const FArcMassAbilityCost* Cost = Entry.GetPtr<FArcMassAbilityCost>();
		if (!Cost)
		{
			continue;
		}

		FArcCostCheckResult EntryResult;
		if (!Cost->CheckCost(Context, &EntryResult))
		{
			if (OutResult)
			{
				*OutResult = EntryResult;
				OutResult->bCanAfford = false;
			}
			return false;
		}
	}

	if (OutResult)
	{
		*OutResult = FArcCostCheckResult();
	}
	return true;
}

void FArcCost_Set::ApplyCost(const FArcMassAbilityCostContext& Context) const
{
	for (const FInstancedStruct& Entry : Costs)
	{
		if (const FArcMassAbilityCost* Cost = Entry.GetPtr<FArcMassAbilityCost>())
		{
			Cost->ApplyCost(Context);
		}
	}
}
