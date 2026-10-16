// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Triggers/ArcTriggerOn.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "ArcMassMechanics/Combat/ArcHitTypes.h"
#include "Effects/ArcEffectSpec.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/EntityManager.h"
#include "StructUtils/SharedStruct.h"

namespace ArcTriggerOnPrivate
{
	const UArcEffectDefinition* GetSpecDefinition(const FSharedStruct& SpecStruct)
	{
		const FArcEffectSpec* Spec = SpecStruct.GetPtr<FArcEffectSpec>();
		return Spec ? Spec->Definition.Get() : nullptr;
	}

	const UArcEffectDefinition* FindEventEffect(const FArcGameEvent& Event)
	{
		if (const FArcEffectAppliedPayload* Applied = Event.Payload.GetPtr<FArcEffectAppliedPayload>())
		{
			return GetSpecDefinition(Applied->Spec);
		}
		if (const FArcEffectExecutedPayload* Executed = Event.Payload.GetPtr<FArcEffectExecutedPayload>())
		{
			return GetSpecDefinition(Executed->Spec);
		}
		if (const FArcEffectRemovedPayload* Removed = Event.Payload.GetPtr<FArcEffectRemovedPayload>())
		{
			return GetSpecDefinition(Removed->Spec);
		}
		if (const FArcEffectRejectedPayload* Rejected = Event.Payload.GetPtr<FArcEffectRejectedPayload>())
		{
			return GetSpecDefinition(Rejected->Spec);
		}
		if (const FArcHitResolvedPayload* Hit = Event.Payload.GetPtr<FArcHitResolvedPayload>())
		{
			return Hit->Definition.Get();
		}
		if (const FArcDamageDealtPayload* Damage = Event.Payload.GetPtr<FArcDamageDealtPayload>())
		{
			return Damage->Definition.Get();
		}
		return nullptr;
	}

	const UArcAbilityDefinition* FindEventAbility(const FArcGameEvent& Event)
	{
		if (const FArcAbilityActivatedPayload* Activated = Event.Payload.GetPtr<FArcAbilityActivatedPayload>())
		{
			return Activated->Definition.Get();
		}
		if (const FArcAbilityEndedPayload* Ended = Event.Payload.GetPtr<FArcAbilityEndedPayload>())
		{
			return Ended->Definition.Get();
		}
		if (const FArcAbilityCostFailedPayload* CostFailed = Event.Payload.GetPtr<FArcAbilityCostFailedPayload>())
		{
			return CostFailed->Definition.Get();
		}
		if (const FArcAbilityCooldownFailedPayload* CooldownFailed = Event.Payload.GetPtr<FArcAbilityCooldownFailedPayload>())
		{
			return CooldownFailed->Definition.Get();
		}
		if (const FArcAbilityCooldownExpiredPayload* CooldownExpired = Event.Payload.GetPtr<FArcAbilityCooldownExpiredPayload>())
		{
			return CooldownExpired->Definition.Get();
		}
		return nullptr;
	}

	void HandleEvent(const TWeakObjectPtr<UWorld>& WeakWorld, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex,
		const FArcGameEvent& Event)
	{
		FMassEntityManager* EntityManager = ArcTriggers::GetEntityManager(WeakWorld.Get());
		if (!EntityManager)
		{
			return;
		}

		FArcActiveEffect* Host = ArcTriggers::FindHostEffect(*EntityManager, Owner, HostHandle);
		FArcEffectTask_TriggerOn* Task = ArcTriggers::FindTask<FArcEffectTask_TriggerOn>(Host, TaskIndex);
		if (!Task)
		{
			return;
		}

		const FString Description = ArcTriggers::DescribeTask(*Host, TaskIndex, TEXT("On"));
		if (ArcTriggers::IsTriggerOriginated(Event.SourceTags))
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped: event %ls is trigger-originated", *Description, *Event.Tag.ToString());
			return;
		}

		const FSharedStruct HostSpecStruct = Host->Spec;
		const FArcEffectSpec* HostSpec = HostSpecStruct.GetPtr<FArcEffectSpec>();
		if (!HostSpec)
		{
			return;
		}

		FArcTriggerContext Context;
		Context.EntityManager = EntityManager;
		Context.Owner = Owner;
		Context.HostSource = HostSpec->Context.SourceEntity;
		Context.HostContext = &HostSpec->Context;
		Context.Event = &Event;
		const bool bOwnerIsInstigator = Event.Instigator == Owner;
		Context.Other = bOwnerIsInstigator ? Event.Target : Event.Instigator;
		Context.OwnerTags = bOwnerIsInstigator ? &Event.SourceTags : &Event.TargetTags;
		Context.OtherTags = bOwnerIsInstigator ? &Event.TargetTags : &Event.SourceTags;
		Context.EventEffect = FindEventEffect(Event);
		Context.Ability = FindEventAbility(Event);

