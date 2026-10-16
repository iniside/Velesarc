// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActionTrait.h"

#include "ArcMassActionAsset.h"
#include "ArcMassActionFragment.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassActionTrait)

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

void UArcMassActionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcMassActionTag>();
}

void UArcMassActionTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	FArcMassActionFragment Fragment;
	TMap<FName, TObjectPtr<UArcMassStatelessActionAsset>>& StatelessActions = Fragment.GetMutableStatelessActions();
	TMap<FName, FArcMassStatefulActionList>& StatefulActions = Fragment.GetMutableStatefulActions();
	for (const TPair<FName, FArcMassActionTraitEntry>& Pair : ActionMap)
	{
		if (Pair.Value.StatelessActions)
		{
			StatelessActions.Add(Pair.Key, Pair.Value.StatelessActions);
		}
		if (Pair.Value.StatefulActions)
		{
			FArcMassStatefulActionList& List = StatefulActions.FindOrAdd(Pair.Key);
			List.Actions = Pair.Value.StatefulActions->Actions;
		}
	}
	Out.AddSparseFragment(Fragment);
}
