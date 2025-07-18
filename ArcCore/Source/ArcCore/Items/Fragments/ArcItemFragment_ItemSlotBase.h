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


#include "ArcItemFragment.h"
#include "UObject/Object.h"
#include "ArcItemFragment_ItemSlotBase.generated.h"

struct FArcSlotData;

USTRUCT(meta = (Hidden, LoadBehavior = "LazyOnDemand"))
struct ARCCORE_API FArcItemFragment_ItemSlotBase : public FArcItemFragment
{
	GENERATED_BODY()

public:
	virtual ~FArcItemFragment_ItemSlotBase() override = default;

	virtual void OnAddedToSlot(const FArcSlotData* InSlot, const FArcItemData* Item) const {};
	virtual void OnSlotChanged(const FArcSlotData* InSlot, const FArcItemData* Item) const {};
	virtual void OnRemovedFromSlot(const FArcSlotData* InSlot, const FArcItemData* Item) const {};
	
	virtual UScriptStruct* GetSlotInstanceType() const
	{
		return nullptr;
	}
};