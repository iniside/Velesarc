// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Tiers/ArcTierTypes.h"

#include "Effects/ArcEffectDefinition.h"

int32 FArcTierBands::GetTier(float Magnitude) const
{
	for (int32 Index = Thresholds.Num() - 1; Index >= 0; --Index)
	{
		if (Magnitude >= Thresholds[Index])
		{
			return Index + 1;
		}
	}
	return 0;
}

UArcEffectDefinition* UArcTierEffectTable::FindEffect(const FGameplayTag& Key, int32 Tier) const
{
	for (const FArcTierEffectEntry& Entry : Entries)
	{
		if (Entry.Tier == Tier && Entry.Key == Key)
		{
			return Entry.Effect;
		}
	}
	return nullptr;
}
