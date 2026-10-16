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

#include "ArcCraftOutputDelivery_EntityItemStore.generated.h"

/**
 * Delivery policy that routes crafted items into a FArcMassItemStoreFragment (or derived)
 * on either the station entity or the instigator entity.
 *
 * Used for player simple-craft: StoreType = FArcMassItemStoreFragment, bDeliverToInstigator = false
 * (when Instigator == Station == player entity this is equivalent to player inventory).
 *
 * bDeliverToInstigator = true routes output to the instigator (e.g. a workbench that
 * pushes crafted items straight into the player's backpack instead of its own hopper).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Item Store", Tooltip = "Delivery policy that routes crafted items into a FArcMassItemStoreFragment on either the station entity or the instigator entity."))
struct ARCCRAFT_API FArcCraftOutputDelivery_EntityItemStore : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/**
	 * The item-store fragment type to target on the delivery entity.
	 * Must be FArcMassItemStoreFragment or a derived type.
	 */
	UPROPERTY(EditAnywhere, Category = "Delivery",
		meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment", AllowAbstract = "false"))
	TObjectPtr<UScriptStruct> StoreType = nullptr;

	/**
	 * When true, output is delivered to InstigatorEntity.
	 * When false (default), output is delivered to StationEntity.
	 *
	 * For player simple-craft (Instigator == Station), this flag has no effect.
	 */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	bool bDeliverToInstigator = false;

	FArcCraftOutputDelivery_EntityItemStore();

	virtual EArcCraftDeliveryDisposition DeliverOutput_Mass(
		FMassExecutionContext& Context,
		FMassEntityManager& EntityManager,
		FMassEntityHandle StationEntity,
		FMassEntityHandle InstigatorEntity,
		const FGuid& EntryId,
		const TArray<FArcItemSpec>& Specs) const override;

	virtual ~FArcCraftOutputDelivery_EntityItemStore() override = default;
};
