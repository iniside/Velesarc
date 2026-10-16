// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Triggers/ArcTriggerWhile.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "GameplayTagsManager.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "StructUtils/SharedStruct.h"

namespace ArcTriggerWhilePrivate
{
	void Evaluate(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex)
	{
		FArcActiveEffect* Host = ArcTriggers::FindHostEffect(EntityManager, Owner, HostHandle);
		FArcEffectTask_TriggerWhile* Task = ArcTriggers::FindTask<FArcEffectTask_TriggerWhile>(Host, TaskIndex);
		if (!Task)
		{
			return;
		}

		const FSharedStruct HostSpecStruct = Host->Spec;
		const FArcEffectSpec* HostSpec = HostSpecStruct.GetPtr<FArcEffectSpec>();
		if (!HostSpec)
		{
			return;
		}

		FArcTriggerContext Context;
		Context.EntityManager = &EntityManager;
		Context.Owner = Owner;
		Context.HostSource = HostSpec->Context.SourceEntity;
		Context.HostContext = &HostSpec->Context;

		const bool bMet = ArcTriggers::AreConditionsMet(Task->Conditions, Context);
		if (bMet == Task->bActive)
		{
			return;
		}

		Task->bActive = bMet;
		const FString Description = ArcTriggers::DescribeTask(*Host, TaskIndex, TEXT("While"));
		if (bMet)
		{
			const TArray<FInstancedStruct> Effects = Task->Effects;
			FArcTriggerAppliedList NewApplied;
			ArcTriggers::FireEffects(Effects, Context, &NewApplied, nullptr);
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls conditions met: applied %d effects", *Description, NewApplied.Num());

			Task = ArcTriggers::FindTask<FArcEffectTask_TriggerWhile>(ArcTriggers::FindHostEffect(EntityManager, Owner, HostHandle), TaskIndex);
			if (!Task)
			{
				ArcTriggers::RemoveAppliedEffects(EntityManager, NewApplied);
				return;
			}
			Task->Applied.Append(NewApplied);
			return;
		}

		FArcTriggerAppliedList Removed = MoveTemp(Task->Applied);
		Task->Applied.Reset();
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls conditions unmet: removing %d effects", *Description, Removed.Num());
		ArcTriggers::RemoveAppliedEffects(EntityManager, Removed);
	}

	void DeferEvaluate(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex)
	{
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>([Owner, HostHandle, TaskIndex](FMassEntityManager& DeferredManager)
		{
			Evaluate(DeferredManager, Owner, HostHandle, TaskIndex);
		});
	}

	// Deferred, never inline: no effect stack change inside a bus callback.
	void DeferEvaluateInWorld(const TWeakObjectPtr<UWorld>& WeakWorld, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex)
	{
		if (FMassEntityManager* EntityManager = ArcTriggers::GetEntityManager(WeakWorld.Get()))
		{
			DeferEvaluate(*EntityManager, Owner, HostHandle, TaskIndex);
		}
	}

	// Requirements match a child tag through its parent, and tag edges are published per exact tag, so children are watched too.
	void GatherWatchedTags(const FArcGameplayTagRequirements& Requirements, TSet<FGameplayTag>& OutTags)
	{
		TArray<FGameplayTag> Named = Requirements.RequireTags.GetGameplayTagArray();
		Named.Append(Requirements.IgnoreTags.GetGameplayTagArray());
		Named.Append(Requirements.TagQuery.GetGameplayTagArray());

		const UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
		for (const FGameplayTag& Tag : Named)
		{
			OutTags.Add(Tag);
			for (const FGameplayTag& Child : TagsManager.RequestGameplayTagChildren(Tag))
			{
				OutTags.Add(Child);
			}
		}
	}
}

