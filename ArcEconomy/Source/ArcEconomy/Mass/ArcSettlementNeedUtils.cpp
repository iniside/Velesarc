// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcSettlementNeedUtils.h"
#include "MassEntityManager.h"
#include "Mass/ArcEconomyFragments.h"
#include "Data/ArcSettlementNeedTypes.h"
#include "Data/ArcSettlementNeedDataAsset.h"
#include "Fragments/ArcNeedFragment.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

namespace ArcEconomy::SettlementNeeds
{

void UpdateNeedsFromMarket(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    const FArcSettlementMarketFragment& Market)
{
    const FArcSettlementNeedsConfigFragment* NeedsConfig =
        EntityManager.GetSharedFragmentDataPtr<FArcSettlementNeedsConfigFragment>(SettlementEntity);
    if (!NeedsConfig)
    {
        return;
    }

    const float StorageCap = FMath::Max(1.0f, static_cast<float>(Market.TotalStorageCap));

    for (const TPair<TObjectPtr<UScriptStruct>, TObjectPtr<UArcSettlementNeedDataAsset>>& ConfigPair : NeedsConfig->NeedConfigs)
    {
        UScriptStruct* NeedStruct = ConfigPair.Key;
        const UArcSettlementNeedDataAsset* DataAsset = ConfigPair.Value;
        if (!NeedStruct || !DataAsset)
        {
            continue;
        }

        // Sum weighted supply for items matching any satisfaction tag
        float WeightedSupply = 0.0f;

        for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& MarketPair : Market.PriceTable)
        {
            const UArcItemDefinition* ItemDef = MarketPair.Key;
            if (!ItemDef)
            {
                continue;
            }

            const FArcItemFragment_Tags* ItemTags = ItemDef->FindFragment<FArcItemFragment_Tags>();
            if (!ItemTags)
            {
                continue;
            }

            // Sum supply quantity from all sources for this item
            int32 ItemSupply = 0;
            for (const FArcMarketSupplySource& Source : MarketPair.Value.SupplySources)
            {
                ItemSupply += Source.Quantity;
            }

            if (ItemSupply <= 0)
            {
                continue;
            }

            // Check against each satisfaction source
            for (const FArcNeedSatisfactionEntry& Entry : DataAsset->SatisfactionSources)
            {
                if (ItemTags->AssetTags.HasTag(Entry.ItemTag))
                {
                    WeightedSupply += static_cast<float>(ItemSupply) * Entry.Weight;
                    break; // One match per item is enough
                }
            }
        }

        // Map supply to need value: more supply = lower need (0-100)
        // Use storage cap as the reference maximum
        const float SupplyRatio = WeightedSupply / StorageCap;
        const float NeedValue = FMath::Clamp(100.0f - (SupplyRatio * 100.0f), 0.0f, 100.0f);

        // Write to the need fragment on the settlement entity
        FStructView NeedView = EntityManager.GetFragmentDataStruct(SettlementEntity, NeedStruct);
        if (NeedView.IsValid())
        {
            FArcNeedFragment& NeedFragment = NeedView.Get<FArcNeedFragment>();
            NeedFragment.CurrentValue = NeedValue;
        }
    }
}

} // namespace ArcEconomy::SettlementNeeds
