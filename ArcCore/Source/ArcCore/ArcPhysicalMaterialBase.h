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


#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ArcPhysicalMaterialBase.generated.h"

/**
 *
 */
UCLASS()
class ARCCORE_API UArcPhysicalMaterialBase : public UPhysicalMaterial
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditDefaultsOnly
		, Category = "Arc Core"
		, meta = (Categories = "Arc.PhysicalMaterial"))
	FGameplayTag MaterialTag;

	UPROPERTY(EditDefaultsOnly
		, Category = "Arc Core"
		, meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;

public:
	FString GetMaterial() const
	{
		FString FullTag = MaterialTag.GetTagName().ToString();
		if (FullTag.Len() > 0)
		{
			TArray<FString> Parsed;
			FullTag.ParseIntoArray(Parsed
				, TEXT("."));

			const FString Material = Parsed.Last();
			return Material;
		}

		return FString();
	}
};