void FArcEffectTask_TriggerWhile::OnApplication(FArcActiveEffect& OwningEffect)
{
	bActive = false;
	Applied.Reset();

	UWorld* World = OwningEffect.OwnerWorld;
	UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
	FMassEntityManager* EntityManager = ArcTriggers::GetEntityManager(World);
	const int32 TaskIndex = ArcTriggers::FindTaskIndex(OwningEffect, this);
	if (!EventBus || !EntityManager || !OwningEffect.Handle.IsValid() || TaskIndex == INDEX_NONE)
	{
		return;
	}

	const FArcEffectSpec* Spec = OwningEffect.Spec.GetPtr<FArcEffectSpec>();
	FArcTriggerContext Parties;
	Parties.Owner = OwningEffect.OwnerEntity;
	Parties.HostSource = Spec ? Spec->Context.SourceEntity : FMassEntityHandle();

	const TWeakObjectPtr<UWorld> WeakWorld = World;
	const FMassEntityHandle Owner = OwningEffect.OwnerEntity;
	const FArcActiveEffectHandle HostHandle = OwningEffect.Handle;

	TSet<TPair<FMassEntityHandle, FGameplayTag>> WatchedTags;
	TSet<TPair<FMassEntityHandle, FArcAttributeRef>> WatchedAttributes;
	for (const FInstancedStruct& ConditionStruct : Conditions)
	{
		const FArcTriggerCondition* Condition = ConditionStruct.GetPtr<FArcTriggerCondition>();
		if (!Condition)
		{
			continue;
		}

		EArcTriggerParty TagParty = EArcTriggerParty::Owner;
		FArcGameplayTagRequirements Requirements;
		if (Condition->GetAsTagRequirements(TagParty, Requirements))
		{
			const FMassEntityHandle Entity = Parties.GetEntity(TagParty);
			if (Entity.IsValid())
			{
				TSet<FGameplayTag> Tags;
				ArcTriggerWhilePrivate::GatherWatchedTags(Requirements, Tags);
				for (const FGameplayTag& Tag : Tags)
				{
					WatchedTags.Add(TPair<FMassEntityHandle, FGameplayTag>(Entity, Tag));
				}
			}
		}

		EArcTriggerParty AttributeParty = EArcTriggerParty::Owner;
		TArray<FArcAttributeRef> Attributes;
		if (Condition->GetWatchedAttributes(AttributeParty, Attributes))
		{
			const FMassEntityHandle Entity = Parties.GetEntity(AttributeParty);
			if (Entity.IsValid())
			{
				for (const FArcAttributeRef& Attribute : Attributes)
				{
					WatchedAttributes.Add(TPair<FMassEntityHandle, FArcAttributeRef>(Entity, Attribute));
				}
			}
		}
	}

	const FArcGameEventDelegate Delegate = FArcGameEventDelegate::CreateLambda([WeakWorld, Owner, HostHandle, TaskIndex](const FArcGameEvent&)
	{
		ArcTriggerWhilePrivate::DeferEvaluateInWorld(WeakWorld, Owner, HostHandle, TaskIndex);
	});
	for (const TPair<FMassEntityHandle, FGameplayTag>& Watched : WatchedTags)
	{
		Subscriptions.Add(EventBus->Subscribe(ArcGameEventTags::TagAdded.GetTag(), Watched.Key, Watched.Value, Delegate));
		Subscriptions.Add(EventBus->Subscribe(ArcGameEventTags::TagRemoved.GetTag(), Watched.Key, Watched.Value, Delegate));
	}
	for (const TPair<FMassEntityHandle, FArcAttributeRef>& Watched : WatchedAttributes)
	{
		Subscriptions.Add(EventBus->Subscribe(Watched.Key, Watched.Value, Delegate));
	}

	ArcTriggerWhilePrivate::DeferEvaluate(*EntityManager, Owner, HostHandle, TaskIndex);
}

void FArcEffectTask_TriggerWhile::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	UWorld* World = OwningEffect.OwnerWorld;
	UE_LOGF(LogArcMassMechanics, Verbose, "Trigger While owner %ls removed (%ls): tearing down, removing %d effects",
		*OwningEffect.OwnerEntity.DebugGetDescription(), *UEnum::GetValueAsString(Reason), Applied.Num());

	if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
	{
		for (const FDelegateHandle& Handle : Subscriptions)
		{
			EventBus->Unsubscribe(Handle);
		}
	}
	Subscriptions.Reset();

	bActive = false;
	ArcTriggers::DeferRemoveAppliedEffects(World, MoveTemp(Applied));
	Applied.Reset();
}
