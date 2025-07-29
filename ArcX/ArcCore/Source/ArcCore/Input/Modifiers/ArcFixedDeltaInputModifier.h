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

#include "InputModifiers.h"

#include "ArcFixedDeltaInputModifier.generated.h"

/**
 *  Input modifier, which will try to modify any axis into fixed delta time. This will make input more consistent across different framerates.
 *
 *  Default to 60.
 *
 *  This modifier should be added first, and then the user should add different modifiers, which will modify the normalized value.
 *  Do not add any other scale by delta time modifiers, as they will make the problem of input being dependent on frame rate more pronouced. 
 */
UCLASS(NotBlueprintable, MinimalAPI)
class UArcFixedDeltaInputModifier : public UInputModifier
{
	GENERATED_BODY()

public:

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config, meta=(ClampMin=30, ClampMax=120))
	float DesiredFramerate = 60.f;

protected:

	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};