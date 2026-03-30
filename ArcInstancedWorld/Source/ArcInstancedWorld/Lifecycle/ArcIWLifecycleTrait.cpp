// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWLifecycleTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWLifecycleTrait)

void UArcIWLifecycleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	check(LifecycleConfig.PhaseDurations.Num() > 0);
	check(LifecycleConfig.IsValidPhase(LifecycleConfig.InitialPhase));
	check(LifecycleConfig.IsValidPhase(LifecycleConfig.TerminalPhase));

	BuildContext.AddFragment<FArcLifecycleFragment>();
	BuildContext.AddTag<FArcLifecycleTag>();

	if (bAutoAdvance)
	{
		BuildContext.AddTag<FArcLifecycleAutoAdvanceTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	const FConstSharedStruct LifecycleConfigFragment = EntityManager.GetOrCreateConstSharedFragment(LifecycleConfig);
	BuildContext.AddConstSharedFragment(LifecycleConfigFragment);

	if (VisConfig.MeshOverrides.Num() > 0)
	{
		const FConstSharedStruct VisConfigFragment = EntityManager.GetOrCreateConstSharedFragment(VisConfig);
		BuildContext.AddConstSharedFragment(VisConfigFragment);
	}

	if (bPersistLifecycle)
	{
		BuildContext.AddFragment<FArcMassPersistenceFragment>();

		FArcMassPersistenceConfigFragment PersistConfig;
		PersistConfig.AllowedFragments.Add(FArcLifecycleFragment::StaticStruct());
		const FConstSharedStruct PersistConfigFragment = EntityManager.GetOrCreateConstSharedFragment(PersistConfig);
		BuildContext.AddConstSharedFragment(PersistConfigFragment);
	}
}
