// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Attributes/ArcAttribute.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "ArcAbilityCostTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcTestAttributeCost : public FArcMassAbilityCost
{
	GENERATED_BODY()

	UPROPERTY()
	FArcAttributeRef Attribute;

	UPROPERTY()
	float Amount = 0.f;

	virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const override
	{
		const FArcAttribute* Resolved = ArcAttributes::ResolveAttribute(Context.EntityManager, Context.Entity, Attribute);
		if (!Resolved)
		{
			if (OutResult)
			{
				OutResult->bCanAfford = false;
				OutResult->Reason = EArcCostFailReason::Unsatisfied;
			}
			return false;
		}

		const bool bCanAfford = Resolved->BaseValue >= Amount;
		if (OutResult)
		{
			OutResult->bCanAfford = bCanAfford;
			OutResult->RequiredAmount = Amount;
			OutResult->CurrentAmount = Resolved->BaseValue;
			OutResult->MissingAmount = FMath::Max(0.f, Amount - Resolved->BaseValue);
			OutResult->Reason = EArcCostFailReason::Insufficient;
		}
		return bCanAfford;
	}

	virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override
	{
		const float Current = ArcAttributes::GetValue(Context.EntityManager, Context.Entity, Attribute);
		ArcAttributes::SetValue(Context.EntityManager, Context.Entity, Attribute, Current - Amount);
	}
};
