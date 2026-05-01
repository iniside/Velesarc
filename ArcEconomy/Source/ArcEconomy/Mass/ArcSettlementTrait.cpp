// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcSettlementTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Strategy/ArcPopulationFragment.h"

void UArcSettlementTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    FArcSettlementFragment& SettlementFragment = BuildContext.AddFragment_GetRef<FArcSettlementFragment>();
    FArcSettlementMarketFragment& MarketFragment = BuildContext.AddFragment_GetRef<FArcSettlementMarketFragment>();
    BuildContext.AddFragment<FArcSettlementWorkforceFragment>();

    SettlementFragment = SettlementConfig;
    MarketFragment.K = InitialK;

    if (GovernorDataAsset)
    {
        FArcGovernorFragment& GovernorFragment = BuildContext.AddFragment_GetRef<FArcGovernorFragment>();
        GovernorFragment.GovernorConfig = GovernorDataAsset;
    }

    // Population tracking — mutable fragment, MaxPopulation changes at runtime
    FArcPopulationFragment& PopulationFragment = BuildContext.AddFragment_GetRef<FArcPopulationFragment>();
    PopulationFragment.MaxPopulation = InitialMaxPopulation;
    PopulationFragment.GrowthIntervalSeconds = GrowthIntervalSeconds;
    PopulationFragment.StarvationGracePeriod = StarvationGracePeriod;

    // NPC entity config — stored on settlement fragment for director to read
    SettlementFragment.NPCEntityConfig = NPCEntityConfig;
    SettlementFragment.SpawnLocationQuery = SpawnLocationQuery;
}
