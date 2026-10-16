// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Abilities/ArcAbilityHandle.h"

struct FMassEntityManager;
struct FArcItemData;
struct FArcAbilitySnapshot;
class UArcAbilityDefinition;

/**
 * Read-only extraction of an item's granted ability state into an FArcAbilitySnapshot, for UI.
 *
 * Single place that bridges ArcMassItems (item data) + ArcAbilityKit (granted-abilities instance)
 * + ArcMassAbilities (live ability fragments, cooldown, gate sequence). Both the QuickBar
 * StructModel VM-fill and any future test call ResolveAbilityForItem; the input-processor gate
 * sequence is mirrored here as a read-only predicate (CanActivate).
 *
 * The entity manager is intentionally NON-const: ArcAbilities::FindAbility and
 * FArcAbilityCooldown::CheckCooldown both take a non-const FMassEntityManager&.
 *
 * Every read null-checks before deref: an entity with a quick bar need not carry ability
 * fragments (they are seeded by the ability trait, not the quickbar trait). Any miss -- null item,
 * no granted-abilities instance, empty handle list, no live ability, missing definition --
 * yields a default snapshot (bHasAbility == false), never a crash.
 */
namespace ArcAbilityResolver
{
	/** Extract the ability snapshot for the FIRST ability granted by ItemData (single ability per slot). */
	FArcAbilitySnapshot ResolveAbilityForItem(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const FArcItemData* ItemData);

	/**
	 * Read-only mirror of the UArcAbilityInputProcessor activation gate sequence
	 * (Processors/ArcAbilityInputProcessor.cpp:127-245). Returns true if the ability would pass
	 * every built-in gate this frame.
	 *
	 * bGateInStateTree abilities intentionally SKIP the cost/cooldown gates here (exactly as the
	 * processor does) so the UI does not falsely show them as blocked -- those abilities gate
	 * inside their StateTree instead.
	 */
	bool CanActivate(FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		FArcAbilityHandle Handle,
		const UArcAbilityDefinition* Definition);
}
