// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaStateTreeTrait.h"

#include "MassStateTreeFragments.h"
#include "MassStateTreeSubsystem.h"
#include "MassStateTreeTypes.h"
#include "MassEntityUtils.h"
#include "MassEntityTemplateRegistry.h"
#include "StateTree.h"
#include "ArcArea/Mass/ArcAreaFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAreaStateTreeTrait)

void UArcAreaStateTreeTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(&World);
	if (!EnumHasAnyFlags(WorldExecutionFlags, UE::MassStateTree::ExecutionFlags))
	{
		return;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	UMassStateTreeSubsystem* MassStateTreeSubsystem = World.GetSubsystem<UMassStateTreeSubsystem>();
	if (!MassStateTreeSubsystem && !BuildContext.IsInspectingData())
	{
		return;
	}

	if (!StateTree && !BuildContext.IsInspectingData())
	{
		return;
	}
	if (StateTree != nullptr && !BuildContext.IsInspectingData() && !StateTree->IsReadyToRun())
	{
		return;
	}

	FMassStateTreeSharedFragment SharedStateTree;
	SharedStateTree.StateTree = StateTree;

	const FConstSharedStruct StateTreeFragment = EntityManager.GetOrCreateConstSharedFragment(SharedStateTree);
	BuildContext.AddConstSharedFragment(StateTreeFragment);

	BuildContext.AddFragment<FMassStateTreeInstanceFragment>();
}

bool UArcAreaStateTreeTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	if (!BuildContext.HasFragment<FArcAreaFragment>())
	{
		UE_LOG(LogTemp, Error, TEXT("UArcAreaStateTreeTrait requires UArcAreaTrait on the same entity."));
		OutTraitRequirements.Add(FArcAreaFragment::StaticStruct());
		return false;
	}

	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(&World);
	if (!EnumHasAnyFlags(WorldExecutionFlags, EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server))
	{
		return true;
	}

	if (!StateTree)
	{
		UE_LOG(LogTemp, Error, TEXT("UArcAreaStateTreeTrait: StateTree asset is not set."));
		return false;
	}

	return true;
}
