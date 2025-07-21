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


#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ArcCustomInputHandler.generated.h"

/**
 *
 */
UCLASS(DefaultToInstanced
	, EditInlineNew
	, Abstract)
class ARCCORE_API UArcCustomInputHandler : public UObject
{
	GENERATED_BODY()

public:
	virtual void InputPressed(APawn* InPawn
							  , FGameplayTag InputTag)
	{
	};

	virtual void InputTriggered(APawn* InPawn
								, FGameplayTag InputTag)
	{
	};

	virtual void InputReleased(APawn* InPawn
							   , FGameplayTag InputTag)
	{
	};
};
