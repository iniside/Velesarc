// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAbilitiesTrait.h"
#include "Traits/ArcDefaultEffectsObserver.h"
#include "Traits/ArcAbilityGrantTrait.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"

void UArcAbilitiesTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAbilityCollectionFragment>();
	BuildContext.AddFragment<FArcAbilityInputFragment>();
	BuildContext.AddFragment<FArcAbilityCooldownFragment>();
	BuildContext.AddFragment<FArcEffectStackFragment>();
	BuildContext.AddFragment<FArcAggregatorFragment>();
	BuildContext.AddFragment<FArcOwnedTagsFragment>();
	BuildContext.AddFragment<FArcImmunityFragment>();
	BuildContext.AddFragment_GetRef<FArcPendingAttributeOpsFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	if (HandlerConfig)
	{
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct SharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
		BuildContext.AddConstSharedFragment(SharedFrag);
	}

	for (const FInstancedStruct& AttrFragment : AttributeFragments)
	{
		if (AttrFragment.IsValid())
		{
			BuildContext.AddFragment(AttrFragment);
		}
	}

	if (AbilitySets.Num() > 0)
	{
		FArcAbilityGrantSharedFragment GrantConfig;
		GrantConfig.AbilitySets = AbilitySets;
		const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(GrantConfig);
		BuildContext.AddConstSharedFragment(SharedFragment);
	}

	if (DefaultEffects.Num() > 0)
	{
		FArcDefaultEffectsSharedFragment DefaultEffectsConfig;
		DefaultEffectsConfig.DefaultEffects = DefaultEffects;
		const FConstSharedStruct SharedFragment = EntityManager.GetOrCreateConstSharedFragment(DefaultEffectsConfig);
		BuildContext.AddConstSharedFragment(SharedFragment);
	}
}
