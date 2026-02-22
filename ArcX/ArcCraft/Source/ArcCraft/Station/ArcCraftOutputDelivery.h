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

#include "ArcCraftOutputDelivery.generated.h"

class UArcCraftStationComponent;
class UArcItemsStoreComponent;

/**
 * Base instanced struct for crafted item delivery.
 * Defines how a crafting station delivers finished items.
 *
 * Override this to create custom delivery methods (e.g. drop on ground,
 * send to a specific container, mail to player, etc.).
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/**
	 * Deliver a crafted item.
	 * @return true if the item was successfully delivered.
	 */
	virtual bool DeliverOutput(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& OutputSpec,
		const UObject* Instigator) const;

	virtual ~FArcCraftOutputDelivery() = default;
};

/**
 * Stores crafted items on the station's owner actor in a UArcItemsStoreComponent.
 * Player can pick them up later.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Store On Station"))
struct ARCCRAFT_API FArcCraftOutputDelivery_StoreOnStation : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/** Which items store class to use on the station's owner actor for output. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	TSubclassOf<UArcItemsStoreComponent> OutputStoreClass;

	virtual bool DeliverOutput(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& OutputSpec,
		const UObject* Instigator) const override;

	virtual ~FArcCraftOutputDelivery_StoreOnStation() override = default;
};

/**
 * Delivers crafted items directly to the instigator's (player's) inventory.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "To Instigator Inventory"))
struct ARCCRAFT_API FArcCraftOutputDelivery_ToInstigator : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	/** Which items store class to look for on the instigator actor. */
	UPROPERTY(EditAnywhere, Category = "Delivery")
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	virtual bool DeliverOutput(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& OutputSpec,
		const UObject* Instigator) const override;

	virtual ~FArcCraftOutputDelivery_ToInstigator() override = default;
};
