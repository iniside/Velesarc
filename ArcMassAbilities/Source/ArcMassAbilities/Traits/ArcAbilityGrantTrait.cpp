// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAbilityGrantTrait.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcAbilityGrantTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAbilityCollectionFragment>();
	BuildContext.AddFragment<FArcAbilityInputFragment>();

	if (AbilitySets.Num() > 0)
	{
		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

		FArcAbilityGrantSharedFragment GrantConfig;
		GrantConfig.AbilitySets = AbilitySets;
		const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(GrantConfig);
		BuildContext.AddConstSharedFragment(SharedFragment);
	}
}
