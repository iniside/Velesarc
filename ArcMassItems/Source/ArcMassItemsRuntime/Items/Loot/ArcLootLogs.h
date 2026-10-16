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

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcLoot, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogArcLootDrop, Log, All);

// Engine VisualLogger gates on UE_DEBUG_RECORDING_ENABLED in ue5-main
// (Engine/Source/Runtime/Engine/Public/VisualLogger/VisualLogger.h:65).
// ENABLE_VISUAL_LOG is a legacy 5.4/5.5 guard and is NOT defined here.
#if UE_DEBUG_RECORDING_ENABLED
#include "VisualLogger/VisualLogger.h"
#include "MassEntitySubsystem.h"
#include "Engine/World.h"

// Parameter order matches engine UE_VLOG / UE_VLOG_LOCATION
// (LogOwner, CategoryName, Verbosity, [Location, Thickness, Color,] Format, ...)
#define ARCLOOT_VLOG(World, Verbosity, Format, ...) \
    UE_VLOG(UWorld::GetSubsystem<UMassEntitySubsystem>(World), LogArcLoot, Verbosity, Format, ##__VA_ARGS__)
#define ARCLOOT_VLOG_LOCATION(World, Verbosity, Location, Thickness, Color, Format, ...) \
    UE_VLOG_LOCATION(UWorld::GetSubsystem<UMassEntitySubsystem>(World), LogArcLoot, Verbosity, Location, Thickness, Color, Format, ##__VA_ARGS__)
#else
#define ARCLOOT_VLOG(...)          do {} while(0)
#define ARCLOOT_VLOG_LOCATION(...) do {} while(0)
#endif
