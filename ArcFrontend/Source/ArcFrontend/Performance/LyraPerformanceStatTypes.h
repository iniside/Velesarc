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


#include "Misc/EnumRange.h"

#include "LyraPerformanceStatTypes.generated.h"

//////////////////////////////////////////////////////////////////////

// Way to display the stat
UENUM(BlueprintType)
enum class ELyraStatDisplayMode : uint8
{
	// Don't show this stat
	Hidden,

	// Show this stat in text form
	TextOnly,

	// Show this stat in graph form
	GraphOnly,

	// Show this stat as both text and graph
	TextAndGraph
};

//////////////////////////////////////////////////////////////////////

// Different kinds of stats that can be displayed on-screen
UENUM(BlueprintType)
enum class ELyraDisplayablePerformanceStat : uint8
{
	// stat fps (in Hz)
	ClientFPS,

	// server tick rate (in Hz)
	ServerFPS,
	
	// idle time spent waiting for vsync or frame rate limit (in seconds)
	IdleTime,

	// Stat unit overall (in seconds)
	FrameTime,

	// Stat unit (game thread, in seconds)
	FrameTime_GameThread,

	// Stat unit (render thread, in seconds)
	FrameTime_RenderThread,

	// Stat unit (RHI thread, in seconds)
	FrameTime_RHIThread,

	// Stat unit (inferred GPU time, in seconds)
	FrameTime_GPU,

	// Network ping (in ms)
	Ping,

	// The incoming packet loss percentage (%)
	PacketLoss_Incoming,

	// The outgoing packet loss percentage (%)
	PacketLoss_Outgoing,

	// The number of packets received in the last second
	PacketRate_Incoming,

	// The number of packets sent in the past second
	PacketRate_Outgoing,

	// The avg. size (in bytes) of packets received
	PacketSize_Incoming,

	// The avg. size (in bytes) of packets sent
	PacketSize_Outgoing,

	// New stats should go above here
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ELyraDisplayablePerformanceStat, ELyraDisplayablePerformanceStat::Count);

//////////////////////////////////////////////////////////////////////
