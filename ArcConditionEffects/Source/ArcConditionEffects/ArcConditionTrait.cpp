// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcConditionsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    BuildContext.AddFragment<FArcConditionStatesFragment>();

    FArcConditionConfigsShared ConfigFragment;
    ConfigFragment.Configs[(int32)EArcConditionType::Burning]     = BurningConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Bleeding]    = BleedingConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Chilled]     = ChilledConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Shocked]     = ShockedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Poisoned]    = PoisonedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Diseased]    = DiseasedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Weakened]    = WeakenedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Oiled]       = OiledConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Wet]         = WetConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Corroded]    = CorrodedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Blinded]     = BlindedConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Suffocating] = SuffocatingConfig;
    ConfigFragment.Configs[(int32)EArcConditionType::Exhausted]   = ExhaustedConfig;

    FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
    const FConstSharedStruct SharedConfig = EntityManager.GetOrCreateConstSharedFragment(ConfigFragment);
    BuildContext.AddConstSharedFragment(SharedConfig);
}
