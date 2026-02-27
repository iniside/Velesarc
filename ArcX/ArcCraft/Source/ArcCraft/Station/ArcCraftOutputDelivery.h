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
class UArcCraftVisEntityComponent;
struct FArcCraftOutputFragment;

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

/**
 * Stores crafted items in the entity's FArcCraftOutputFragment.
 * When the actor is alive, items also appear in the actor's output store
 * (synced by UArcCraftVisEntityComponent). When the actor is despawned,
 * items persist in the entity fragment.
 *
 * The tick processor writes directly to FArcCraftOutputFragment and does NOT
 * use this delivery type — it only operates on entity data.
 * This delivery is used by UArcCraftStationComponent when the actor is alive.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Storage"))
struct ARCCRAFT_API FArcCraftOutputDelivery_EntityStore : public FArcCraftOutputDelivery
{
	GENERATED_BODY()

public:
	virtual bool DeliverOutput(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& OutputSpec,
		const UObject* Instigator) const override;

	virtual ~FArcCraftOutputDelivery_EntityStore() override = default;

private:
	UArcCraftVisEntityComponent* GetVisComponent(const UArcCraftStationComponent* Station) const;
	FArcCraftOutputFragment* GetOutputFragment(const UArcCraftStationComponent* Station) const;
	UArcItemsStoreComponent* GetMirrorOutputStore(const UArcCraftStationComponent* Station) const;
};
