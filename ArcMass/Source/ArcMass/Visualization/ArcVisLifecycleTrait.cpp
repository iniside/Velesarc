// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcVisLifecycleTrait.h"
#include "ArcLifecyclePhaseVisualsFragment.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcVisLifecycleTrait)

void UArcVisLifecycleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// Canonical lifecycle fragment + tag
	BuildContext.AddFragment<FArcLifecycleFragment>();
	BuildContext.AddTag<FArcLifecycleTag>();

	if (bAutoAdvance)
	{
		BuildContext.AddTag<FArcLifecycleAutoAdvanceTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	// Canonical lifecycle config (timing, initial/terminal phase)
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(LifecycleConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	// Per-phase visuals (optional — only authored if PhaseVisuals is non-empty)
	FArcLifecyclePhaseVisualsFragment VisualsFragment;
	VisualsFragment.PhaseVisuals = PhaseVisuals;
	const FConstSharedStruct VisualsShared = EntityManager.GetOrCreateConstSharedFragment(VisualsFragment);
	BuildContext.AddConstSharedFragment(VisualsShared);
}
