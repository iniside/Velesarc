// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWorkAreaBoundsTrait.h"

#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcWorkAreaBoundsTrait)

void UArcWorkAreaBoundsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();

	FArcWorkAreaBoundsFragment& BoundsFragment = BuildContext.AddFragment_GetRef<FArcWorkAreaBoundsFragment>();
	BoundsFragment = Bounds;
}
