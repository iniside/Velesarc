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

#include "Traits/ArcSettlementNeedsTrait.h"

#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "Fragments/ArcSettlementNeedFragments.h"
#include "Data/ArcSettlementNeedTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettlementNeedsTrait)

namespace ArcSettlementNeedsTraitPrivate
{
	template <typename TNeedFragment>
	void AddNeedFragment(FMassEntityTemplateBuildContext& BuildContext, const float InitialNeedValue)
	{
		TNeedFragment& Fragment = BuildContext.AddFragment_GetRef<TNeedFragment>();
		Fragment.CurrentValue = InitialNeedValue;
	}
} // namespace ArcSettlementNeedsTraitPrivate

void UArcSettlementNeedsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	using namespace ArcSettlementNeedsTraitPrivate;

	// Add all six need fragments with initial values
	AddNeedFragment<FArcSettlementFoodNeed>(BuildContext, InitialNeedValue);
	AddNeedFragment<FArcSettlementWaterNeed>(BuildContext, InitialNeedValue);
	AddNeedFragment<FArcSettlementShelterNeed>(BuildContext, InitialNeedValue);
	AddNeedFragment<FArcSettlementClothingNeed>(BuildContext, InitialNeedValue);
	AddNeedFragment<FArcSettlementHealthcareNeed>(BuildContext, InitialNeedValue);
	AddNeedFragment<FArcSettlementMoraleNeed>(BuildContext, InitialNeedValue);

	// Build and register shared config fragment
	FArcSettlementNeedsConfigFragment Config;

	if (FoodConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementFoodNeed::StaticStruct(), FoodConfig);
	}
	if (WaterConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementWaterNeed::StaticStruct(), WaterConfig);
	}
	if (ShelterConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementShelterNeed::StaticStruct(), ShelterConfig);
	}
	if (ClothingConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementClothingNeed::StaticStruct(), ClothingConfig);
	}
	if (HealthcareConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementHealthcareNeed::StaticStruct(), HealthcareConfig);
	}
	if (MoraleConfig)
	{
		Config.NeedConfigs.Add(FArcSettlementMoraleNeed::StaticStruct(), MoraleConfig);
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FSharedStruct SharedConfig = EntityManager.GetOrCreateSharedFragment(Config);
	BuildContext.AddSharedFragment(SharedConfig);
}
