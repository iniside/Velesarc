// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerceptionTraits.h"

#include "MassEntityTemplateRegistry.h"
#include "ArcMass/ArcMassSpatialHashSubsystem.h"

void UArcPerceptionCombinedPerceiverTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();

	// Sight
	BuildContext.AddFragment<FArcMassSightPerceptionResult>();
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct SightFragment = EntityManager.GetOrCreateConstSharedFragment(SightConfig);
	BuildContext.AddConstSharedFragment(SightFragment);

	// Hearing
	BuildContext.AddFragment<FArcMassHearingPerceptionResult>();
	const FConstSharedStruct HearingFragment = EntityManager.GetOrCreateConstSharedFragment(HearingConfig);
	BuildContext.AddConstSharedFragment(HearingFragment);
}

void UArcPerceptionCombinedPerceivableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FArcMassSpatialHashFragment>();

	// Sight perceivable
	BuildContext.AddTag<FArcMassSightPerceivableTag>();

	// Hearing perceivable
	BuildContext.AddTag<FArcMassHearingPerceivableTag>();
	BuildContext.AddFragment<FArcMassPerceivableStimuliHearingFragment>();
}
