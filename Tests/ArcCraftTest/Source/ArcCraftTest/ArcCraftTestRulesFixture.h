// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Rarity/ArcCraftSettings.h"
#include "ArcCraft/Rarity/ArcStatCatalogData.h"
#include "ArcCraft/Shared/ArcItemFragment_StatRanges.h"

#include "ArcCraftTestRecordFiles.h"

/**
 * Writes a stat catalog record and a craft rules record that names it, and points
 * UArcCraftSettings::Rules at the rules record, so FArcCraftModifier_Stats resolves its StatId through
 * ArcCraftRarity::GetRules()->StatCatalog on the production path. The previous setting is restored on
 * destruction. Declare it after the FArcCraftTestRecordFiles it installs into, so it is restored before
 * the records are removed.
 */
class FArcCraftTestRulesFixture
{
public:
	FArcCraftTestRulesFixture() = default;
	FArcCraftTestRulesFixture(const FArcCraftTestRulesFixture&) = delete;
	FArcCraftTestRulesFixture& operator=(const FArcCraftTestRulesFixture&) = delete;

	~FArcCraftTestRulesFixture()
	{
		Restore();
	}

	/** A valid wearer attribute for catalog entries; any resolvable struct property works. */
	static FArcAttributeRef MakeStatAttribute()
	{
		FArcAttributeRef Ref;
		Ref.FragmentType = FArcItemAttributeStatRange::StaticStruct();
		Ref.PropertyName = GET_MEMBER_NAME_CHECKED(FArcItemAttributeStatRange, Min);
		return Ref;
	}

	/** A wearer-side catalog entry with Sign 1 and the given range. */
	static FArcStatCatalogEntry MakeEntry(FName StatId, float Min, float Max)
	{
		FArcStatCatalogEntry Entry;
		Entry.StatId = StatId;
		Entry.Attribute = MakeStatAttribute();
		Entry.Min = Min;
		Entry.Max = Max;
		return Entry;
	}

	/** Returns false when a record could not be written or indexed; nothing is installed then. */
	bool Install(FArcCraftTestRecordFiles& Records, const TArray<FArcStatCatalogEntry>& Entries)
	{
		FArcStatCatalogData Catalog;
		Catalog.Entries = Entries;
		const FPrimaryAssetId CatalogId = Records.Write(Catalog);
		if (!CatalogId.IsValid())
		{
			return false;
		}

		FArcCraftRulesData RulesData;
		RulesData.StatCatalog = FArcNamedPrimaryAssetId(CatalogId);
		const FPrimaryAssetId RulesId = Records.Write(RulesData);
		if (!RulesId.IsValid())
		{
			return false;
		}

		UArcCraftSettings* Settings = GetMutableDefault<UArcCraftSettings>();
		if (!bInstalled)
		{
			PreviousRules = Settings->Rules;
			bInstalled = true;
		}
		Settings->Rules = RulesId;
		return true;
	}

	void Restore()
	{
		if (bInstalled)
		{
			GetMutableDefault<UArcCraftSettings>()->Rules = PreviousRules;
			bInstalled = false;
		}
	}

private:
	FArcNamedPrimaryAssetId PreviousRules;
	bool bInstalled = false;
};
