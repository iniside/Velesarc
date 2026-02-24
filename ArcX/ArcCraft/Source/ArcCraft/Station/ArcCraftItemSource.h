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

#include "ArcCraftItemSource.generated.h"

class UArcCraftStationComponent;
class UArcRecipeDefinition;
class UArcItemsStoreComponent;
class UArcCraftVisEntityComponent;
struct FArcItemData;
struct FArcCraftInputFragment;

/**
 * Base instanced struct for ingredient sourcing.
 * Defines how a crafting station obtains items for recipes.
 *
 * Override this to create custom item sources (e.g. searching nearby containers,
 * pulling from a shared storage network, etc.).
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftItemSource
{
	GENERATED_BODY()

public:
	/**
	 * Get all items available from this source.
	 * Used for recipe discovery and UI display.
	 */
	virtual TArray<const FArcItemData*> GetAvailableItems(
		const UArcCraftStationComponent* Station,
		const UObject* Instigator) const;

	/**
	 * Check if a recipe's ingredients can be satisfied without consuming anything.
	 */
	virtual bool CanSatisfyRecipe(
		const UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const;

	/**
	 * Consume ingredients for a recipe.
	 * @param OutMatchedItems   Filled with matched item data per ingredient slot.
	 * @param OutQualityMults   Filled with quality multiplier per ingredient slot.
	 * @return true if all ingredients were consumed successfully.
	 */
	virtual bool ConsumeIngredients(
		UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator,
		TArray<const FArcItemData*>& OutMatchedItems,
		TArray<float>& OutQualityMults) const;

	/**
	 * Deposit an item into this source's storage.
	 * Used when players add ingredients to a station.
	 */
	virtual bool DepositItem(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& Item,
		const UObject* Instigator);

	virtual ~FArcCraftItemSource() = default;

protected:
	/** Shared logic: match and optionally consume recipe ingredients from an items store. */
	static bool MatchAndConsumeFromStore(
		UArcItemsStoreComponent* ItemsStore,
		const UArcRecipeDefinition* Recipe,
		bool bConsume,
		TArray<const FArcItemData*>* OutMatchedItems = nullptr,
		TArray<float>* OutQualityMults = nullptr);
};

/**
 * Item source that uses a UArcItemsStoreComponent on the instigator (player).
 * This is the simplest source — ingredients come from the player's inventory.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Instigator Inventory"))
struct ARCCRAFT_API FArcCraftItemSource_InstigatorStore : public FArcCraftItemSource
{
	GENERATED_BODY()

public:
	/** Which items store class to look for on the instigator actor. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	virtual TArray<const FArcItemData*> GetAvailableItems(
		const UArcCraftStationComponent* Station,
		const UObject* Instigator) const override;

	virtual bool CanSatisfyRecipe(
		const UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const override;

	virtual bool ConsumeIngredients(
		UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator,
		TArray<const FArcItemData*>& OutMatchedItems,
		TArray<float>& OutQualityMults) const override;

	virtual bool DepositItem(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& Item,
		const UObject* Instigator) override;

	virtual ~FArcCraftItemSource_InstigatorStore() override = default;

protected:
	UArcItemsStoreComponent* GetItemsStore(const UObject* Instigator) const;
};

/**
 * Item source that uses a UArcItemsStoreComponent on the station's owning actor.
 * Items must be deposited into the station first.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Station Storage"))
struct ARCCRAFT_API FArcCraftItemSource_StationStore : public FArcCraftItemSource
{
	GENERATED_BODY()

public:
	/** Which items store class to look for on the station's owner actor. */
	UPROPERTY(EditAnywhere, Category = "Source")
	TSubclassOf<UArcItemsStoreComponent> ItemsStoreClass;

	virtual TArray<const FArcItemData*> GetAvailableItems(
		const UArcCraftStationComponent* Station,
		const UObject* Instigator) const override;

	virtual bool CanSatisfyRecipe(
		const UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const override;

	virtual bool ConsumeIngredients(
		UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator,
		TArray<const FArcItemData*>& OutMatchedItems,
		TArray<float>& OutQualityMults) const override;

	virtual bool DepositItem(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& Item,
		const UObject* Instigator) override;

	virtual ~FArcCraftItemSource_StationStore() override = default;

protected:
	UArcItemsStoreComponent* GetStationStore(const UArcCraftStationComponent* Station) const;
};

/**
 * Item source that reads from the entity's FArcCraftInputFragment.
 * Items must be deposited via the actor interface; depositing writes directly
 * to the entity fragment through UArcCraftVisEntityComponent.
 *
 * When the actor is alive (vis active), items are also available from the
 * UArcItemsStoreComponent that mirrors the entity data. When the actor is
 * despawned, only the entity fragment persists — the processor reads it directly.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Entity Storage"))
struct ARCCRAFT_API FArcCraftItemSource_EntityStore : public FArcCraftItemSource
{
	GENERATED_BODY()

public:
	virtual TArray<const FArcItemData*> GetAvailableItems(
		const UArcCraftStationComponent* Station,
		const UObject* Instigator) const override;

	virtual bool CanSatisfyRecipe(
		const UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const override;

	virtual bool ConsumeIngredients(
		UArcCraftStationComponent* Station,
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator,
		TArray<const FArcItemData*>& OutMatchedItems,
		TArray<float>& OutQualityMults) const override;

	virtual bool DepositItem(
		UArcCraftStationComponent* Station,
		const FArcItemSpec& Item,
		const UObject* Instigator) override;

	virtual ~FArcCraftItemSource_EntityStore() override = default;

private:
	/**
	 * Get the UArcCraftVisEntityComponent from the station's owner.
	 * Returns nullptr if the owner doesn't have one.
	 */
	UArcCraftVisEntityComponent* GetVisComponent(const UArcCraftStationComponent* Station) const;

	/**
	 * Get the input store component that mirrors entity data.
	 * This is the store class configured on the vis component.
	 */
	UArcItemsStoreComponent* GetInputStore(const UArcCraftStationComponent* Station) const;
};
