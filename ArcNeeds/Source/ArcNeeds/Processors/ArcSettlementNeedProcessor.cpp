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

#include "Processors/ArcSettlementNeedProcessor.h"

#include "MassExecutionContext.h"
#include "Fragments/ArcSettlementNeedFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettlementNeedProcessor)

UArcSettlementNeedProcessor::UArcSettlementNeedProcessor()
	: SettlementNeedsQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcSettlementNeedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SettlementNeedsQuery.Initialize(EntityManager);
	SettlementNeedsQuery.AddRequirement<FArcSettlementFoodNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.AddRequirement<FArcSettlementWaterNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.AddRequirement<FArcSettlementShelterNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.AddRequirement<FArcSettlementClothingNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.AddRequirement<FArcSettlementHealthcareNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.AddRequirement<FArcSettlementMoraleNeed>(EMassFragmentAccess::ReadWrite);
	SettlementNeedsQuery.RegisterWithProcessor(*this);
}

void UArcSettlementNeedProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcSettlementNeed);

	AccumulatedTime += Context.GetDeltaTimeSeconds();
	if (AccumulatedTime < 1.0f)
	{
		return;
	}

	const float DeltaTime = AccumulatedTime;
	AccumulatedTime = 0.0f;

	SettlementNeedsQuery.ForEachEntityChunk(Context, [DeltaTime](FMassExecutionContext& Ctx)
	{
		const TArrayView<FArcSettlementFoodNeed> FoodNeeds = Ctx.GetMutableFragmentView<FArcSettlementFoodNeed>();
		const TArrayView<FArcSettlementWaterNeed> WaterNeeds = Ctx.GetMutableFragmentView<FArcSettlementWaterNeed>();
		const TArrayView<FArcSettlementShelterNeed> ShelterNeeds = Ctx.GetMutableFragmentView<FArcSettlementShelterNeed>();
		const TArrayView<FArcSettlementClothingNeed> ClothingNeeds = Ctx.GetMutableFragmentView<FArcSettlementClothingNeed>();
		const TArrayView<FArcSettlementHealthcareNeed> HealthcareNeeds = Ctx.GetMutableFragmentView<FArcSettlementHealthcareNeed>();
		const TArrayView<FArcSettlementMoraleNeed> MoraleNeeds = Ctx.GetMutableFragmentView<FArcSettlementMoraleNeed>();

		for (int32 EntityIndex = 0; EntityIndex < Ctx.GetNumEntities(); ++EntityIndex)
		{
			if (FMath::Abs(FoodNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				FoodNeeds[EntityIndex].CurrentValue = FMath::Clamp(FoodNeeds[EntityIndex].CurrentValue + (FoodNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
			if (FMath::Abs(WaterNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				WaterNeeds[EntityIndex].CurrentValue = FMath::Clamp(WaterNeeds[EntityIndex].CurrentValue + (WaterNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
			if (FMath::Abs(ShelterNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				ShelterNeeds[EntityIndex].CurrentValue = FMath::Clamp(ShelterNeeds[EntityIndex].CurrentValue + (ShelterNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
			if (FMath::Abs(ClothingNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				ClothingNeeds[EntityIndex].CurrentValue = FMath::Clamp(ClothingNeeds[EntityIndex].CurrentValue + (ClothingNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
			if (FMath::Abs(HealthcareNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				HealthcareNeeds[EntityIndex].CurrentValue = FMath::Clamp(HealthcareNeeds[EntityIndex].CurrentValue + (HealthcareNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
			if (FMath::Abs(MoraleNeeds[EntityIndex].ChangeRate) > UE_SMALL_NUMBER)
			{
				MoraleNeeds[EntityIndex].CurrentValue = FMath::Clamp(MoraleNeeds[EntityIndex].CurrentValue + (MoraleNeeds[EntityIndex].ChangeRate * DeltaTime), 0.f, 100.f);
			}
		}
	});
}
