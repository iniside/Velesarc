// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassGoalPlanInfoSharedFragment.h"

#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcMassGoalPlanInfoTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	const FConstSharedStruct SmartObjectDefFragment = EntityManager.GetOrCreateConstSharedFragment(GoalPlanInfo);
	BuildContext.AddConstSharedFragment(SmartObjectDefFragment);
}
