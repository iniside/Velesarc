// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLinearProjectileTrait.h"

#include "MassEntityTemplateRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLinearProjectileTrait)

void UArcLinearProjectileTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	Super::BuildTemplate(BuildContext, World);
	BuildContext.AddTag<FArcLinearProjectileTag>();
}
