// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/EntityFragments.h"
#include "StructUtils/SharedStruct.h"
#include "Engine/World.h"

namespace ArcEffectTaskHelpersImpl
{

UArcGameEventSubsystem* GetEventBus(const FArcActiveEffect& OwningEffect)
{
	return OwningEffect.OwnerWorld ? OwningEffect.OwnerWorld->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
}

FMassEntityManager* GetEntityManager(const TWeakObjectPtr<UWorld>& WeakWorld)
{
	UWorld* World = WeakWorld.Get();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
}

int32 FindTaskIndex(const FArcActiveEffect& OwningEffect, const FArcEffectTask* Task)
{
	for (int32 Index = 0; Index < OwningEffect.Tasks.Num(); ++Index)
	{
		if (OwningEffect.Tasks[Index].GetPtr<FArcEffectTask>() == Task)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

template<typename TaskType>
const TaskType* FindTask(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle Handle, int32 TaskIndex,
	FArcActiveEffect*& OutEffect)
{
	OutEffect = nullptr;
	if (!EntityManager.IsEntityValid(Owner))
	{
		return nullptr;
	}
	FArcEffectStackFragment* Stack = FMassEntityView(EntityManager, Owner).GetFragmentDataPtr<FArcEffectStackFragment>();
	OutEffect = Stack ? ArcEffects::FindActiveEffect(*Stack, Handle) : nullptr;
	if (!OutEffect || !OutEffect->Tasks.IsValidIndex(TaskIndex))
	{
		return nullptr;
	}
	return OutEffect->Tasks[TaskIndex].GetPtr<TaskType>();
}

FArcEffectContext GetChildContext(const FArcActiveEffect& OwningEffect)
{
	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	return Spec ? ArcEffects::MakeChildContext(Spec->Context) : FArcEffectContext();
}

void ApplyChild(FMassEntityManager& EntityManager, FMassEntityHandle Target, UArcEffectDefinition* Effect, const FArcEffectContext& ChildContext)
{
	FArcEffectApplyRequest Request;
	Request.Definition = Effect;
	Request.Context = ChildContext;
	ArcEffects::TryApplyEffect(EntityManager, Target, Request);
}

template<typename TaskType>
void ApplyFromTask(const TWeakObjectPtr<UWorld>& WeakWorld, FMassEntityHandle Owner, FArcActiveEffectHandle Handle, int32 TaskIndex,
	TFunctionRef<bool(const TaskType&)> Filter)
{
	FMassEntityManager* EntityManager = GetEntityManager(WeakWorld);
	if (!EntityManager)
	{
		return;
	}

	FArcActiveEffect* OwningEffect = nullptr;
	const TaskType* Task = FindTask<TaskType>(*EntityManager, Owner, Handle, TaskIndex, OwningEffect);
	if (!Task || !Task->EffectToApply || !Filter(*Task))
	{
		return;
	}

	UArcEffectDefinition* Effect = Task->EffectToApply;
	const FArcEffectContext ChildContext = GetChildContext(*OwningEffect);
	ApplyChild(*EntityManager, Owner, Effect, ChildContext);
}

void Unsubscribe(const FArcActiveEffect& OwningEffect, FDelegateHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return;
	}
	if (UArcGameEventSubsystem* EventBus = GetEventBus(OwningEffect))
	{
		EventBus->Unsubscribe(Handle);
	}
	Handle.Reset();
}

} // namespace ArcEffectTaskHelpersImpl

// --- FArcEffectTask_OnAttributeChanged ---

void FArcEffectTask_OnAttributeChanged::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcGameEventSubsystem* EventBus = ArcEffectTaskHelpersImpl::GetEventBus(OwningEffect);
	const FMassEntityHandle Owner = OwningEffect.OwnerEntity;
	const int32 TaskIndex = ArcEffectTaskHelpersImpl::FindTaskIndex(OwningEffect, this);
	if (!EventBus || !Owner.IsValid() || !OwningEffect.Handle.IsValid() || TaskIndex == INDEX_NONE)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	const FArcActiveEffectHandle Handle = OwningEffect.Handle;

	DelegateHandle = EventBus->Subscribe(Owner, Attribute,
		FArcGameEventDelegate::CreateLambda([WeakWorld, Owner, Handle, TaskIndex](const FArcGameEvent&)
		{
			ArcEffectTaskHelpersImpl::ApplyFromTask<FArcEffectTask_OnAttributeChanged>(WeakWorld, Owner, Handle, TaskIndex,
				[](const FArcEffectTask_OnAttributeChanged&) -> bool { return true; });
		}));
}

