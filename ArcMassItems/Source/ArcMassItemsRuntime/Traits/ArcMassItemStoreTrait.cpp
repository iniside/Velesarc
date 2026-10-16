// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcMassItemStoreTrait.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcMassItemEventFragment.h"
#include "MassEntityTemplateRegistry.h"

void UArcMassItemStoreTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	if (StoreTypes.IsEmpty())
	{
		Out.AddSparseFragment<FArcMassItemStoreFragment>();
	}
	else
	{
		for (const TObjectPtr<UScriptStruct>& Type : StoreTypes)
		{
			UScriptStruct* Effective = Type ? Type.Get() : FArcMassItemStoreFragment::StaticStruct();
			FInstancedStruct Instance;
			Instance.InitializeAs(Effective);
			Out.AddSparseFragment(FConstStructView(Instance));
		}
	}

	// Singleton — events tag themselves with StoreType so one fragment serves N stores.
	Out.AddSparseFragment<FArcMassItemEventFragment>();
}
