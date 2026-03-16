/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/ArcItemId.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ArcItemAbilityHelpers.generated.h"

struct FArcGameplayEffectContext;
struct FArcItemData;
class UArcItemDefinition;
class UArcItemsStoreComponent;
class UAbilitySystemComponent;

UCLASS()
class ARCCORE_API UArcItemAbilityHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Resolve item data from store + ID. Returns nullptr if item not found. */
	static const FArcItemData* GetItemData(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId);

	/** Resolve item definition from store + ID. Returns nullptr if item not found. */
	UFUNCTION(BlueprintPure, Category = "ArcCore|Item Ability")
	static const UArcItemDefinition* GetItemDefinition(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId);

	/** Remove stacks from item. Returns false if item lacks stacks fragment or has insufficient stacks. */
	UFUNCTION(BlueprintCallable, Category = "ArcCore|Item Ability")
	static bool ConsumeStacks(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, int32 Count);

	/** Get current stack count. Returns 0 if item not found or has no stacks fragment. */
	UFUNCTION(BlueprintPure, Category = "ArcCore|Item Ability")
	static int32 GetStackCount(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId);

	/** Check if item has at least Required stacks. */
	UFUNCTION(BlueprintPure, Category = "ArcCore|Item Ability")
	static bool HasEnoughStacks(UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, int32 Required);

	/**
	 * Create a FGameplayEffectContextHandle populated with item data.
	 * Sets SourceItemId, ItemsStoreComponent, SourceItemPtr, and SourceItem (definition)
	 * on the FArcGameplayEffectContext.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCore|Item Ability")
	static FGameplayEffectContextHandle MakeItemEffectContext(UAbilitySystemComponent* ASC, UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId);

	/**
	 * Extract FArcGameplayEffectContext from a context handle.
	 * Returns nullptr if the handle is invalid or not an FArcGameplayEffectContext.
	 */
	static FArcGameplayEffectContext* GetItemDataFromContext(FGameplayEffectContextHandle ContextHandle);

	/**
	 * Build item effect context, wrap in FGameplayEventData, and send via SendGameplayEventToActor.
	 * Returns the number of abilities that handled the event (0 = no ability activated).
	 */
	UFUNCTION(BlueprintCallable, Category = "ArcCore|Item Ability")
	static int32 SendItemUseEvent(UAbilitySystemComponent* ASC, UArcItemsStoreComponent* ItemsStore, FArcItemId ItemId, FGameplayTag EventTag);
};
