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

#include "ArcFixedDeltaInputModifier.h"

FInputActionValue UArcFixedDeltaInputModifier::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue
	, float DeltaTime)
{
	if (CurrentValue.GetValueType() == EInputActionValueType::Boolean)
	{
		return CurrentValue; // Only process FVector2D values
	}
	
	const float BaselineDelta = 1.f / DesiredFramerate;
	
	if (DeltaTime > 0.0f && DeltaTime != BaselineDelta)
	{
		float Scale = BaselineDelta / DeltaTime;
            
		// Clamp the scale to prevent extreme values
		Scale = FMath::Clamp(Scale, 0.1f, 10.0f);
            
		FVector2D Input = CurrentValue.Get<FVector2D>();
		return FInputActionValue(Input * Scale);
	}
        
	return CurrentValue;
}
