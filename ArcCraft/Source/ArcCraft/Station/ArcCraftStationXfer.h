/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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
#include "Mass/EntityHandle.h"

struct FMassEntityManager;
struct FArcItemSpec;

/**
 * Shared station→instigator delivery helper. Routes a spec back to a player entity in a
 * bag-aware way so every "add items back to the player" station path (input withdraw and
 * output withdraw) shares one atomic primitive.
 */
namespace ArcCraftStationXfer
{
	/**
	 * Deliver Spec to InstigatorEntity, returning the number of stacks actually stored so the
	 * caller can decrement its source by that count only (never more than landed → no item loss).
	 *
	 * - Instigator has a bag overlay → spills across bag slots via Bags::AddItemSpilling and
	 *   returns the placed count (may be partial; remainder is left for the caller's source).
	 * - No bag overlay → degrades to a plain store AddItem of the whole spec. AddItem is
	 *   all-or-nothing: a valid returned id means the whole spec landed (return Spec.Amount);
	 *   InvalidId means AddItem rejected it and nothing was stored (return 0).
	 *
	 * Must run on the game thread (assumes no open transaction; all bag ops are synchronous).
	 */
	int32 DeliverSpecToInstigator(FMassEntityManager& Mgr, FMassEntityHandle Instigator, const FArcItemSpec& Spec);
}
