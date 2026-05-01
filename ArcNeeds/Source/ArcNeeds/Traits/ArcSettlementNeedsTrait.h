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
#include "MassEntityTraitBase.h"
#include "Data/ArcSettlementNeedDataAsset.h"
#include "ArcSettlementNeedsTrait.generated.h"

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Settlement Needs", Category = "Needs"))
class ARCNEEDS_API UArcSettlementNeedsTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	// -------------------------------------------------------------------------
	// Survival Tier
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Needs|Survival")
	TObjectPtr<UArcSettlementNeedDataAsset> FoodConfig;

	UPROPERTY(EditAnywhere, Category = "Needs|Survival")
	TObjectPtr<UArcSettlementNeedDataAsset> WaterConfig;

	UPROPERTY(EditAnywhere, Category = "Needs|Survival")
	TObjectPtr<UArcSettlementNeedDataAsset> ShelterConfig;

	// -------------------------------------------------------------------------
	// Comfort Tier
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Needs|Comfort")
	TObjectPtr<UArcSettlementNeedDataAsset> ClothingConfig;

	UPROPERTY(EditAnywhere, Category = "Needs|Comfort")
	TObjectPtr<UArcSettlementNeedDataAsset> HealthcareConfig;

	UPROPERTY(EditAnywhere, Category = "Needs|Comfort")
	TObjectPtr<UArcSettlementNeedDataAsset> MoraleConfig;

	// -------------------------------------------------------------------------
	// General
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Needs", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float InitialNeedValue = 50.0f;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
