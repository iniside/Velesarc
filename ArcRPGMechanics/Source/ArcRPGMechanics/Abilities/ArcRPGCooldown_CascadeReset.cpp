// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/ArcRPGCooldown_CascadeReset.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Sources/ArcSourceItemResolver.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"

#include "Fragments/ArcRPGItemFragment_AbilityStats.h"
#include "Fragments/ArcRPGItemFragment_CascadingCost.h"

void FArcRPGCooldown_CascadeReset::ApplyCooldown(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle, float* OutAppliedDuration) const
{
	// Resolve the source item — its SoftCooldownDuration is the timer length. No item = nothing to arm.
	const FArcItemData* ItemPtr = ArcAbilityKit::FindSourceItem(EntityManager, Entity, Handle);
	if (ItemPtr == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CascadeReset] ApplyCooldown: no resolvable source item entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return;
	}

	FArcAbilityCooldownFragment* Frag = FMassEntityView(EntityManager, Entity).GetFragmentDataPtr<FArcAbilityCooldownFragment>();
	if (Frag == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CascadeReset] ApplyCooldown: caster has no FArcAbilityCooldownFragment entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return;
	}

	const float SoftCooldownDuration = ItemPtr->GetValue(FArcRPGItemFragment_AbilityStats::GetSoftCooldownDurationData());
	if (OutAppliedDuration)
	{
		*OutAppliedDuration = SoftCooldownDuration;
	}

	// Idempotent find-or-refresh: a matching entry is refreshed in place; never blindly append (a duplicate would
	// let one entry expire early and reset the item's cascade stacks before the intended window).
	FArcCooldownEntry* ExistingEntry = Frag->ActiveCooldowns.FindByPredicate(
		[Handle](const FArcCooldownEntry& Entry)
		{
			return Entry.AbilityHandle == Handle;
		});

	if (ExistingEntry != nullptr)
	{
		ExistingEntry->RemainingTime = SoftCooldownDuration;
		ExistingEntry->MaxCooldownTime = SoftCooldownDuration;
		ExistingEntry->bBlocking = false;
	}
	else
	{
		FArcCooldownEntry& NewEntry = Frag->ActiveCooldowns.AddDefaulted_GetRef();
		NewEntry.AbilityHandle = Handle;
		NewEntry.RemainingTime = SoftCooldownDuration;
		NewEntry.MaxCooldownTime = SoftCooldownDuration;
		NewEntry.bBlocking = false;
	}
}

void FArcRPGCooldown_CascadeReset::OnExpired(FMassEntityManager& EntityManager, FMassEntityHandle Entity, FArcAbilityHandle Handle) const
{
	const FArcItemData* ItemPtr = ArcAbilityKit::FindSourceItem(EntityManager, Entity, Handle);
	if (ItemPtr == nullptr)
	{
		return;
	}

	FArcRPGItemInstance_CascadingCost* CascadingInstance =
		ArcItemsHelper::FindMutableInstance<FArcRPGItemInstance_CascadingCost>(ItemPtr);
	if (CascadingInstance != nullptr)
	{
		CascadingInstance->ResetStacks();
		// Server-authoritative bump so client-side version validation re-pulls the reset stacks.
		const_cast<FArcItemData*>(ItemPtr)->IncrementVersion();
	}
}
