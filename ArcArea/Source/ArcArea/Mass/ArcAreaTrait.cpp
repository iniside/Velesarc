// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaTrait.h"

#include "ArcAreaModule.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/EntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTree.h"
#include "Engine/World.h"

void UArcAreaTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcAreaTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	FArcAreaConfigFragment Config;
	Config.AreaDefinition = AreaDefinition;
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	// The SmartObject handle created by UArcSmartObjectAddObserver is the area's identity; without a
	// definition the observers would build a SmartObject from nullptr, so the composition is left out.
	if (SmartObjectDefinition)
	{
		BuildContext.AddFragment<FArcSmartObjectOwnerFragment>();
		BuildContext.AddTag<FArcSmartObjectTag>();

		FArcSmartObjectDefinitionSharedFragment SODefFragment;
		SODefFragment.SmartObjectDefinition = SmartObjectDefinition;
		const FConstSharedStruct SODefSharedFragment = EntityManager.GetOrCreateConstSharedFragment(SODefFragment);
		BuildContext.AddConstSharedFragment(SODefSharedFragment);
	}

	// Add StateTree execution fragments when a StateTree is provided.
	if (StateTree)
	{
		const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(&World);
		if (EnumHasAnyFlags(WorldExecutionFlags, UE::MassStateTree::ExecutionFlags) || BuildContext.IsInspectingData())
		{
			UMassStateTreeSubsystem* MassStateTreeSubsystem = World.GetSubsystem<UMassStateTreeSubsystem>();
			if (MassStateTreeSubsystem || BuildContext.IsInspectingData())
			{
				if (StateTree->IsReadyToRun() || BuildContext.IsInspectingData())
				{
					FMassStateTreeSharedFragment SharedStateTree;
					SharedStateTree.StateTree = StateTree;

					const FConstSharedStruct StateTreeFragment = EntityManager.GetOrCreateConstSharedFragment(SharedStateTree);
					BuildContext.AddConstSharedFragment(StateTreeFragment);

					BuildContext.AddFragment<FMassStateTreeInstanceFragment>();
				}
			}
		}
	}
}

bool UArcAreaTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	if (!SmartObjectDefinition)
	{
		UE_LOGF(LogArcAreaDiag, Error,"UArcAreaTrait on '%ls': SmartObjectDefinition is not set — an area is identified by its SmartObject handle and cannot be registered without one.",
			*GetPathName());
		return false;
	}

	return true;
}
