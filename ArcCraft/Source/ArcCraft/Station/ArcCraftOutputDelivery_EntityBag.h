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
#include "ArcCraft/Station/ArcCraftOutputDelivery.h"

#include "ArcCraftOutputDelivery_EntityBag.generated.h"

/**
 * Delivery policy that routes crafted items through the bag API (FArcMassBagStoreFragment overlay):
 * each spec is added to the store AND placed into the first accepting free slot, in list order, with
 * exact rollback if no slot accepts.
 *
 * On a full bag nothing is spilled — the policy stops at the first spec that does not fit, snapshots it
 * and every later spec into the queue entry's HeldOutput, marks bOutputBlocked (pausing that station's queue), and registers with
 * UArcCraftOutputResumeSubsystem so crafting resumes when a slot frees. Because resolution can't be
 * known until the deferred Mass command runs, this policy returns DeferredResolve: its command owns
 * the advance-or-block decision (keyed by EntryId, index-shift safe).
 *
 * If the target entity has no bag overlay, the policy degrades gracefully to a plain store add so a
 * bag-less entity is never stranded (returns ResolvedSync in that case).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Bag", Tooltip = "Routes crafted items through the bag API; holds the item and pauses the station if the bag is full."))
struct ARCCRAFT_API FArcCraftOutputDelivery_EntityBag : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/**
	 * When true, output is delivered to InstigatorEntity's bag.
	 * When false (default), output is delivered to StationEntity's bag.
	 *
	 * For player hand-craft (Instigator == Station), this flag has no effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	bool bDeliverToInstigator = false;

	virtual EArcCraftDeliveryDisposition DeliverOutput_Mass(
		FMassExecutionContext& Context,
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const FGuid& EntryId,
		const TArray<FArcItemSpec>& Specs) const override;

	virtual ~FArcCraftOutputDelivery_EntityBag() override = default;
};
