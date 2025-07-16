/**
 * This file is part of ArcX.
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
#include "GameplayTagContainer.h"

#include "ArcQuickBarInputBindHandling.generated.h"

class UArcQuickBarComponent;
struct FArcItemData;
struct FArcSlotData;

/*
 * TODO This struct will simply bind/unbdind ability by adding tag to
 * DynamicTags of AbilitySpec. We may also want to add indirection,
 * where if item is moved between slots, the  bindings stay the same,
 * but we instead use slot it to activate anything, that is currently
 * on it.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Quick Bar Input Bind"))
struct ARCCORE_API FArcQuickBarInputBindHandling
{
	GENERATED_BODY()

public:
	/*
	 * Input to which ability in this slot will be bound
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core", meta = (Categories = "InputTag"))
	FGameplayTag TagInput;

	/*
	 * Ability in the slot must have exactly this tag to be bound.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	FGameplayTag AbilityRequiredTag;

	/*
	 * Owner Must Have this tag for Delegates to bind abillity input.
	 * Ie. we want only to have abilities from single bar auto bind,
	 * when editing "equipped" abilities.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	FGameplayTag OwnerRequiredTag;

	/*
	 * If false, will
	 */
	// UPROPERTY(EditDefaultsOnly, Category = "Arc")
	// EArcQuickBarBindPolicy BindPolicy;

	virtual bool ShouldBindInputs(class UArcQuickBarComponent* InQuickBar) const
	{
		return true;
	}
	
	virtual bool OnAddedToQuickBar(class UArcCoreAbilitySystemComponent* InArcASC
								   , TArray<const struct FArcItemData*> InSlots) const;

	virtual bool OnRemovedFromQuickBar(class UArcCoreAbilitySystemComponent* InArcASC
									   , TArray<const struct FArcItemData*> InSlots) const;

	virtual ~FArcQuickBarInputBindHandling() = default;
};