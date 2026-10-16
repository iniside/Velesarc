// Copyright Lukasz Baran. All Rights Reserved.

#include "Combat/ArcRPGZoneArmorSource.h"

#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "Combat/ArcRPGAttributeSet_ItemArmor.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcEffectContext.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Mass/EntityManager.h"

namespace ArcRPGZoneArmorSourcePrivate
{
	FGameplayTag FindStruckZone(const FGameplayTagContainer& TargetTags)
	{
		const FGameplayTag ZoneRoot = ArcMassMechanicsTags::HitZone;
		for (const FGameplayTag& Tag : TargetTags)
		{
			if (Tag != ZoneRoot && Tag.MatchesTag(ZoneRoot))
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	bool CoversZone(const FArcItemData& Item, const FGameplayTag& Zone)
	{
		if (!Item.GetSlotId().IsValid() || !Item.IsActive() || !Item.GetItemDefinition())
		{
			return false;
		}

		const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(&Item);
		return TagsFragment && TagsFragment->ItemTags.HasTagExact(Zone);
	}
}

FArcRPGExtraCaptureSource_ZoneItemStat::FArcRPGExtraCaptureSource_ZoneItemStat()
{
	StoreType = FArcMassItemStoreFragment::StaticStruct();
}

bool FArcRPGExtraCaptureSource_ZoneItemStat::ResolveValue(const FArcEffectContext& Context, FMassEntityManager& EntityManager,
	const FArcEvaluateParameters& EvalParams, float& OutValue) const
{
	using namespace ArcRPGZoneArmorSourcePrivate;

	OutValue = 0.f;

	const FGameplayTag Zone = FindStruckZone(EvalParams.TargetTags);
	if (!Zone.IsValid() || !Stat.IsValid() || !StoreType || !StoreType->IsChildOf(FArcMassItemStoreFragment::StaticStruct())
		|| !EntityManager.IsEntityValid(Context.TargetEntity))
	{
		return true;
	}

	const FArcMassItemStoreFragment* Store = EntityManager.GetSparseElementDataForEntity(StoreType.Get(), Context.TargetEntity)
		.GetPtr<FArcMassItemStoreFragment>();
	if (!Store)
	{
		return true;
	}

	float Value = 0.f;
	for (const FArcMassReplicatedItem& Entry : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Item = Entry.ToItem();
		if (!Item || !CoversZone(*Item, Zone))
		{
			continue;
		}

		const FArcItemInstance_ItemStats* Stats = ArcItemsHelper::FindInstance<FArcItemInstance_ItemStats>(Item);
		if (Stats)
		{
			Value = FMath::Max(Value, Stats->GetStatValue(Stat, EvalParams.SourceTags));
		}
	}

	OutValue = Value;
	return true;
}

EArcCaptureSource FArcRPGExtraCaptureSource_ZoneItemStat::GetCaptureSide() const
{
	return EArcCaptureSource::Target;
}

FArcRPGExtraCaptureSource_ZoneArmor::FArcRPGExtraCaptureSource_ZoneArmor()
{
	Stat = FArcRPGAttributeSet_ItemArmor::GetArmorAttribute();
}

FArcRPGExtraCaptureSource_ZoneResistance::FArcRPGExtraCaptureSource_ZoneResistance()
{
	Stat = FArcRPGAttributeSet_ItemArmor::GetResistanceAttribute();
}
