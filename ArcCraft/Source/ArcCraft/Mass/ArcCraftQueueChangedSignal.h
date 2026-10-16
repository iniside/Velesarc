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

namespace ArcCraftSignals
{
	// Why: entity signal posted by UArcCraftTickProcessor (and by FArcCraftFromStationInputCommand) whenever
	// the craft queue on a station entity is mutated (entry added, activated, or completed).
	// UArcCraftingViewModel subscribes to this signal to drive queue-panel refresh.
	inline const FName QueueChanged = FName(TEXT("ArcCraft.QueueChanged"));

	// Why: entity signal posted by FArcCraftFromStationInputCommand (deposit), FArcCraftItemSource_EntityStore
	// (withdraw) and UArcCraftTickProcessor (consume) whenever FArcCraftInputFragment::InputItems is mutated. The crafting UI
	// subscribes to drive combined availability and deposit-origin refresh.
	inline const FName InputChanged = FName(TEXT("ArcCraft.InputChanged"));

	// Why: entity signal posted whenever FArcCraftOutputFragment::OutputItems is mutated — the
	// base delivery (FArcCraftOutputDelivery::DeliverOutput_Mass) and the legacy tick write
	// (UArcCraftTickProcessor). The crafting UI subscribes to drive station-output refresh.
	inline const FName OutputChanged = FName(TEXT("ArcCraft.OutputChanged"));
}
