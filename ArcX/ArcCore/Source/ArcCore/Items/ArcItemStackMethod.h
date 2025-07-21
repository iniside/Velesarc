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

#include "ArcItemStackMethod.generated.h"

UENUM(BlueprintType)
enum class EArcItemStackType : uint8
{
	// If Definitions are the same stack items.
	StackByDefinition = 0

	// Always create new instance of item.
	, NewInstance = 1

	// There can be only single item of this definition.
	, Unique = 2

	/**
	 * Item does not stack, there can be only one instance of item, and can be shared as
	 * socket by multiple items.
	 */ 
	, Shared = 4
	
};


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
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const { return FArcItemId::InvalidId; }

	/** Override to indicate if item can stack at all. */
	virtual bool CanStack() const { return true; };

	/** Override to indicate if item is unique. If true only one instance of this item per Item Component can exists at most. */
	virtual bool IsUnique() const { return false; }

	virtual ~FArcItemStackMethod() = default;
};

USTRUCT()
struct ARCCORE_API FArcItemStackMethod_StackEnum : public FArcItemStackMethod
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere)
	EArcItemStackType StackingType = EArcItemStackType::NewInstance;
	
public:
	/**
	 * @brief 
	 * @param Owner Where item will be added
	 * @param InItem Spec from potential item will be created
	 * @return Valid item Id if item stacked (already exists in items and something changed about it)
	 * or InvalidId if item should be added to items.
	 */
	virtual FArcItemId StackCheck(UArcItemsStoreComponent* Owner, const FArcItemSpec& InSpec) const override;

	virtual bool CanStack() const override;

	virtual bool IsUnique() const override;

	virtual ~FArcItemStackMethod_StackEnum() override = default;
};