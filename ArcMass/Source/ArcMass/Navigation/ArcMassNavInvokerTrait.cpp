// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcMassNavInvokerTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassNavInvokerTrait)

void UArcMassNavInvokerTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassNavInvokerFragment& Fragment = BuildContext.AddFragment_GetRef<FMassNavInvokerFragment>();
	Fragment.GenerationRadius = GenerationRadius;
	Fragment.RemovalRadius    = FMath::Max(RemovalRadius, GenerationRadius);
	Fragment.SupportedAgents  = SupportedAgents;
	Fragment.Priority         = Priority;

	if (bStatic)
	{
		BuildContext.AddTag<FMassNavInvokerStaticTag>();
	}
}
