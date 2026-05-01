// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesTestTypes.h"
#include "MassEntityManager.h"
#include "StructUtils/InstancedStruct.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"

FMassEntityHandle ArcMassAbilitiesTestHelpers::CreateAbilityEntity(FMassEntityManager& EntityManager, float HealthBase, float ArmorBase)
{
	FArcTestStatsFragment Stats;
	Stats.Health.BaseValue = HealthBase;
	Stats.Health.CurrentValue = HealthBase;
	Stats.Armor.BaseValue = ArmorBase;
	Stats.Armor.CurrentValue = ArmorBase;

	TArray<FInstancedStruct> Instances;
	Instances.Add(FInstancedStruct::Make(Stats));
	Instances.Add(FInstancedStruct::Make(FArcEffectStackFragment()));
	Instances.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
	Instances.Add(FInstancedStruct::Make(FArcOwnedTagsFragment()));
	Instances.Add(FInstancedStruct::Make(FArcPendingAttributeOpsFragment()));

	return EntityManager.CreateEntity(Instances);
}
