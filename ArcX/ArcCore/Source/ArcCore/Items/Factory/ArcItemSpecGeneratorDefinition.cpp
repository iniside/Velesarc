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

#include "Items/Factory/ArcItemSpecGeneratorDefinition.h"

#include "ArcItemFactoryStats.h"
#include "ArcLootSubsystem.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

FArcItemSpec FArcItemSpecGenerator::GenerateItemSpec(UArcItemSpecGeneratorDefinition* FactoryData
													 , const FArcNamedPrimaryAssetId& Row
													 , AActor* From
													 , APlayerController* For) const
{
	FArcItemSpec Spec;
	Spec.SetItemDefinition(Row);
	return Spec;
}

void FArcItemGenerator_SpecDefinitionSingleItem::GenerateItems(TArray<FArcItemSpec>& OutItems
	, const UArcItemGeneratorDefinition* InDef
	, AActor* From
	, APlayerController* For) const
{
	const int32 NumDef = InDef->ItemDefinitions.Num();
	if (NumDef == 0)
	{
		return;
	}
	const int32 DefIdx = FMath::RandRange(0, NumDef - 1);
	OutItems.Reset(1);

	
	FArcItemSpec Spec = ItemGenerator.Get<FArcItemSpecGenerator>().GenerateItemSpec(SpecDefinition, InDef->ItemDefinitions[DefIdx], From, For);
	OutItems.Add(MoveTemp(Spec));
}

FArcItemSpec FArcItemSpecGenerator_RandomItemStats::GenerateItemSpec(UArcItemSpecGeneratorDefinition* FactoryData
																	 , const FArcNamedPrimaryAssetId& Row
																	 , AActor* From
																	 , APlayerController* For) const
{
	FArcItemSpec Spec;
	Spec.SetItemDefinition(Row);
	if (FactoryData == nullptr)
	{
		return Spec;
	}

	if (FactoryData->ItemStats == nullptr)
	{
		return Spec;
	}

	

	FArcItemFragment_ItemStats* ItemStats = new FArcItemFragment_ItemStats;

	for (int32 Idx = 0; Idx < MaxStats; ++Idx)
	{
		const int32 StatPoolNum = FactoryData->ItemStats->GenItemStats.Num();
		if (StatPoolNum == 0)
		{
			break;
		}

		const int32 StatPoolIdx = FMath::RandRange(0, StatPoolNum - 1);
		const FArcFactoryItemStatSet& StatPoolSet = FactoryData->ItemStats->GenItemStats[StatPoolIdx];
		const int32 StatNum = StatPoolSet.AttributePool.Num();
		if (StatNum == 0)
		{
			break;
		}

		FArcFactoryItemStatData StatData;
		bool bFound = false;
		int32 MaxAttempts = StatNum * 2;
		while (bFound == false && MaxAttempts-- > 0)
		{
			const int32 StatIdx = FMath::RandRange(0, StatNum - 1);
			StatData = StatPoolSet.AttributePool[StatIdx];
			if (ItemStats->DefaultStats.Num() == 0)
			{
				bFound = true;
				break;
			}

			const bool bContains = ItemStats->DefaultStats.ContainsByPredicate([StatData](const FArcItemAttributeStat& InStat)
			{
				return InStat.Attribute == StatData.Attribute;
			});

			if (bContains == false)
			{
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			break;
		}

		const float StatValue = FMath::RandRange(StatData.Min.GetValue(), StatData.Max.GetValue());

		const int32 DefaultIdx = ItemStats->DefaultStats.AddDefaulted();
		ItemStats->DefaultStats[DefaultIdx].Attribute = StatData.Attribute;
		ItemStats->DefaultStats[DefaultIdx].Value.SetValue(StatValue);
		
	}
	
	Spec.AddInstanceData(ItemStats);

	return Spec;
}
