/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemSpec.h"
#include "Mass/EntityHandle.h"

#include "ArcCraftOutputDelivery.generated.h"

struct FArcCraftOutputFragment;
struct FMassEntityManager;
struct FMassExecutionContext;

/**
 * Outcome of a DeliverOutput_Mass call, telling the processor who owns advancing the queue entry.
 *
 * - ResolvedSync:   the policy delivered (or queued a store add) without needing to relocate the
 *                   craft queue entry. The processor advances the completed entry itself.
 * - DeferredResolve: the policy's deferred command owns the advance-or-block decision (e.g. the bag
 *                    policy advances once when every spec was added, or holds the undelivered specs and
 *                    blocks on a full bag). The processor must NOT advance the entry — the command will.
 */
enum class EArcCraftDeliveryDisposition : uint8
{
	ResolvedSync,
	DeferredResolve
};

/**
 * Base instanced struct for crafted item delivery.
 * Defines how a crafting station delivers finished items.
 *
 * Override this to create custom delivery methods (e.g. drop on ground,
 * send to a specific container, mail to player, etc.).
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base instanced struct for crafted item delivery. Defines how a crafting station delivers finished items."))
struct ARCCRAFT_API FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/**
	 * Mass-side delivery from inside UArcCraftTickProcessor::Execute (worker thread).
	 * Specs are every output of one finished craft, main output first.
	 * Default impl pushes a deferred Set that appends each spec to FArcCraftOutputFragment::OutputItems
	 * on StationEntity. Override per delivery type (e.g. _EntityItemStore redirects to FArcMassItemStoreFragment).
	 *
	 * EntryId identifies the completed queue entry so a policy that resolves asynchronously (the bag
	 * policy) can relocate it inside its deferred command. The base/store impls ignore it and return
	 * ResolvedSync, leaving the processor to advance the entry.
	 */
	virtual EArcCraftDeliveryDisposition DeliverOutput_Mass(
		FMassExecutionContext& Context,
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const FGuid& EntryId,
		const TArray<FArcItemSpec>& Specs) const;

	virtual ~FArcCraftOutputDelivery() = default;
};

/**
 * Stores crafted items in the entity's FArcCraftOutputFragment. The actor-side
 * mirror retired with UArcItemsStoreComponent (plan step 8b-craft); items live
 * solely on the entity fragment.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Storage", Tooltip = "Stores crafted items in the entity's FArcCraftOutputFragment; items live solely on the entity fragment."))
struct ARCCRAFT_API FArcCraftOutputDelivery_EntityStore : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	virtual ~FArcCraftOutputDelivery_EntityStore() override = default;
};
