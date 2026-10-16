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

#include "Processors/ArcNeedProcessors.h"
#include "ArcNeedSignals.h"
#include "Fragments/ArcNeedBands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNeedProcessors)

namespace ArcNeedProcessorsPrivate
{

/** The band table carries the settlement's authored SecondsPerGameHour; a body outside a settlement has none and runs at the default. */
template <typename TNeedFragment>
void AdvanceChunk(FMassExecutionContext& Context, double WorldTime)
{
	const TArrayView<TNeedFragment> Needs = Context.GetMutableFragmentView<TNeedFragment>();
	const FArcNeedBandTableFragment* BandTable = Context.GetConstSharedFragmentPtr<FArcNeedBandTableFragment>();
	const float SecondsPerGameHour = BandTable ? BandTable->SecondsPerGameHour : ArcNeeds::DefaultSecondsPerGameHour;

	for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
	{
		UE_CLOGF(BandTable == nullptr && Needs[EntityIndex].LastUpdateTime <= 0.0, LogTemp, Warning,"ArcNeeds: entity %ls has no FArcNeedBandTableFragment; its need decays at the %.0f s/game-hour default.",
			*Context.GetEntity(EntityIndex).DebugGetDescription(), ArcNeeds::DefaultSecondsPerGameHour);
		ArcNeeds::AdvanceNeed(Needs[EntityIndex], WorldTime, SecondsPerGameHour, false);
	}
}

} // namespace ArcNeedProcessorsPrivate

// ---------------------------------------------------------------------------
// UArcHungerNeedProcessor
// ---------------------------------------------------------------------------

UArcHungerNeedProcessor::UArcHungerNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcHungerNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcHungerNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.AddConstSharedRequirement<FArcNeedBandTableFragment>(EMassFragmentPresence::Optional);
	NeedsQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcHungerNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcHungerNeed);

	const double WorldTime = Context.GetWorld()->GetTimeSeconds();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	NeedsQuery.ForEachEntityChunk(Context, [WorldTime, &SignalSubsystem](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FArcHungerNeedFragment> Needs = ChunkContext.GetMutableFragmentView<FArcHungerNeedFragment>();
		const FArcNeedBandTableFragment* BandTable = ChunkContext.GetConstSharedFragmentPtr<FArcNeedBandTableFragment>();
		const float SecondsPerGameHour = BandTable ? BandTable->SecondsPerGameHour : ArcNeeds::DefaultSecondsPerGameHour;
		const TArray<FArcNeedBand>& HungerBands = ArcNeeds::HungerBandsFor(BandTable);

		for (int32 EntityIndex = 0; EntityIndex < ChunkContext.GetNumEntities(); ++EntityIndex)
		{
			FArcHungerNeedFragment& Need = Needs[EntityIndex];
			UE_CLOGF(BandTable == nullptr && Need.LastUpdateTime <= 0.0, LogTemp, Warning,"ArcNeeds: entity %ls has no FArcNeedBandTableFragment; its need decays at the %.0f s/game-hour default.",
				*ChunkContext.GetEntity(EntityIndex).DebugGetDescription(), ArcNeeds::DefaultSecondsPerGameHour);

			const bool bJustInitialized = Need.LastUpdateTime <= 0.0;
			ArcNeeds::AdvanceNeed(Need, WorldTime, SecondsPerGameHour, false);

			const int32 NewBand = ArcNeeds::BandIndexFor(HungerBands, Need.CurrentValue);
			if (bJustInitialized)
			{
				Need.LastBand = static_cast<uint8>(NewBand);
				continue;
			}

			const int32 OldBand = static_cast<int32>(Need.LastBand);
			if (NewBand != OldBand)
			{
				Need.LastBand = static_cast<uint8>(NewBand);
				if (NewBand > OldBand)
				{
					SignalSubsystem.SignalEntityDeferred(
						ChunkContext, ArcNeeds::Signals::UtilityEvaluate, ChunkContext.GetEntity(EntityIndex));
				}
			}
		}
	});
}

// ---------------------------------------------------------------------------
// UArcThirstNeedProcessor
// ---------------------------------------------------------------------------

UArcThirstNeedProcessor::UArcThirstNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

void UArcThirstNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcThirstNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.AddConstSharedRequirement<FArcNeedBandTableFragment>(EMassFragmentPresence::Optional);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcThirstNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcThirstNeed);

	const double WorldTime = Context.GetWorld()->GetTimeSeconds();

	NeedsQuery.ForEachEntityChunk(Context, [WorldTime](FMassExecutionContext& Context)
	{
		ArcNeedProcessorsPrivate::AdvanceChunk<FArcThirstNeedFragment>(Context, WorldTime);
	});
}

// ---------------------------------------------------------------------------
// UArcFatigueNeedProcessor
// ---------------------------------------------------------------------------

UArcFatigueNeedProcessor::UArcFatigueNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcFatigueNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ProcessorRequirements.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);

	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcFatigueNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.AddConstSharedRequirement<FArcNeedBandTableFragment>(EMassFragmentPresence::Optional);
	NeedsQuery.AddSubsystemRequirement<UMassSignalSubsystem>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcFatigueNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcFatigueNeed);

	const double WorldTime = Context.GetWorld()->GetTimeSeconds();
	UMassSignalSubsystem& SignalSubsystem = Context.GetMutableSubsystemChecked<UMassSignalSubsystem>();

	NeedsQuery.ForEachEntityChunk(Context, [WorldTime, &SignalSubsystem](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FArcFatigueNeedFragment> Needs = ChunkContext.GetMutableFragmentView<FArcFatigueNeedFragment>();
		const FArcNeedBandTableFragment* BandTable = ChunkContext.GetConstSharedFragmentPtr<FArcNeedBandTableFragment>();
		const float SecondsPerGameHour = BandTable ? BandTable->SecondsPerGameHour : ArcNeeds::DefaultSecondsPerGameHour;
		const TArray<FArcNeedBand>& FatigueBands = ArcNeeds::FatigueBandsFor(BandTable);

		for (int32 EntityIndex = 0; EntityIndex < ChunkContext.GetNumEntities(); ++EntityIndex)
		{
			FArcFatigueNeedFragment& Need = Needs[EntityIndex];
			UE_CLOGF(BandTable == nullptr && Need.LastUpdateTime <= 0.0, LogTemp, Warning,"ArcNeeds: entity %ls has no FArcNeedBandTableFragment; its need decays at the %.0f s/game-hour default.",
				*ChunkContext.GetEntity(EntityIndex).DebugGetDescription(), ArcNeeds::DefaultSecondsPerGameHour);

			const bool bJustInitialized = Need.LastUpdateTime <= 0.0;
			ArcNeeds::AdvanceNeed(Need, WorldTime, SecondsPerGameHour, false);

			const int32 NewBand = ArcNeeds::BandIndexFor(FatigueBands, Need.CurrentValue);
			if (bJustInitialized)
			{
				Need.LastBand = static_cast<uint8>(NewBand);
				continue;
			}

			const int32 OldBand = static_cast<int32>(Need.LastBand);
			if (NewBand != OldBand)
			{
				Need.LastBand = static_cast<uint8>(NewBand);
				if (NewBand > OldBand)
				{
					SignalSubsystem.SignalEntityDeferred(
						ChunkContext, ArcNeeds::Signals::UtilityEvaluate, ChunkContext.GetEntity(EntityIndex));
				}
			}
		}
	});
}
