// Copyright Lukasz Baran. All Rights Reserved.

#include "Models/Abilities/ArcAbilityResolver.h"

#include "ViewModels/Abilities/ArcAbilityViewModel.h"

#include "Items/ArcItemsHelpers.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityCooldown.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "Abilities/ArcAbilityActivationRequirement.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"

#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcAbilityResolver_Local
{
	// Mirrors the BlockAbilitiesWithTags scan in UArcAbilityInputProcessor::SignalEntities
	// (ArcAbilityInputProcessor.cpp:149-173): an ability is blocked if any OTHER currently-active
	// ability declares BlockAbilitiesWithTags that intersects this Definition's AbilityTags.
	bool IsBlockedByOtherActives(const FArcAbilityCollectionFragment& Collection,
		const UArcAbilityDefinition& Definition)
	{
		for (const FArcActiveAbility& Active : Collection.Abilities)
		{
			if (!Active.bIsActive || !Active.Definition)
			{
				continue;
			}
			if (Active.Definition->BlockAbilitiesWithTags.Num() > 0
				&& Definition.AbilityTags.HasAny(Active.Definition->BlockAbilitiesWithTags))
			{
				return true;
			}
		}
		return false;
	}
}

namespace ArcAbilityResolver
{
	bool CanActivate(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcAbilityHandle Handle,
		const UArcAbilityDefinition* Definition)
	{
		if (!Definition)
		{
			return false;
		}

		FMassEntityView EntityView(EntityManager, Entity);

		// Owned tags are a sparse fragment. The input processor `continue`s (skips activation this
		// frame) when the owned-tags fragment is absent (ArcAbilityInputProcessor.cpp:102-106) --
		// i.e. it fails CLOSED. Mirror that: no owned-tags fragment => cannot activate.
		const FArcOwnedTagsFragment* OwnedTagsPtr = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
		if (!OwnedTagsPtr)
		{
			return false;
		}
		const FArcTagCountContainer& OwnedTags = OwnedTagsPtr->Tags;

		// Gate 1 -- ActivationRequiredTags (ArcAbilityInputProcessor.cpp:127-136).
		if (Definition->ActivationRequiredTags.Num() > 0
			&& !OwnedTags.HasAll(Definition->ActivationRequiredTags))
		{
			return false;
		}

		// Gate 2 -- ActivationBlockedTags (ArcAbilityInputProcessor.cpp:138-147).
		if (Definition->ActivationBlockedTags.Num() > 0
			&& OwnedTags.HasAny(Definition->ActivationBlockedTags))
		{
			return false;
		}

		// Gate 3 -- other-active BlockAbilitiesWithTags (ArcAbilityInputProcessor.cpp:149-173).
		// Needs the sparse ability-collection fragment to iterate the entity's active abilities.
		const FArcAbilityCollectionFragment* CollectionPtr = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
		if (CollectionPtr
			&& ArcAbilityResolver_Local::IsBlockedByOtherActives(*CollectionPtr, *Definition))
		{
			return false;
		}

		// Gates 4 & 5 -- cost + cooldown, ONLY when the ability does NOT gate inside its StateTree
		// (ArcAbilityInputProcessor.cpp:175-222). bGateInStateTree abilities intentionally skip these
		// here so the UI does not falsely report them blocked; they gate inside the tree instead.
		if (!Definition->bGateInStateTree)
		{
			// Gate 4 -- cost (ArcAbilityInputProcessor.cpp:175-198).
			if (Definition->Cost.IsValid())
			{
				const FArcMassAbilityCost* CostPtr = Definition->Cost.GetPtr<FArcMassAbilityCost>();
				if (CostPtr)
				{
					FArcMassAbilityCostContext CostCtx{EntityManager, Entity, Handle, Definition};
					FArcCostCheckResult CostResult;
					if (!CostPtr->CheckCost(CostCtx, &CostResult))
					{
						return false;
					}
				}
			}

			// Gate 5 -- cooldown (ArcAbilityInputProcessor.cpp:200-222).
			if (Definition->Cooldown.IsValid())
			{
				const FArcAbilityCooldown* CooldownPtr = Definition->Cooldown.GetPtr<FArcAbilityCooldown>();
				if (CooldownPtr)
				{
					FArcCooldownCheckResult CdResult;
					if (!CooldownPtr->CheckCooldown(EntityManager, Entity, Handle, &CdResult))
					{
						return false;
					}
				}
			}
		}

		// Gate 6 -- pluggable ActivationRequirements (ArcAbilityInputProcessor.cpp:224-245).
		for (const FInstancedStruct& RequirementStruct : Definition->ActivationRequirements)
		{
			const FArcAbilityActivationRequirement* Requirement = RequirementStruct.GetPtr<FArcAbilityActivationRequirement>();
			if (Requirement != nullptr)
			{
				FArcAbilityActivationContext ReqCtx{EntityManager, Entity, Handle, Definition, EntityView};
				if (!Requirement->Check(ReqCtx))
				{
					return false;
				}
			}
		}

		return true;
	}

	FArcAbilitySnapshot ResolveAbilityForItem(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcItemData* ItemData)
	{
		FArcAbilitySnapshot Snapshot;

		// a. Null item -> no ability.
		if (!ItemData)
		{
			return Snapshot;
		}

		// b. No granted-abilities instance on the item -> no ability.
		const FArcItemInstance_GrantedMassAbilities* GrantedInstance =
			ArcItemsHelper::FindInstance<FArcItemInstance_GrantedMassAbilities>(ItemData);
		if (!GrantedInstance)
		{
			return Snapshot;
		}

		// c. Empty handle list -> no ability. Take the FIRST handle (single ability per slot).
		if (GrantedInstance->GrantedAbilities.Num() == 0)
		{
			return Snapshot;
		}
		const FArcAbilityHandle Handle = GrantedInstance->GrantedAbilities[0];

		// d. No live ability for that handle on this entity -> no ability.
		const FArcActiveAbility* Ability = ArcAbilities::FindAbility(EntityManager, Entity, Handle);
		if (!Ability)
		{
			return Snapshot;
		}

		// e. Populate from the live ability.
		Snapshot.bHasAbility = true;
		Snapshot.bIsActive = Ability->bIsActive;
		Snapshot.InputTag = Ability->InputTag;

		const UArcAbilityDefinition* Definition = Ability->Definition;
		if (Definition)
		{
			Snapshot.AbilityTags = Definition->AbilityTags;
		}

		// f. Cooldown (guard definition + a valid cooldown struct).
		if (Definition && Definition->Cooldown.IsValid())
		{
			const FArcAbilityCooldown* CooldownPtr = Definition->Cooldown.GetPtr<FArcAbilityCooldown>();
			if (CooldownPtr)
			{
				FArcCooldownCheckResult CdResult;
				CooldownPtr->CheckCooldown(EntityManager, Entity, Handle, &CdResult);
				Snapshot.CooldownRemaining = CdResult.RemainingSeconds;
				Snapshot.CooldownDuration = CdResult.MaxCooldownSeconds;
				Snapshot.CooldownPercent = (CdResult.MaxCooldownSeconds > 0.f)
					? (CdResult.RemainingSeconds / CdResult.MaxCooldownSeconds)
					: 0.f;
			}
		}

		// g. Can-activate gate (read-only mirror of the input processor).
		Snapshot.bCanActivate = CanActivate(EntityManager, Entity, Handle, Definition);

		return Snapshot;
	}
}
