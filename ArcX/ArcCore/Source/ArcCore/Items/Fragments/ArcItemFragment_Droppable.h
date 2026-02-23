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
#include "ArcItemFragment_Droppable.generated.h"

class UMassEntityConfigAsset;

/**
 * Marks an item as droppable into the world.
 * Items with this fragment can be converted to Mass entities and spawned on the ground.
 * The DropEntityConfig defines the Mass entity template used for spawning
 * (should include FArcLootContainerFragment trait for holding the item spec).
 */
USTRUCT()
struct ARCCORE_API FArcItemFragment_Droppable : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** Mass entity config used to spawn the dropped item in the world. */
	UPROPERTY(EditAnywhere, meta = (DisplayThumbnail = false))
	TObjectPtr<UMassEntityConfigAsset> DropEntityConfig = nullptr;
};
