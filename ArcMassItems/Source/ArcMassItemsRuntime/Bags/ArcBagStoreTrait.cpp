// Copyright Lukasz Baran. All Rights Reserved.

#include "Bags/ArcBagStoreTrait.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Bags/ArcBagDefinition.h"
#include "Bags/ArcBagId.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Mass/EntityManager.h"
#include "StructUtils/StructView.h"

void UArcMassBagStoreTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	// Build one template value and emit it via the typed overload so the spawner
	// can copy it to each entity. Bag instances are intentionally absent here —
	// FArcBagId::Generate() must run per-entity (in InitializeEntity).
	FArcMassBagStoreFragment Fragment;
	UScriptStruct* Effective = TargetStoreType ? TargetStoreType.Get() : FArcMassItemStoreFragment::StaticStruct();
	Fragment.TargetStoreType = Effective;
	Out.AddSparseFragment<FArcMassBagStoreFragment>(Fragment);
}

void UArcMassBagStoreTrait::InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const
{
	// Resolve the per-entity sparse fragment seeded by BuildSparseFragments.
	FStructView View = Mgr.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), Entity);
	FArcMassBagStoreFragment* Frag = View.GetPtr<FArcMassBagStoreFragment>();
	if (Frag == nullptr)
	{
		return;
	}

	for (const TObjectPtr<UArcBagDefinition>& DefPtr : InitialBags)
	{
		UArcBagDefinition* Def = DefPtr.Get();
		if (Def == nullptr)
		{
			continue;
		}

		FArcBagInstance Instance;
		Instance.BagId = FArcBagId::Generate();
		Instance.DefinitionId = Def->GetPrimaryAssetId();
		// Set the resolved cache directly: server-side ops resolve the definition
		// (acceptance/slot rules) without depending on AssetManager registration.
		Instance.CachedDefinition = Def;
		Instance.Slots.SetNum(Def->SlotCount);
		Frag->Bags.AddItem(MoveTemp(Instance));
	}
}
