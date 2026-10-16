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

#include "ArcMassInputMappingContextAndPriority.generated.h"

class UInputMappingContext;

/**
 * Soft reference to an InputMappingContext along with a priority and a flag
 * controlling whether the mapping should be exposed in the user-facing
 * Enhanced Input settings (rebindable keys). Mirror of ArcCore's
 * FInputMappingContextAndPriority but kept independent so ArcMassGameplayFramework
 * does not depend on ArcCore.
 */
USTRUCT(BlueprintType)
struct FArcMassInputMappingContextAndPriority
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	/** Higher priority mappings are evaluated before lower priority mappings. */
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;

	/** When true, the IMC is registered with UEnhancedInputUserSettings (rebindable). */
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bRegisterWithSettings = true;
};
