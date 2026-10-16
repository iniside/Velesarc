// Copyright Lukasz Baran. All Rights Reserved.

#include "Teams/ArcMassTeamFragment.h"

#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassTeamFragment)

void UArcMassTeamTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment_GetRef<FArcMassTeamFragment>().TeamId = TeamId;
}
