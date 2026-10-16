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
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "Tags/ArcGameplayTagRequirements.h"

#include "ArcStatCatalogData.generated.h"

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcStatCatalogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FName StatId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float Sign = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (Categories = "ItemStat.Category"))
	FGameplayTag Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FArcGameplayTagRequirements SourceTagReqs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	FArcGameplayTagRequirements TargetTagReqs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float Max = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	bool bItemSide = false;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcStatCatalogData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Catalog")
	FGuid CatalogId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog", meta = (TitleProperty = "StatId"))
	TArray<FArcStatCatalogEntry> Entries;
};
