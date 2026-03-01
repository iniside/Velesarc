// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAsyncMessageEndpointTrait.h"
#include "ArcMassAsyncMessageEndpointFragment.h"
#include "MassEntityTemplateRegistry.h"

void UArcMassAsyncMessageEndpointTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcMassAsyncMessageEndpointFragment>();
	BuildContext.AddTag<FArcMassAsyncMessageEndpointTag>();
}
