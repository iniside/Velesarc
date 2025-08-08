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
#include "ArcItemId.h"
#include "ScalableFloat.h"

#include "ArcItemStackMethod.generated.h"

struct FArcItemSpec;
class UArcItemsStoreComponent;

/**
 * Override this struct, to define custom rules for how item is stacking.
 */
USTRUCT()
struct ARCCORE_API FArcItemStackMethod
{
	GENERATED_BODY()

public:
	/**
	 * @brief 
	 * @param Owner Where item will be added
	 * @param InItem Spec from potential item will be created
	 * @return Valid item Id if item stacked (already exists in items and something changed about it)
	 * or InvalidId if item should be added to items.
	 */
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const { return FArcItemId::InvalidId; }

	/** Override to indicate if item can stack at all. */
	virtual bool CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const { return true; };

	/** * @brief 
	 * @param Owner Where item will be added
	 * @param InSpec Spec from potential item will be created
	 * @return True if item can be added to items, false otherwise.
	 */
	virtual bool CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const { return true; }

	virtual ~FArcItemStackMethod() = default;
};

USTRUCT()
struct ARCCORE_API FArcItemStackMethod_CanNotStack : public FArcItemStackMethod
{
	GENERATED_BODY()
	
public:
	/**
	 * @brief 
	 * @param Owner Where item will be added
	 * @param InItem Spec from potential item will be created
	 * @return Valid item Id if item stacked (already exists in items and something changed about it)
	 * or InvalidId if item should be added to items.
	 */
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const override;

	virtual bool CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual bool CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual ~FArcItemStackMethod_CanNotStack() override = default;
};

// Only one Item of this ItemDefinition can be ever instanced in inventory.
USTRUCT()
struct ARCCORE_API FArcItemStackMethod_CanNotStackUnique : public FArcItemStackMethod
{
	GENERATED_BODY()
	
public:
	/**
	 * @brief 
	 * @param Owner Where item will be added
	 * @param InItem Spec from potential item will be created
	 * @return Valid item Id if item stacked (already exists in items and something changed about it)
	 * or InvalidId if item should be added to items.
	 */
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const override;

	virtual bool CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual bool CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual ~FArcItemStackMethod_CanNotStackUnique() override = default;
};

// Items with the same item definition will be stacked together up to a maximum Stack size.
USTRUCT()
struct ARCCORE_API FArcItemStackMethod_StackByType : public FArcItemStackMethod
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FScalableFloat MaxStacks;
	
	/**
	 * @brief 
	 * @param Owner Where item will be added
	 * @param InItem Spec from potential item will be created
	 * @return Valid item Id if item stacked (already exists in items and something changed about it)
	 * or InvalidId if item should be added to items.
	 */
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec, uint16& OutNewStacks, uint16& OutRemainingStacks) const override;

	virtual bool CanStack(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual bool CanAdd(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual ~FArcItemStackMethod_StackByType() override = default;
};