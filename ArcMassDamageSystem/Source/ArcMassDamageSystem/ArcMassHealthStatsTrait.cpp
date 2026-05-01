// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHealthStatsTrait.h"

#include "ArcMassHealthStatsFragment.h"
#include "ArcMassHealthClampHandler.h"
#include "MassEntityTemplateRegistry.h"

void UArcMassHealthStatsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FArcMassHealthStatsFragment HealthFrag;
	HealthFrag.Health.InitValue(DefaultHealth);
	HealthFrag.MaxHealth.InitValue(DefaultMaxHealth);
	BuildContext.AddFragment(FConstStructView::Make(HealthFrag));
}
