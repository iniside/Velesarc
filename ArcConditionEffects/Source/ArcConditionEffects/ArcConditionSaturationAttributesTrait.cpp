// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionSaturationAttributesTrait.h"

#include "ArcConditionSaturationAttributes.h"
#include "MassEntityTemplateRegistry.h"

void UArcConditionSaturationAttributesTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcConditionSaturationAttributes>();
}