		if (Context.EventEffect && Context.EventEffect == HostSpec->Definition.Get())
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped: event %ls comes from its own host effect", *Description, *Event.Tag.ToString());
			return;
		}

		if (const FArcHitResolvedPayload* Hit = Event.Payload.GetPtr<FArcHitResolvedPayload>())
		{
			Context.bHasHit = Hit->Outcome == EArcHitOutcome::Hit;
			Context.bCrit = Hit->bCrit;
			Context.Zone = Hit->Zone;
			Context.DamageType = Hit->DamageType;
		}
		else if (const FArcDamageDealtPayload* Damage = Event.Payload.GetPtr<FArcDamageDealtPayload>())
		{
			Context.bHasHit = true;
			Context.bCrit = Damage->bCrit;
			Context.Zone = Damage->Zone;
			Context.DamageType = Damage->DamageType;
		}

		if (!ArcTriggers::AreConditionsMet(Task->Conditions, Context))
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped: conditions unmet (event %ls, other %ls)",
				*Description, *Event.Tag.ToString(), *Context.Other.DebugGetDescription());
			return;
		}

		float Roll = 0.f;
		if (Task->Chance < 1.f)
		{
			Roll = FMath::FRand();
			if (Roll >= Task->Chance)
			{
				UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped: chance roll %.4f >= chance %.4f (event %ls)",
					*Description, Roll, Task->Chance, *Event.Tag.ToString());
				return;
			}
		}

		const ArcTriggers::FChargeOutcome Outcome = ArcTriggers::TryConsumeCharge(*EntityManager, Owner, HostHandle, Task->ChargeMode,
			Task->RemainingCharges, Task->PendingHostStackConsumes, Description);
		if (Outcome.Result == ArcTriggers::EChargeResult::Spent)
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped: charges spent (event %ls)", *Description, *Event.Tag.ToString());
			return;
		}

		if (!Task->RemoveWithHost.IsEmpty())
		{
			ArcTriggers::PruneAppliedEffects(*EntityManager, Task->RemoveWithHost);
		}
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls fired: event %ls, other %ls, roll %.4f chance %.4f, charge %ls",
			*Description, *Event.Tag.ToString(), *Context.Other.DebugGetDescription(), Roll, Task->Chance,
			ArcTriggers::GetChargeResultName(Outcome.Result));

		const EArcTriggerChargeMode ChargeMode = Task->ChargeMode;
		const bool bRemoveHostWhenChargesSpent = Task->bRemoveHostWhenChargesSpent;
		const TArray<FInstancedStruct> Effects = Task->Effects;
		FArcTriggerAppliedList Applied;
		ArcTriggers::FireEffects(Effects, Context, nullptr, &Applied);

		Task = ArcTriggers::FindTask<FArcEffectTask_TriggerOn>(ArcTriggers::FindHostEffect(*EntityManager, Owner, HostHandle), TaskIndex);
		if (!Task)
		{
			ArcTriggers::RemoveAppliedEffects(*EntityManager, Applied);
			return;
		}
		Task->RemoveWithHost.Append(Applied);
		if (ChargeMode == EArcTriggerChargeMode::HostStack && Outcome.Result == ArcTriggers::EChargeResult::Consumed)
		{
			Task->PendingHostStackConsumes = FMath::Max(0, Task->PendingHostStackConsumes - 1);
		}
		ArcTriggers::PayCharge(*EntityManager, Owner, HostHandle, ChargeMode, Outcome, bRemoveHostWhenChargesSpent, Description);
	}
}

void FArcEffectTask_TriggerOn::OnApplication(FArcActiveEffect& OwningEffect)
{
	RemainingCharges = ArcTriggers::GetInitialCharges(ChargeMode, Charges);
	PendingHostStackConsumes = 0;

	UWorld* World = OwningEffect.OwnerWorld;
	UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr;
	const int32 TaskIndex = ArcTriggers::FindTaskIndex(OwningEffect, this);
	if (!EventBus || !OwningEffect.Handle.IsValid() || !EventTag.IsValid() || TaskIndex == INDEX_NONE)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WeakWorld = World;
	const FMassEntityHandle Owner = OwningEffect.OwnerEntity;
	const FArcActiveEffectHandle HostHandle = OwningEffect.Handle;
	FArcGameEventDelegate Delegate = FArcGameEventDelegate::CreateLambda([WeakWorld, Owner, HostHandle, TaskIndex](const FArcGameEvent& Event)
	{
		ArcTriggerOnPrivate::HandleEvent(WeakWorld, Owner, HostHandle, TaskIndex, Event);
	});

	if (ArcTriggers::IsTagChangeChannel(EventTag))
	{
		if (Tag.IsValid())
		{
			Subscription = EventBus->Subscribe(EventTag, Owner, Tag, MoveTemp(Delegate));
		}
		return;
	}
	Subscription = EventBus->Subscribe(EventTag, Owner, Role, MoveTemp(Delegate));
}

void FArcEffectTask_TriggerOn::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	UWorld* World = OwningEffect.OwnerWorld;
	UE_LOGF(LogArcMassMechanics, Verbose, "Trigger On owner %ls removed (%ls): tearing down",
		*OwningEffect.OwnerEntity.DebugGetDescription(), *UEnum::GetValueAsString(Reason));

	if (UArcGameEventSubsystem* EventBus = World ? World->GetSubsystem<UArcGameEventSubsystem>() : nullptr)
	{
		if (Subscription.IsValid())
		{
			EventBus->Unsubscribe(Subscription);
		}
	}
	Subscription.Reset();

	ArcTriggers::DeferRemoveAppliedEffects(World, MoveTemp(RemoveWithHost));
	RemoveWithHost.Reset();
}
