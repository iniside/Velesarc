// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaTrait.h"

#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "MassEntityFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "MassStateTreeTypes.h"
#include "StateTree.h"
#include "Engine/World.h"

void UArcAreaTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAreaFragment>();
	BuildContext.AddTag<FArcAreaTag>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	FArcAreaConfigFragment Config;
	Config.AreaDefinition = AreaDefinition;
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);

	// Add smart object ownership fragments when a definition is provided.
	// UArcSmartObjectAddObserver will create the SmartObject at the entity's transform.
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
	if (!StateTree)
	{
		return true;
	}

	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(&World);
	if (!EnumHasAnyFlags(WorldExecutionFlags, EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server))
	{
		return true;
	}

	return true;
}
