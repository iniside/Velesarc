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

#include "Fragments/ArcNeedFragment.h"
#include "ArcSettlementNeedFragments.generated.h"

// ============================================================================
// Survival Tier
// ============================================================================

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementFoodNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementWaterNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementShelterNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};

// ============================================================================
// Comfort Tier
// ============================================================================

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementClothingNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementHealthcareNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCNEEDS_API FArcSettlementMoraleNeed : public FArcNeedFragment
{
	GENERATED_BODY()
};
