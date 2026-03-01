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
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MassEntityManager.h"
#include "StructUtils/InstancedStruct.h"
#include "AssetRegistry/AssetData.h"
#include "Items/ArcItemSpec.h"
#include "ArcCraft/Station/ArcCraftStationTypes.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"
#include "ArcCraft/Station/ArcCraftOutputDelivery.h"

#include "ArcCraftStationComponent.generated.h"

class UArcRecipeDefinition;
class UArcCraftVisEntityComponent;
struct FMassEntityHandle;

/**
 * Crafting station component that supports:
 * - Explicit recipe selection or automatic recipe discovery from ingredients
 * - Customizable ingredient sourcing (instanced struct)
 * - Customizable output delivery (instanced struct)
 * - Crafting queue with priority
 * - Two time modes: AutoTick (server ticks) and InteractionCheck (UTC stamp)
 *
 * Place on a world actor (forge, alchemy bench, campfire, etc.) to create
 * a crafting station. Players interact to deposit items, select recipes,
 * and collect crafted output.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCCRAFT_API UArcCraftStationComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	// ---- Configuration ----

	/** Tags identifying this station type (e.g. "Station.Forge", "Station.Alchemy").
	 *  Recipes' RequiredStationTags must be a subset of these. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station")
	FGameplayTagContainer StationTags;

	/** How time is tracked for crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Time")
	EArcCraftStationTimeMode TimeMode = EArcCraftStationTimeMode::AutoTick;

	/** Maximum entries in the craft queue. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Queue", meta = (ClampMin = "0"))
	int32 MaxQueueSize = 5;

	/** Tick interval for AutoTick mode (seconds). Lower = more precise, higher = less CPU. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Time", meta = (ClampMin = "0.1"))
	float TickInterval = 0.5f;

	/** How ingredients are sourced.
	 *  Default: none (must be configured).
	 *  Use "Instigator Inventory" to pull from the player's store.
	 *  Use "Station Storage" to require items be deposited first. */
	UPROPERTY(EditAnywhere, Category = "Station|Items",meta = (ExcludeBaseStruct))
	TInstancedStruct<FArcCraftItemSource> ItemSource;

	/** How crafted items are delivered.
	 *  Default: none (must be configured).
	 *  Use "Store On Station" to hold output on the station.
	 *  Use "To Instigator Inventory" to deliver directly to the player. */
	UPROPERTY(EditAnywhere, Category = "Station|Items",meta = (ExcludeBaseStruct))
	TInstancedStruct<FArcCraftOutputDelivery> OutputDelivery;

	// ---- Runtime State (Replicated) ----

	/** The craft queue. */
	UPROPERTY(Replicated)
	FArcCraftStationQueue CraftQueue;

	/** Index of the currently active queue entry (lowest priority). */
	int32 ActiveEntryIndex = INDEX_NONE;

	/** Instigator for the current crafting session. */
	TWeakObjectPtr<UObject> CurrentInstigator;

	// ---- Recipe Cache ----

	/** Cached asset data for recipes matching this station's tags. */
	TArray<FAssetData> CachedStationRecipes;
	bool bRecipesCached = false;

	// ---- Delegates ----
public:
	/** Fired when a queue entry is added or removed. */
	UPROPERTY(BlueprintAssignable, Category = "Craft Station|Events")
	FArcCraftStationQueueChangedDelegate OnQueueChanged;

	/** Fired when a single item finishes crafting. */
	UPROPERTY(BlueprintAssignable, Category = "Craft Station|Events")
	FArcCraftStationItemCompletedDelegate OnItemCompleted;

