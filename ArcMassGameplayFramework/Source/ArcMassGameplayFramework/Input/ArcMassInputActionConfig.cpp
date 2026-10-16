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

#include "Input/ArcMassInputActionConfig.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY(LogArcMassInputConfig);

UArcMassInputActionConfig::UArcMassInputActionConfig(const FObjectInitializer& ObjectInitializer)
{
}

const UInputAction* UArcMassInputActionConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag
																		   , bool bLogNotFound) const
{
	for (const FArcMassInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOGF(LogArcMassInputConfig
			, Error
			,"Can't find NativeInputAction for InputTag [%ls] on InputConfig [%ls].", *InputTag.ToString()
			, *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* UArcMassInputActionConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag
																			, bool bLogNotFound) const
{
	for (const FArcMassInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOGF(LogArcMassInputConfig
			, Error
			,"Can't find AbilityInputAction for InputTag [%ls] on InputConfig [%ls].", *InputTag.ToString()
			, *GetNameSafe(this));
	}

	return nullptr;
}
