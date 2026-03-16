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

#include "ChartCreation.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "LyraPerformanceStatSubsystem.generated.h"

enum class ELyraDisplayablePerformanceStat : uint8;

class FSubsystemCollectionBase;
class ULyraPerformanceStatSubsystem;
class UObject;
struct FFrame;

//////////////////////////////////////////////////////////////////////

// Observer which caches the stats for the previous frame
struct FLyraPerformanceStatCache : public IPerformanceDataConsumer
{
public:
	FLyraPerformanceStatCache(ULyraPerformanceStatSubsystem* InSubsystem)
		: MySubsystem(InSubsystem)
	{
	}

	//~IPerformanceDataConsumer interface
	virtual void StartCharting() override;
	virtual void ProcessFrame(const FFrameData& FrameData) override;
	virtual void StopCharting() override;
	//~End of IPerformanceDataConsumer interface

	double GetCachedStat(ELyraDisplayablePerformanceStat Stat) const;

protected:
	IPerformanceDataConsumer::FFrameData CachedData;
	ULyraPerformanceStatSubsystem* MySubsystem;

	float CachedServerFPS = 0.0f;
	float CachedPingMS = 0.0f;
	float CachedPacketLossIncomingPercent = 0.0f;
	float CachedPacketLossOutgoingPercent = 0.0f;
	float CachedPacketRateIncoming = 0.0f;
	float CachedPacketRateOutgoing = 0.0f;
	float CachedPacketSizeIncoming = 0.0f;
	float CachedPacketSizeOutgoing = 0.0f;
};

//////////////////////////////////////////////////////////////////////

// Subsystem to allow access to performance stats for display purposes
UCLASS(BlueprintType)
class ULyraPerformanceStatSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	double GetCachedStat(ELyraDisplayablePerformanceStat Stat) const;

	//~USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~End of USubsystem interface

protected:
	TSharedPtr<FLyraPerformanceStatCache> Tracker;
};
