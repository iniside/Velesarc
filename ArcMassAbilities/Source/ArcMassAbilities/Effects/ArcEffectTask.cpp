// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "MassEntitySubsystem.h"
#include "StructUtils/SharedStruct.h"

namespace ArcEffectTaskHelpers
{

UArcEffectEventSubsystem* GetEventSubsystem(FArcActiveEffect& OwningEffect)
{
	if (OwningEffect.OwnerWorld)
	{
		return OwningEffect.OwnerWorld->GetSubsystem<UArcEffectEventSubsystem>();
	}
	return nullptr;
}

FMassEntityHandle GetOwnerEntity(FArcActiveEffect& OwningEffect)
{
	return OwningEffect.OwnerEntity;
}

FMassEntityManager* GetEntityManager(FArcActiveEffect& OwningEffect)
{
	if (OwningEffect.OwnerWorld)
	{
		UMassEntitySubsystem* MES = OwningEffect.OwnerWorld->GetSubsystem<UMassEntitySubsystem>();
		if (MES)
		{
			return &MES->GetMutableEntityManager();
		}
	}
	return nullptr;
}

FMassEntityHandle GetSourceEntity(FArcActiveEffect& OwningEffect)
{
	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	return Spec ? Spec->Context.SourceEntity : FMassEntityHandle();
}

UArcEffectEventSubsystem* GetEventSubsystem(TWeakObjectPtr<UWorld> WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	if (World)
	{
		return World->GetSubsystem<UArcEffectEventSubsystem>();
	}
	return nullptr;
}

FMassEntityManager* GetEntityManager(TWeakObjectPtr<UWorld> WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	if (World)
	{
		UMassEntitySubsystem* MES = World->GetSubsystem<UMassEntitySubsystem>();
		if (MES)
		{
			return &MES->GetMutableEntityManager();
		}
	}
	return nullptr;
}

} // namespace ArcEffectTaskHelpers

// --- FArcEffectTask_OnAttributeChanged ---

void FArcEffectTask_OnAttributeChanged::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (!EventSys || !Owner.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	FMassEntityHandle SourceEntity = ArcEffectTaskHelpers::GetSourceEntity(OwningEffect);

	DelegateHandle = EventSys->SubscribeAttributeChanged(Owner, Attribute,
		FArcOnAttributeChanged::FDelegate::CreateLambda(
			[this, WeakWorld, SourceEntity](FMassEntityHandle Entity, float OldValue, float NewValue)
			{
				if (!EffectToApply)
				{
					return;
				}
				FMassEntityManager* EM = ArcEffectTaskHelpers::GetEntityManager(WeakWorld);
				if (EM)
				{
					ArcEffects::TryApplyEffect(*EM, Entity, EffectToApply, SourceEntity);
				}
			}));
}

void FArcEffectTask_OnAttributeChanged::OnRemoved(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (EventSys && DelegateHandle.IsValid())
	{
		EventSys->UnsubscribeAttributeChanged(Owner, Attribute, DelegateHandle);
		DelegateHandle.Reset();
	}
}

// --- FArcEffectTask_OnAbilityActivated ---

void FArcEffectTask_OnAbilityActivated::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (!EventSys || !Owner.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	FMassEntityHandle SourceEntity = ArcEffectTaskHelpers::GetSourceEntity(OwningEffect);

	DelegateHandle = EventSys->SubscribeAbilityActivated(Owner,
		FArcOnAbilityActivated::FDelegate::CreateLambda(
			[this, WeakWorld, SourceEntity](FMassEntityHandle Entity, UArcAbilityDefinition* Ability)
			{
				if (AbilityFilter && Ability != AbilityFilter)
				{
					return;
				}
				if (!EffectToApply)
				{
					return;
				}
				FMassEntityManager* EM = ArcEffectTaskHelpers::GetEntityManager(WeakWorld);
				if (EM)
				{
					ArcEffects::TryApplyEffect(*EM, Entity, EffectToApply, SourceEntity);
				}
			}));
}

void FArcEffectTask_OnAbilityActivated::OnRemoved(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (EventSys && DelegateHandle.IsValid())
	{
		EventSys->UnsubscribeAbilityActivated(Owner, DelegateHandle);
		DelegateHandle.Reset();
	}
}

// --- FArcEffectTask_OnEffectApplied ---

void FArcEffectTask_OnEffectApplied::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (!EventSys || !Owner.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	FMassEntityHandle SourceEntity = ArcEffectTaskHelpers::GetSourceEntity(OwningEffect);

	DelegateHandle = EventSys->SubscribeEffectApplied(Owner,
		FArcOnEffectApplied::FDelegate::CreateLambda(
			[this, WeakWorld, SourceEntity](FMassEntityHandle Entity, UArcEffectDefinition* Effect)
			{
				if (!EffectToApply || EffectToApply == Effect)
				{
					return;
				}
				FGameplayTagContainer EffectTags = ArcEffectHelpers::GetEffectTags(Effect);
				if (!EffectTagFilter.RequirementsMet(EffectTags))
				{
					return;
				}
				FMassEntityManager* EM = ArcEffectTaskHelpers::GetEntityManager(WeakWorld);
				if (EM)
				{
					ArcEffects::TryApplyEffect(*EM, Entity, EffectToApply, SourceEntity);
				}
			}));
}

void FArcEffectTask_OnEffectApplied::OnRemoved(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (EventSys && DelegateHandle.IsValid())
	{
		EventSys->UnsubscribeEffectApplied(Owner, DelegateHandle);
		DelegateHandle.Reset();
	}
}

// --- FArcEffectTask_OnEffectExecuted ---

void FArcEffectTask_OnEffectExecuted::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (!EventSys || !Owner.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	FMassEntityHandle SourceEntity = ArcEffectTaskHelpers::GetSourceEntity(OwningEffect);

	DelegateHandle = EventSys->SubscribeEffectExecuted(Owner,
		FArcOnEffectExecuted::FDelegate::CreateLambda(
			[this, WeakWorld, SourceEntity](FMassEntityHandle Entity, UArcEffectDefinition* Effect)
			{
				if (!EffectToApply || EffectToApply == Effect)
				{
					return;
				}
				FGameplayTagContainer EffectTags = ArcEffectHelpers::GetEffectTags(Effect);
				if (!EffectTagFilter.RequirementsMet(EffectTags))
				{
					return;
				}
				FMassEntityManager* EM = ArcEffectTaskHelpers::GetEntityManager(WeakWorld);
				if (EM)
				{
					ArcEffects::TryApplyEffect(*EM, Entity, EffectToApply, SourceEntity);
				}
			}));
}

void FArcEffectTask_OnEffectExecuted::OnRemoved(FArcActiveEffect& OwningEffect)
{
	UArcEffectEventSubsystem* EventSys = ArcEffectTaskHelpers::GetEventSubsystem(OwningEffect);
	FMassEntityHandle Owner = ArcEffectTaskHelpers::GetOwnerEntity(OwningEffect);
	if (EventSys && DelegateHandle.IsValid())
	{
		EventSys->UnsubscribeEffectExecuted(Owner, DelegateHandle);
		DelegateHandle.Reset();
	}
}
