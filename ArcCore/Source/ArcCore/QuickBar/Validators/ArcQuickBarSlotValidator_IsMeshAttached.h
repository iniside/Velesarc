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


#include "ArcQuickBarSlotValidator.h"

#include "ArcQuickBarSlotValidator_IsMeshAttached.generated.h"
/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcQuickBarSlotValidator_IsMeshAttached : public FArcQuickBarSlotValidator
{
	GENERATED_BODY()

public:
	virtual bool IsValid(UArcQuickBarComponent* InQuickBarComp
						 , const struct FArcQuickBar& InQuickBar
						 , const struct FArcQuickBarSlot& InSlot) const override;

	virtual ~FArcQuickBarSlotValidator_IsMeshAttached() override
	{
	}
};