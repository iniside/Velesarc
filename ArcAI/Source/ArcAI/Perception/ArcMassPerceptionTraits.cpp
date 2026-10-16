// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPerceptionTraits.h"

void UArcPerceptionCombinedPerceiverTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	ArcMassPerception::ApplySightPerceiver(BuildContext, World, SightConfig);
	ArcMassPerception::ApplyHearingPerceiver(BuildContext, World, HearingConfig);
}

void UArcPerceptionCombinedPerceivableTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	ArcMassPerception::ApplySightPerceivable(BuildContext);
	ArcMassPerception::ApplyHearingPerceivable(BuildContext);
}
