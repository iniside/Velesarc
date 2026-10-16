// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAbilitiesTestTypes.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"

FMassEntityHandle ArcMassAbilitiesTestHelpers::CreateAbilityEntity(FMassEntityManager& EntityManager, float HealthBase, float ArmorBase)
{
	FArcAttributeSet_TestStats Stats;
	Stats.Health.BaseValue = HealthBase;
	Stats.Health.CurrentValue = HealthBase;
	Stats.Armor.BaseValue = ArmorBase;
	Stats.Armor.CurrentValue = ArmorBase;

	// Sparse fragments (FArcAttributeSet_TestStats, FArcOwnedTagsFragment, FArcPendingAttributeOpsFragment)
	// are not part of archetype composition — EntityManager.CreateEntity(FragmentInstanceList) silently
	// drops them. Use UE::Mass::FEntityBuilder which routes sparse adds through SetFragmentsData at
	// Commit. See ArcEntityBuilderSparseTests.cpp for the canonical pattern.
	UE::Mass::FEntityBuilder Builder(EntityManager);
	return Builder
		.Add<FArcAttributeSet_TestStats>(Stats)
		.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcPendingAttributeOpsFragment>()
		.Commit();
}

FMassEntityHandle ArcMassAbilitiesTestHelpers::CreateEntityFromInstances(FMassEntityManager& EntityManager, TConstArrayView<FInstancedStruct> Instances)
{
	// Add every instance (regular and sparse) to FEntityBuilder so the entity is committed with
	// all fragments present and active — routing only regular fragments here would leave an
	// all-sparse list with an empty builder, producing an invalid handle that asserts in a later
	// AddSparseElementToEntity. The runtime-typed Builder.Add(FInstancedStruct) doesn't write the
	// sparse initial value reliably (empirically FArcAttributeSet_TestStats with non-default values
	// loses them), so re-seed each sparse fragment's authored value post-commit via the mutable
	// sparse accessor (the fragment is already present, so no structural change is needed).
	UE::Mass::FEntityBuilder Builder(EntityManager);
	TArray<const FInstancedStruct*, TInlineAllocator<8>> SparseInstances;
	for (const FInstancedStruct& Instance : Instances)
	{
		const UScriptStruct* Type = Instance.GetScriptStruct();
		if (!Type)
		{
			continue;
		}
		Builder.Add(Instance);
		if (UE::Mass::IsSparse(Type))
		{
			SparseInstances.Add(&Instance);
		}
	}

	const FMassEntityHandle Entity = Builder.Commit();

	for (const FInstancedStruct* SparsePtr : SparseInstances)
	{
		const UScriptStruct* Type = SparsePtr->GetScriptStruct();
		FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(Type, Entity);
		if (!SparseView.IsValid())
		{
			SparseView = EntityManager.AddSparseElementToEntity(Entity, Type);
		}
		uint8* DstMem = SparseView.GetMemory();
		const uint8* SrcMem = SparsePtr->GetMemory();
		if (DstMem && SrcMem)
		{
			Type->CopyScriptStruct(DstMem, SrcMem);
		}
	}

	return Entity;
}

int32 FArcTestExitFlagTask::ExitStateCount = 0;
