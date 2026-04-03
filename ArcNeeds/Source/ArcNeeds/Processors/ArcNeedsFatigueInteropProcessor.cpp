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

#include "ArcNeedsFatigueInteropProcessor.h"

#include "MassExecutionContext.h"
#include "Attributes/ArcNeedsSurvivalAttributeSet.h"
#include "Fragments/ArcNeedFragment.h"
#include "Player/ArcHeroComponentBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNeedsFatigueInteropProcessor)

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UArcNeedsFatigueInteropProcessor::UArcNeedsFatigueInteropProcessor()
	: FatigueQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

// ---------------------------------------------------------------------------
// ConfigureQueries
// ---------------------------------------------------------------------------

void UArcNeedsFatigueInteropProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	FatigueQuery.Initialize(EntityManager);
	FatigueQuery.AddRequirement<FArcFatigueNeedFragment>(EMassFragmentAccess::ReadOnly);
	FatigueQuery.AddRequirement<FArcCoreAbilitySystemFragment>(EMassFragmentAccess::ReadWrite);
	FatigueQuery.RegisterWithProcessor(*this);
}

// ---------------------------------------------------------------------------
// Execute -- sync fatigue fragment value into GAS attribute once per second
// ---------------------------------------------------------------------------

void UArcNeedsFatigueInteropProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const float DeltaSeconds = Context.GetDeltaTimeSeconds();
	AccumulatedTime += DeltaSeconds;

	if (AccumulatedTime < 1.f)
	{
		return;
	}
	AccumulatedTime = 0.f;

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNeedsFatigueInterop);

	FatigueQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcFatigueNeedFragment> FatigueFragments = Ctx.GetFragmentView<FArcFatigueNeedFragment>();
			TArrayView<FArcCoreAbilitySystemFragment> ASCFragments = Ctx.GetMutableFragmentView<FArcCoreAbilitySystemFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcFatigueNeedFragment& FatigueFragment = FatigueFragments[EntityIt];
				const FArcCoreAbilitySystemFragment& ASCFrag = ASCFragments[EntityIt];

				if (!ASCFrag.AbilitySystem.IsValid())
				{
					continue;
				}

				// Sync fragment -> GAS attribute
				ASCFrag.AbilitySystem->SetNumericAttributeBase(
					UArcNeedsSurvivalAttributeSet::GetFatigueAttribute(),
					FatigueFragment.CurrentValue);
			}
		}
	);
}
