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

#include "ArcCraft/Recipe/ArcRecipeQuality.h"

int32 UArcQualityTierTable::GetTierValue(const FGameplayTag& InTag) const
{
	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (Mapping.TierTag == InTag)
		{
			return Mapping.TierValue;
		}
	}
	return -1;
}

float UArcQualityTierTable::GetQualityMultiplier(const FGameplayTag& InTag) const
{
	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (Mapping.TierTag == InTag)
		{
			return Mapping.QualityMultiplier;
		}
	}
	return 1.0f;
}

FGameplayTag UArcQualityTierTable::FindBestTierTag(const FGameplayTagContainer& InTags) const
{
	FGameplayTag BestTag;
	int32 BestValue = -1;

	for (const FArcQualityTierMapping& Mapping : Tiers)
	{
		if (InTags.HasTag(Mapping.TierTag) && Mapping.TierValue > BestValue)
		{
			BestValue = Mapping.TierValue;
			BestTag = Mapping.TierTag;
		}
	}

	return BestTag;
}

float UArcQualityTierTable::EvaluateQuality(const FGameplayTagContainer& InItemTags) const
{
	const FGameplayTag BestTag = FindBestTierTag(InItemTags);
	if (BestTag.IsValid())
	{
		return GetQualityMultiplier(BestTag);
	}
	return 1.0f;
}
