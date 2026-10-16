// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGHitZoneConfig.h"

#include "ArcMassMechanics/ArcMassMechanicsTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ArcRPGHitZoneConfig"

FGameplayTag UArcRPGHitZoneConfig::RollZone() const
{
	float TotalWeight = 0.f;
	for (const FArcRPGHitZone& Entry : Zones)
	{
		if (Entry.Zone.IsValid())
		{
			TotalWeight += FMath::Max(0.f, Entry.Weight);
		}
	}

	if (TotalWeight <= 0.f)
	{
		return FGameplayTag();
	}

	float Roll = FMath::FRand() * TotalWeight;
	FGameplayTag LastWeighted;
	for (const FArcRPGHitZone& Entry : Zones)
	{
		const float Weight = FMath::Max(0.f, Entry.Weight);
		if (!Entry.Zone.IsValid() || Weight <= 0.f)
		{
			continue;
		}

		if (Roll < Weight)
		{
			return Entry.Zone;
		}
		Roll -= Weight;
		LastWeighted = Entry.Zone;
	}
	return LastWeighted;
}

#if WITH_EDITOR
EDataValidationResult UArcRPGHitZoneConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const FGameplayTag ZoneRoot = ArcMassMechanicsTags::HitZone;
	float TotalWeight = 0.f;
	for (const FArcRPGHitZone& Entry : Zones)
	{
		if (!Entry.Zone.IsValid() || !Entry.Zone.MatchesTag(ZoneRoot) || Entry.Zone == ZoneRoot)
		{
			Context.AddError(FText::Format(LOCTEXT("ZoneOutsideRoot", "Hit zone '{0}' is not a child of {1}."),
				FText::FromString(Entry.Zone.ToString()), FText::FromString(ZoneRoot.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		TotalWeight += FMath::Max(0.f, Entry.Weight);
	}

	if (TotalWeight <= 0.f)
	{
		Context.AddWarning(LOCTEXT("NoWeight", "No zone has a weight above 0; executions using this asset roll no zone."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