public:
	UArcCraftStationComponent();

	// ---- Public API ----

	/**
	 * Queue a specific recipe for crafting.
	 * Validates station tags, checks ingredient availability, consumes ingredients, and adds to queue.
	 * @return true if successfully queued.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool QueueRecipe(const UArcRecipeDefinition* Recipe, UObject* Instigator, int32 Amount = 1);

	/**
	 * Auto-discover the best matching recipe from available ingredients and queue it.
	 * Queries the asset registry, filters by station tags and ingredients, loads the best match.
	 * @return The discovered recipe, or nullptr if no match found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	UArcRecipeDefinition* DiscoverAndQueueRecipe(UObject* Instigator);

	/**
	 * Cancel a queued craft entry by its ID.
	 * Note: consumed ingredients are NOT returned (they were consumed at queue time).
	 * @return true if the entry was found and removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool CancelQueueEntry(const FGuid& EntryId);

	/**
	 * Deposit an item into the station's ingredient source.
	 * Only meaningful if the item source supports deposits (e.g. StationStore).
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool DepositItem(const FArcItemSpec& Item, UObject* Instigator);

	/**
	 * Withdraw an input item from the station's ingredient source by index.
	 * @param ItemIndex  Index into the source's available items.
	 * @param Stacks     Number of stacks to withdraw. 0 = all.
	 * @param OutSpec    Filled with the withdrawn item spec on success.
	 * @return true if the item was successfully withdrawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool WithdrawInputItem(int32 ItemIndex, int32 Stacks, FArcItemSpec& OutSpec, UObject* Instigator);

	/**
	 * Withdraw a crafted output item from the station by index.
	 * @param ItemIndex  Index into the output items.
	 * @param Stacks     Number of stacks to withdraw. 0 = all.
	 * @param OutSpec    Filled with the withdrawn item spec on success.
	 * @return true if the item was successfully withdrawn.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool WithdrawOutputItem(int32 ItemIndex, int32 Stacks, FArcItemSpec& OutSpec, UObject* Instigator);

	/**
	 * For InteractionCheck mode: called when the player interacts with the station.
	 * Checks if enough time has passed since the craft started and finishes items accordingly.
	 * @return true if at least one item was completed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool TryCompleteOnInteraction(UObject* Instigator);

	/**
	 * Get all recipes this station can potentially craft (from asset registry).
	 * Loads the recipe assets — use sparingly (e.g. for UI population).
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	TArray<UArcRecipeDefinition*> GetAvailableRecipes();

	/**
	 * Check if a specific recipe's ingredients are available (without consuming).
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	bool CanCraftRecipe(const UArcRecipeDefinition* Recipe, UObject* Instigator) const;

	/**
	 * Get the current craft queue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	const TArray<FArcCraftQueueEntry>& GetQueue() const;

	/**
	 * Get recipes that can be crafted right now with currently available ingredients.
	 * Loads candidate recipes — more expensive than GetAvailableRecipes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Craft Station")
	TArray<UArcRecipeDefinition*> GetCraftableRecipes(UObject* Instigator);

	/**
	 * Get the live craft queue (reads from entity fragment if entity-backed, else actor-side).
	 * Use this for debug UI that needs real-time progress data.
	 */
	TArray<FArcCraftQueueEntry> GetLiveQueue() const;

	/** Whether this station is backed by a Mass entity. */
	bool IsEntityBacked() const { return CachedVisComponent.IsValid(); }

	/** Get the station's tags. */
	const FGameplayTagContainer& GetStationTags() const { return StationTags; }

	/** Replace the actor-side queue from entity data. Called by UArcCraftVisEntityComponent on actor activation. */
	void SetQueueFromEntity(const TArray<FArcCraftQueueEntry>& InEntries);

	/** Get the current actor-side queue entries for syncing back to entity. */
	const FArcCraftStationQueue& GetCraftQueue() const { return CraftQueue; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/**
	 * Build the output FArcItemSpec from a recipe.
	 * Matches ingredients (without consuming), computes quality, applies output modifiers.
	 * Virtual so tests can augment the output spec (e.g. set transient item definitions).
	 */
	virtual FArcItemSpec BuildOutputSpec(
		const UArcRecipeDefinition* Recipe,
		const UObject* Instigator) const;

private:
	/** Ensure the recipe cache is populated from the asset registry. */
	void EnsureRecipesCached();

	/** Find the active (lowest priority) queue entry index. */
	int32 FindActiveEntryIndex() const;

	/** Process the active queue entry in AutoTick mode. */
	void ProcessActiveEntry(float DeltaTime);

	/** Finish one item in a queue entry: build output, deliver, broadcast. */
	void FinishCurrentItem(FArcCraftQueueEntry& Entry);

	// ---- Entity Access Helpers ----

	/**
	 * Get the entity handle from the owning actor's UArcCraftVisEntityComponent.
	 * Returns an invalid handle if the component is missing.
	 */
	FMassEntityHandle GetEntityHandle() const;

	/**
	 * Get the entity manager for the current world.
	 * Returns nullptr if the subsystem is unavailable.
	 */
	FMassEntityManager* GetEntityManager() const;

	/** Cached reference to the vis entity component on our owner. Set in BeginPlay. */
	TWeakObjectPtr<UArcCraftVisEntityComponent> CachedVisComponent;
};
