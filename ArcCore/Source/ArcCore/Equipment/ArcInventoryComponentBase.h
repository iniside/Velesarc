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

#include "CoreMinimal.h"
#include "Components/GameFrameworkComponent.h"

#include "ArcInventoryComponentBase.generated.h"

/*
 * Base class for Inventory Component. Im not sure if anything will be added here.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcInventoryComponentBase : public UGameFrameworkComponent
{
	GENERATED_BODY()
};

/**
 * Grid based inventory with limited amount of slots for items.
 */
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcGridInventoryComponent : public UArcInventoryComponentBase
{
	GENERATED_BODY()
};
