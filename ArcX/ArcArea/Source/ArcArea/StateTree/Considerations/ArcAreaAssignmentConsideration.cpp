// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAssignmentConsideration.h"
#include "MassStateTreeExecutionContext.h"
#include "Mass/ArcAreaAssignmentFragments.h"

float FArcAreaAssignmentConsideration::GetScore(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	bool bIsAssigned = false;
	if (const FArcAreaAssignmentFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity))
	{
		bIsAssigned = Fragment->IsAssigned();
	}

	return (bInvert ? !bIsAssigned : bIsAssigned) ? 1.0f : 0.0f;
}

#if WITH_EDITOR
FText FArcAreaAssignmentConsideration::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return bInvert
		? NSLOCTEXT("ArcArea", "AssignmentConsiderationDescInvert", "Is NOT Assigned to Area")
		: NSLOCTEXT("ArcArea", "AssignmentConsiderationDesc", "Is Assigned to Area");
}
#endif
