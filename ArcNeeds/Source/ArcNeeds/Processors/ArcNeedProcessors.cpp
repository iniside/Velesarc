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
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNeedProcessors)

// ---------------------------------------------------------------------------
// UArcHungerNeedProcessor
// ---------------------------------------------------------------------------

UArcHungerNeedProcessor::UArcHungerNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UArcHungerNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcHungerNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcHungerNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcHungerNeed);

	NeedsQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		const TArrayView<FArcHungerNeedFragment> Needs = Context.GetMutableFragmentView<FArcHungerNeedFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FArcHungerNeedFragment& Need = Needs[EntityIndex];
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue + (Need.ChangeRate * DeltaTime), 0.f, 100.f);
		}
	});
}

// ---------------------------------------------------------------------------
// UArcThirstNeedProcessor
// ---------------------------------------------------------------------------

UArcThirstNeedProcessor::UArcThirstNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UArcThirstNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcThirstNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcThirstNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcThirstNeed);

	NeedsQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		const TArrayView<FArcThirstNeedFragment> Needs = Context.GetMutableFragmentView<FArcThirstNeedFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FArcThirstNeedFragment& Need = Needs[EntityIndex];
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue + (Need.ChangeRate * DeltaTime), 0.f, 100.f);
		}
	});
}

// ---------------------------------------------------------------------------
// UArcFatigueNeedProcessor
// ---------------------------------------------------------------------------

UArcFatigueNeedProcessor::UArcFatigueNeedProcessor()
	: NeedsQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcFatigueNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	NeedsQuery.Initialize(EntityManager);
	NeedsQuery.AddRequirement<FArcFatigueNeedFragment>(EMassFragmentAccess::ReadWrite);
	NeedsQuery.RegisterWithProcessor(*this);
}

void UArcFatigueNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcFatigueNeed);

	NeedsQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		const TArrayView<FArcFatigueNeedFragment> Needs = Context.GetMutableFragmentView<FArcFatigueNeedFragment>();

		for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); ++EntityIndex)
		{
			FArcFatigueNeedFragment& Need = Needs[EntityIndex];
			Need.CurrentValue = FMath::Clamp(Need.CurrentValue + (Need.ChangeRate * DeltaTime), 0.f, 100.f);
		}
	});
}