void FArcEffectTask_OnAttributeChanged::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	ArcEffectTaskHelpersImpl::Unsubscribe(OwningEffect, DelegateHandle);
}

// --- FArcEffectTask_OnAbilityActivated ---

void FArcEffectTask_OnAbilityActivated::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcGameEventSubsystem* EventBus = ArcEffectTaskHelpersImpl::GetEventBus(OwningEffect);
	const FMassEntityHandle Owner = OwningEffect.OwnerEntity;
	const int32 TaskIndex = ArcEffectTaskHelpersImpl::FindTaskIndex(OwningEffect, this);
	if (!EventBus || !Owner.IsValid() || !OwningEffect.Handle.IsValid() || TaskIndex == INDEX_NONE)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	const FArcActiveEffectHandle Handle = OwningEffect.Handle;

	DelegateHandle = EventBus->Subscribe(ArcGameEventTags::AbilityActivated, Owner, EArcEventRole::Target,
		FArcGameEventDelegate::CreateLambda([WeakWorld, Owner, Handle, TaskIndex](const FArcGameEvent& Event)
		{
			const FArcAbilityActivatedPayload* Activated = Event.Payload.GetPtr<FArcAbilityActivatedPayload>();
			const UArcAbilityDefinition* Ability = Activated ? Activated->Definition.Get() : nullptr;
			ArcEffectTaskHelpersImpl::ApplyFromTask<FArcEffectTask_OnAbilityActivated>(WeakWorld, Owner, Handle, TaskIndex,
				[Ability](const FArcEffectTask_OnAbilityActivated& Task) -> bool
				{
					return !Task.AbilityFilter || Ability == Task.AbilityFilter.Get();
				});
		}));
}

void FArcEffectTask_OnAbilityActivated::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	ArcEffectTaskHelpersImpl::Unsubscribe(OwningEffect, DelegateHandle);
}

// --- FArcEffectTask_OnTagChanged ---

void FArcEffectTask_OnTagChanged::OnApplication(FArcActiveEffect& OwningEffect)
{
	UArcGameEventSubsystem* EventBus = ArcEffectTaskHelpersImpl::GetEventBus(OwningEffect);
	const FMassEntityHandle Owner = OwningEffect.OwnerEntity;
	const int32 TaskIndex = ArcEffectTaskHelpersImpl::FindTaskIndex(OwningEffect, this);
	if (!EventBus || !Owner.IsValid() || !OwningEffect.Handle.IsValid() || TaskIndex == INDEX_NONE || !RequiredTag.IsValid() || !EffectToApply)
	{
		return;
	}

	FMassEntityHandle Watched = Owner;
	if (Party == EArcTagChangeParty::Source)
	{
		const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
		Watched = Spec ? Spec->Context.SourceEntity : FMassEntityHandle();
		if (!Watched.IsValid())
		{
			return;
		}
	}

	const TWeakObjectPtr<UWorld> WeakWorld = OwningEffect.OwnerWorld;
	const FArcActiveEffectHandle Handle = OwningEffect.Handle;
	const FArcGameEventDelegate Delegate = FArcGameEventDelegate::CreateLambda([WeakWorld, Owner, Handle, TaskIndex](const FArcGameEvent&)
	{
		ArcEffectTaskHelpersImpl::ApplyFromTask<FArcEffectTask_OnTagChanged>(WeakWorld, Owner, Handle, TaskIndex,
			[](const FArcEffectTask_OnTagChanged&) -> bool { return true; });
	});

	if (When != EArcTagChangeTrigger::Removed)
	{
		AddedHandle = EventBus->Subscribe(ArcGameEventTags::TagAdded, Watched, RequiredTag, Delegate);
	}
	if (When != EArcTagChangeTrigger::Added)
	{
		RemovedHandle = EventBus->Subscribe(ArcGameEventTags::TagRemoved, Watched, RequiredTag, Delegate);
	}
}

void FArcEffectTask_OnTagChanged::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	ArcEffectTaskHelpersImpl::Unsubscribe(OwningEffect, AddedHandle);
	ArcEffectTaskHelpersImpl::Unsubscribe(OwningEffect, RemovedHandle);
}
