// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisualizationTrait.h"

#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

// ---------------------------------------------------------------------------
// UArcMobileVisEntityTrait
// ---------------------------------------------------------------------------

void UArcMobileVisEntityTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassActorFragment>();

	BuildContext.AddFragment<FArcMobileVisLODFragment>();
	BuildContext.AddFragment<FArcMobileVisRepFragment>();
	BuildContext.AddTag<FArcMobileVisEntityTag>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	const FConstSharedStruct VisConfig = EntityManager.GetOrCreateConstSharedFragment(VisualizationConfig);
	BuildContext.AddConstSharedFragment(VisConfig);

	const FConstSharedStruct DistConfig = EntityManager.GetOrCreateConstSharedFragment(DistanceConfig);
	BuildContext.AddConstSharedFragment(DistConfig);
}

// ---------------------------------------------------------------------------
// UArcMobileVisSourceTrait
// ---------------------------------------------------------------------------

void UArcMobileVisSourceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.AddTag<FArcMobileVisSourceTag>();
}
