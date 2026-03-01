// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAssignmentTrait.h"

#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcAreaAssignmentTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAreaAssignmentFragment>();
	BuildContext.AddTag<FArcAreaAssignmentTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(AssignmentConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
