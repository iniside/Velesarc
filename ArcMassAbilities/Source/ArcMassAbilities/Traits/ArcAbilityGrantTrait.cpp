// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAbilityGrantTrait.h"
#include "ArcMassAbilitiesLogs.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassEntityTemplateRegistry.h"

void UArcAbilityGrantTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
}

void UArcAbilityGrantTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcAbilityCollectionFragment>();
	Out.AddSparseFragment<FArcAbilityInputFragment>();
}

void UArcAbilityGrantTrait::InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const
{
	int32 TotalGranted = 0;
	for (const UArcMassAbilitySet* AbilitySet : AbilitySets)
	{
		if (!AbilitySet)
		{
			ARCMA_VLOG(&World, Warning,
				TEXT("[Ability] Null AbilitySet in shared fragment entity=%s"),
				*Entity.DebugGetDescription());
			continue;
		}

		const TArray<FArcAbilityHandle> Handles =
			ArcAbilities::GrantAbilitySet(Mgr, Entity, AbilitySet, const_cast<UWorld*>(&World));
		TotalGranted += Handles.Num();
	}

	ARCMA_VLOG(&World, Log,
		TEXT("[Ability] Grant complete granted=%d entity=%s"),
		TotalGranted,
		*Entity.DebugGetDescription());
}
