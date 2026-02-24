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

#include "ArcCraft/Mass/ArcCraftStationTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcCraftStationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcCraftQueueFragment>();
	BuildContext.AddFragment<FArcCraftInputFragment>();
	BuildContext.AddFragment<FArcCraftOutputFragment>();
	BuildContext.AddTag<FArcCraftStationTag>();

	// Add AutoTick tag so the tick processor picks up this entity.
	// InteractionCheck entities don't get this tag — they're never iterated by the processor.
	if (StationConfig.TimeMode == EArcCraftStationTimeMode::AutoTick)
	{
		BuildContext.AddTag<FArcCraftAutoTickTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(StationConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
