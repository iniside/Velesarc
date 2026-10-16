// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAbilitiesTrait.h"
#include "ArcMassAbilitiesLogs.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInitialAttributesPreset.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Fragments/ArcInitialAttributesPresetSharedFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"

void UArcAbilitiesTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FArcAbilityCooldownFragment>();

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	if (HandlerConfig)
	{
		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		const FConstSharedStruct SharedFrag = EntityManager.GetOrCreateConstSharedFragment(SharedHandler);
		BuildContext.AddConstSharedFragment(SharedFrag);
	}

	if (InitialAttributesPreset)
	{
		FArcInitialAttributesPresetSharedFragment PresetShared;
		PresetShared.Preset = InitialAttributesPreset;
		const FConstSharedStruct SharedFrag = EntityManager.GetOrCreateConstSharedFragment(PresetShared);
		BuildContext.AddConstSharedFragment(SharedFrag);
	}
	else
	{
		UE_LOGF(LogArcMassAbilities, Display,"UArcAbilitiesTrait::BuildTemplate: InitialAttributesPreset is null — initial-attributes observer will not seed BaseValues for this entity.");
	}
}

void UArcAbilitiesTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcAbilityCollectionFragment>();
	Out.AddSparseFragment<FArcAbilityInputFragment>();

	Out.AddSparseFragment<FArcImmunityFragment>();

	if (DefaultTags.IsEmpty())
	{
		Out.AddSparseFragment<FArcOwnedTagsFragment>();
	}
	else
	{
		FArcOwnedTagsFragment OwnedTags;
		for (const FGameplayTag& Tag : DefaultTags)
		{
			OwnedTags.Tags.AddTag(Tag);
		}
		Out.AddSparseFragment(FConstStructView::Make(OwnedTags));
	}
}

bool UArcAbilitiesTrait::ValidateTemplate(const FMassEntityTemplateBuildContext& BuildContext, const UWorld& World, FAdditionalTraitRequirements& OutTraitRequirements) const
{
	// UArcAttributesTrait owns the effect stack, aggregator and pending attribute ops.
	OutTraitRequirements.Add(FArcEffectStackFragment::StaticStruct());
	OutTraitRequirements.Add(FArcAggregatorFragment::StaticStruct());
	return Super::ValidateTemplate(BuildContext, World, OutTraitRequirements);
}

void UArcAbilitiesTrait::InitializeEntity(FMassEntityManager& Mgr, FMassEntityHandle Entity, const UWorld& World) const
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
