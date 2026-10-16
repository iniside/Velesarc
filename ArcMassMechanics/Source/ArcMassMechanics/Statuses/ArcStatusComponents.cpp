// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Statuses/ArcStatusComponents.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"

void FArcEffectComp_InterruptAbilities::OnApplied(const FArcEffectContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.TargetEntity))
	{
		return;
	}

	FMassEntityManager& EntityManager = *Context.EntityManager;
	UArcAbilityStateTreeSubsystem* Subsystem = UWorld::GetSubsystem<UArcAbilityStateTreeSubsystem>(EntityManager.GetWorld());
	if (!Subsystem)
	{
		return;
	}

	TArray<FArcAbilityHandle, TInlineAllocator<4>> ToInterrupt;
	{
		FMassEntityView EntityView(EntityManager, Context.TargetEntity);
		const FArcAbilityCollectionFragment* Collection = EntityView.GetSparseFragmentDataPtr<FArcAbilityCollectionFragment>();
		if (!Collection)
		{
			return;
		}

		const bool bSelfApplied = Context.SourceEntity == Context.TargetEntity;
		for (const FArcActiveAbility& Ability : Collection->Abilities)
		{
			if (!Ability.bIsActive)
			{
				continue;
			}
			if (bSelfApplied && Ability.Handle == Context.SourceAbilityHandle)
			{
				continue;
			}
			if (Ability.Definition && Abilities.RequirementsMet(Ability.Definition->AbilityTags))
			{
				ToInterrupt.Add(Ability.Handle);
			}
		}
	}

	for (const FArcAbilityHandle Handle : ToInterrupt)
	{
		ArcAbilities::CancelAbility(EntityManager, Context.TargetEntity, Handle, *Subsystem, EArcAbilityEndReason::Interrupted, Context.SourceEntity);
	}
}
