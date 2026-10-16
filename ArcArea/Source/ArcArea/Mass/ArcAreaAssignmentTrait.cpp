// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAssignmentTrait.h"

#include "Mass/EntityFragments.h"
#include "MassEntityTemplateRegistry.h"

void UArcAreaAssignmentTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAreaAssignmentFragment>();
	BuildContext.AddTag<FArcAreaAssignmentTag>();
	BuildContext.RequireFragment<FTransformFragment>();
}
