// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAssignmentCondition.h"
#include "MassStateTreeExecutionContext.h"
#include "Mass/ArcAreaAssignmentFragments.h"

bool FArcAreaAssignmentCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	bool bIsAssigned = false;
	if (const FArcAreaAssignmentFragment* Fragment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity))
	{
		bIsAssigned = Fragment->IsAssigned();
	}

	return bInvert ? !bIsAssigned : bIsAssigned;
}
