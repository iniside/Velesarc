// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

struct FMassEntityManager;
struct FArcSettlementMarketFragment;

namespace ArcEconomy::SettlementNeeds
{
    /** Update settlement need fragments based on current market supply levels.
     *  Reads the NeedsConfig shared fragment and writes to each need fragment
     *  on the settlement entity. Safe to call from game thread only. */
    void UpdateNeedsFromMarket(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const FArcSettlementMarketFragment& Market);
}
