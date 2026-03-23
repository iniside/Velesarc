// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeVisualizationTrait.h"

#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"

void UArcCompositeVisualizationTrait::BuildTemplate(
	FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcCompositeVisRepFragment>();
	BuildContext.AddTag<FArcCompositeVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(VisualizationConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
