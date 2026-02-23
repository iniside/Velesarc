// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualizationTrait.h"

#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"

void UArcEntityVisualizationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();
	
	BuildContext.AddFragment<FArcVisRepresentationFragment>();
	BuildContext.AddTag<FArcVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(VisualizationConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
