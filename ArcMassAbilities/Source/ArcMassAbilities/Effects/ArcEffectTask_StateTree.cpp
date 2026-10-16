// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectTask_StateTree.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "MassEntitySubsystem.h"
#include "StructUtils/SharedStruct.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEffectTask_StateTree)

void FArcEffectTask_StateTree::OnApplication(FArcActiveEffect& OwningEffect)
{
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	if (!StateTree)
	{
		return;
	}

	UMassEntitySubsystem* MES = OwningEffect.OwnerWorld ? OwningEffect.OwnerWorld->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!MES)
	{
		return;
	}

	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	FMassEntityHandle SourceEntity = Spec ? Spec->Context.SourceEntity : FMassEntityHandle();
	UArcEffectDefinition* EffectDefinition = Spec ? Spec->Definition : nullptr;

	TreeContext.SetOwnerEntity(OwningEffect.OwnerEntity);
	TreeContext.SetSourceEntity(SourceEntity);
	TreeContext.SetEffectDefinition(EffectDefinition);
	TreeContext.SetWorld(OwningEffect.OwnerWorld);

	TreeContext.Activate(StateTree, MES);
}

void FArcEffectTask_StateTree::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	TreeContext.Deactivate();
}

void FArcEffectTask_StateTree::OnExecute(FArcActiveEffect& OwningEffect)
{
	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	const UArcEffectDefinition* EffectDefinition = Spec ? Spec->Definition : nullptr;
	if (EffectDefinition == nullptr || EffectDefinition->StackingPolicy.DurationType != EArcEffectDuration::Instant)
	{
		return;
	}

	OnApplication(OwningEffect);
	if (TreeContext.IsRunning())
	{
		TreeContext.Tick(0.f);
	}
	TreeContext.Deactivate();
}
