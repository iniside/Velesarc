// Copyright Lukasz Baran. All Rights Reserved.

#include "Triggers/ArcRPGTriggerConditions.h"

#include "Effects/ArcEffectContext.h"
#include "Sources/ArcAbilitySource_Item.h"

#define LOCTEXT_NAMESPACE "ArcRPGTriggerConditions"

bool FArcRPGTriggerCondition_SameSourceItem::IsMet(const FArcTriggerContext& Context) const
{
	if (Context.HitContext == nullptr || Context.HostContext == nullptr)
	{
		return false;
	}

	const FArcAbilitySource_Item* HitItem = Context.HitContext->SourceData.GetPtr<FArcAbilitySource_Item>();
	const FArcAbilitySource_Item* HostItem = Context.HostContext->SourceData.GetPtr<FArcAbilitySource_Item>();
	if (HitItem == nullptr || HostItem == nullptr)
	{
		return false;
	}

	return HitItem->ItemId.IsValid() && HitItem->ItemId == HostItem->ItemId;
}

void FArcRPGTriggerCondition_SameSourceItem::ValidateInOn(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("SameSourceItemNotIf", "Same Source Item needs an If trigger; On and While payloads carry no source data."));
}

void FArcRPGTriggerCondition_SameSourceItem::ValidateInWhile(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("SameSourceItemNotIf", "Same Source Item needs an If trigger; On and While payloads carry no source data."));
}

#undef LOCTEXT_NAMESPACE
