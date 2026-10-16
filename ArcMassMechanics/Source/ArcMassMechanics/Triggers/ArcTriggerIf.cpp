// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Triggers/ArcTriggerIf.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "StructUtils/SharedStruct.h"

namespace ArcTriggerIfPrivate
{
	using ArcTriggers::EChargeResult;
	using ArcTriggers::FChargeOutcome;

	const TCHAR* Kind = TEXT("If");

	FString GetHitName(const ArcTriggers::FArcIfHit& Hit)
	{
		return GetNameSafe(Hit.Spec ? Hit.Spec->Definition.Get() : nullptr);
	}

	bool IsMultiplierChannel(EArcIfChannel Channel)
	{
		return Channel == EArcIfChannel::DamageDoneMultiplier || Channel == EArcIfChannel::DamageTakenMultiplier;
	}

	bool IsDoneChannel(EArcIfChannel Channel)
	{
		return Channel != EArcIfChannel::DamageTakenMultiplier && Channel != EArcIfChannel::DamageTakenFlat;
	}

	bool AllowsSide(EArcEventRole Role, bool bAsSource)
	{
		return bAsSource ? Role != EArcEventRole::Target : Role != EArcEventRole::Instigator;
	}

	bool IsHostOfHit(const FArcEffectSpec& HostSpec, const ArcTriggers::FArcIfHit& Hit)
	{
		const UArcEffectDefinition* HitDefinition = Hit.Spec ? Hit.Spec->Definition.Get() : nullptr;
		return HitDefinition && HitDefinition == HostSpec.Definition.Get();
	}

	FMassEntityHandle GetOther(FMassEntityHandle Owner, const ArcTriggers::FArcIfHit& Hit)
	{
		return Owner == Hit.Source ? Hit.Target : Hit.Source;
	}

	bool IsUsed(const ArcTriggers::FArcIfHit& Hit, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex)
	{
		return Hit.Used.ContainsByPredicate([Owner, HostHandle, TaskIndex](const ArcTriggers::FArcIfUse& Use)
		{
			return Use.Owner == Owner && Use.HostHandle == HostHandle && Use.TaskIndex == TaskIndex;
		});
	}

	bool IsSpentOnlyOnCrit(const FArcEffectTask_TriggerIf& Task, const ArcTriggers::FArcIfHit& Hit)
	{
		return Task.SpendOn == EArcIfSpendOn::OnlyCrit && !Hit.bCrit;
	}

	FArcEffectStackFragment* FindStack(FMassEntityManager& EntityManager, FMassEntityHandle Owner)
	{
		if (!EntityManager.IsEntityValid(Owner))
		{
			return nullptr;
		}
		return FMassEntityView(EntityManager, Owner).GetFragmentDataPtr<FArcEffectStackFragment>();
	}

	FArcTriggerContext MakeIfContext(FMassEntityManager& EntityManager, FMassEntityHandle Owner, const FArcEffectSpec& HostSpec,
		const ArcTriggers::FArcIfHit& Hit, bool bAsSource)
	{
		FArcTriggerContext Context;
		Context.EntityManager = &EntityManager;
		Context.Owner = Owner;
		Context.Other = bAsSource ? Hit.Target : Hit.Source;
		Context.HostSource = HostSpec.Context.SourceEntity;
		Context.HostContext = &HostSpec.Context;
		Context.bHasHit = true;
		Context.bCritRolled = Hit.bCritRolled;
		Context.bCrit = Hit.bCrit;
		Context.Zone = Hit.Zone;
		Context.DamageType = Hit.DamageType;
		if (Hit.Spec)
		{
			Context.HitContext = &Hit.Spec->Context;
			Context.Ability = Hit.Spec->Context.SourceAbility.Get();
			Context.OwnerTags = bAsSource ? &Hit.Spec->CapturedSourceTags : &Hit.Spec->CapturedTargetTags;
			Context.OtherTags = bAsSource ? &Hit.Spec->CapturedTargetTags : &Hit.Spec->CapturedSourceTags;
		}
		return Context;
	}

	void FireDeferred(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle, int32 TaskIndex,
		FMassEntityHandle Other, const UArcAbilityDefinition* Ability, FChargeOutcome Outcome, bool bFireEffects)
	{
		const TWeakObjectPtr<const UArcAbilityDefinition> WeakAbility = Ability;
		EntityManager.Defer().PushCommand<FMassDeferredSetCommand>(
			[Owner, HostHandle, TaskIndex, Other, WeakAbility, Outcome, bFireEffects](FMassEntityManager& DeferredManager)
			{
				FArcActiveEffect* Host = ArcTriggers::FindHostEffect(DeferredManager, Owner, HostHandle);
				FArcEffectTask_TriggerIf* Task = ArcTriggers::FindTask<FArcEffectTask_TriggerIf>(Host, TaskIndex);
				if (!Task)
				{
					return;
				}

				const FString Description = ArcTriggers::DescribeTask(*Host, TaskIndex, Kind);
				const EArcTriggerChargeMode ChargeMode = Task->ChargeMode;
				const bool bRemoveHostWhenChargesSpent = Task->bRemoveHostWhenChargesSpent;

				if (bFireEffects)
				{
					const FSharedStruct HostSpecStruct = Host->Spec;
					const FArcEffectSpec* HostSpec = HostSpecStruct.GetPtr<FArcEffectSpec>();
					if (!HostSpec)
					{
						return;
					}

					FArcTriggerContext Context;
					Context.EntityManager = &DeferredManager;
					Context.Owner = Owner;
					Context.Other = Other;
					Context.HostSource = HostSpec->Context.SourceEntity;
					Context.HostContext = &HostSpec->Context;
					Context.Ability = WeakAbility.Get();

					if (!Task->RemoveWithHost.IsEmpty())
					{
						ArcTriggers::PruneAppliedEffects(DeferredManager, Task->RemoveWithHost);
					}

					const TArray<FInstancedStruct> Effects = Task->Effects;
					FArcTriggerAppliedList Applied;
					ArcTriggers::FireEffects(Effects, Context, nullptr, &Applied);

					Task = ArcTriggers::FindTask<FArcEffectTask_TriggerIf>(ArcTriggers::FindHostEffect(DeferredManager, Owner, HostHandle), TaskIndex);
					if (!Task)
					{
						ArcTriggers::RemoveAppliedEffects(DeferredManager, Applied);
						return;
					}
					Task->RemoveWithHost.Append(Applied);
				}

				if (ChargeMode == EArcTriggerChargeMode::HostStack && Outcome.Result == EChargeResult::Consumed)
				{
					Task->PendingHostStackConsumes = FMath::Max(0, Task->PendingHostStackConsumes - 1);
				}
				ArcTriggers::PayCharge(DeferredManager, Owner, HostHandle, ChargeMode, Outcome, bRemoveHostWhenChargesSpent, Description);
			});
	}

	void CommitEffectsForOwner(FMassEntityManager& EntityManager, const ArcTriggers::FArcIfHit& Hit, FMassEntityHandle Owner,
		bool bOwnerIsSource, bool bOwnerIsTarget)
	{
		FArcEffectStackFragment* Stack = FindStack(EntityManager, Owner);
		if (!Stack)
		{
			return;
		}

		for (FArcActiveEffect& Host : Stack->ActiveEffects)
		{
			const FArcEffectSpec* HostSpec = Host.Spec.GetPtr<FArcEffectSpec>();
			if (!HostSpec || IsHostOfHit(*HostSpec, Hit))
			{
				continue;
			}

			for (int32 TaskIndex = 0; TaskIndex < Host.Tasks.Num(); ++TaskIndex)
			{
				FArcEffectTask_TriggerIf* Task = Host.Tasks[TaskIndex].GetMutablePtr<FArcEffectTask_TriggerIf>();
				if (!Task || Task->Effects.IsEmpty() || IsUsed(Hit, Owner, Host.Handle, TaskIndex))
				{
					continue;
				}

				const bool bAsSource = bOwnerIsSource && AllowsSide(Task->Role, true);
				const bool bAsTarget = bOwnerIsTarget && AllowsSide(Task->Role, false);
				if (!bAsSource && !bAsTarget)
				{
					continue;
				}

				const FString Description = ArcTriggers::DescribeTask(Host, TaskIndex, Kind);
				const FArcTriggerContext Context = MakeIfContext(EntityManager, Owner, *HostSpec, Hit, bAsSource);
				if (!ArcTriggers::AreConditionsMet(Task->Conditions, Context))
				{
					UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls effects skipped: conditions unmet (hit %ls)", *Description, *GetHitName(Hit));
					continue;
				}
				if (IsSpentOnlyOnCrit(*Task, Hit))
				{
					UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls effects skipped: spends only on a crit (hit %ls)", *Description, *GetHitName(Hit));
					continue;
				}

				const FChargeOutcome Outcome = ArcTriggers::TryConsumeCharge(EntityManager, Owner, Host.Handle, Task->ChargeMode,
					Task->RemainingCharges, Task->PendingHostStackConsumes, Description);
				if (Outcome.Result == EChargeResult::Spent)
				{
					UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls effects skipped: charges spent (hit %ls)", *Description, *GetHitName(Hit));
					continue;
				}

				UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls effects queued: other %ls, charge %ls (hit %ls)",
					*Description, *Context.Other.DebugGetDescription(), ArcTriggers::GetChargeResultName(Outcome.Result), *GetHitName(Hit));
				FireDeferred(EntityManager, Owner, Host.Handle, TaskIndex, Context.Other, Context.Ability, Outcome, true);
			}
		}
	}
}

void FArcEffectTask_TriggerIf::OnApplication(FArcActiveEffect& OwningEffect)
{
	RemainingCharges = ArcTriggers::GetInitialCharges(ChargeMode, Charges);
	PendingHostStackConsumes = 0;
}

void FArcEffectTask_TriggerIf::OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason)
{
	UE_LOGF(LogArcMassMechanics, Verbose, "Trigger If owner %ls removed (%ls): tearing down",
		*OwningEffect.OwnerEntity.DebugGetDescription(), *UEnum::GetValueAsString(Reason));
	ArcTriggers::DeferRemoveAppliedEffects(OwningEffect.OwnerWorld, MoveTemp(RemoveWithHost));
	RemoveWithHost.Reset();
}

float ArcTriggers::GetIfModifier(FMassEntityManager& EntityManager, FArcIfHit& Hit, EArcIfChannel Channel)
{
	using namespace ArcTriggerIfPrivate;

	const bool bMultiplier = IsMultiplierChannel(Channel);
	const float Identity = bMultiplier ? 1.f : 0.f;
	float Result = Identity;

	const bool bAsSource = IsDoneChannel(Channel);
	const FMassEntityHandle Owner = bAsSource ? Hit.Source : Hit.Target;
	FArcEffectStackFragment* Stack = FindStack(EntityManager, Owner);
	if (!Stack)
	{
		return Result;
	}

	for (FArcActiveEffect& Host : Stack->ActiveEffects)
	{
		const FArcEffectSpec* HostSpec = Host.Spec.GetPtr<FArcEffectSpec>();
		if (!HostSpec || IsHostOfHit(*HostSpec, Hit))
		{
			continue;
		}

		for (int32 TaskIndex = 0; TaskIndex < Host.Tasks.Num(); ++TaskIndex)
		{
			FArcEffectTask_TriggerIf* Task = Host.Tasks[TaskIndex].GetMutablePtr<FArcEffectTask_TriggerIf>();
			if (!Task)
			{
				continue;
			}

			bool bHasChannel = false;
			float Contribution = Identity;
			for (const FArcIfModifier& Modifier : Task->Modifiers)
			{
				if (Modifier.Channel != Channel)
				{
					continue;
				}
				bHasChannel = true;
				Contribution = bMultiplier ? Contribution * FMath::Max(0.f, 1.f + Modifier.Value) : Contribution + Modifier.Value;
			}
			if (!bHasChannel)
			{
				continue;
			}

			const FString Description = DescribeTask(Host, TaskIndex, Kind);
			if (!AllowsSide(Task->Role, bAsSource))
			{
				UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped %ls: role %ls excludes the %ls side (hit %ls)",
					*Description, *UEnum::GetValueAsString(Channel), *UEnum::GetValueAsString(Task->Role),
					bAsSource ? TEXT("source") : TEXT("target"), *GetHitName(Hit));
				continue;
			}
			if (!AreConditionsMet(Task->Conditions, MakeIfContext(EntityManager, Owner, *HostSpec, Hit, bAsSource)))
			{
				UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped %ls: conditions unmet (hit %ls, crit rolled %d, crit %d, zone %ls)",
					*Description, *UEnum::GetValueAsString(Channel), *GetHitName(Hit),
					Hit.bCritRolled ? 1 : 0, Hit.bCrit ? 1 : 0, *Hit.Zone.ToString());
				continue;
			}

			if (!IsUsed(Hit, Owner, Host.Handle, TaskIndex))
			{
				const FChargeOutcome Outcome = TryConsumeCharge(EntityManager, Owner, Host.Handle, Task->ChargeMode,
					Task->RemainingCharges, Task->PendingHostStackConsumes, Description);
				if (Outcome.Result == EChargeResult::Spent)
				{
					UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls skipped %ls: charges spent (hit %ls)",
						*Description, *UEnum::GetValueAsString(Channel), *GetHitName(Hit));
					continue;
				}
				Hit.Used.Add(FArcIfUse{Owner, Host.Handle, TaskIndex, Outcome});
			}

			Result = bMultiplier ? Result * Contribution : Result + Contribution;
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls contributed %ls: %.4f, channel now %.4f (hit %ls)",
				*Description, *UEnum::GetValueAsString(Channel), Contribution, Result, *GetHitName(Hit));
		}
	}
	return Result;
}

void ArcTriggers::CommitIf(FMassEntityManager& EntityManager, const FArcIfHit& Hit)
{
	using namespace ArcTriggerIfPrivate;

	const bool bTriggerOriginated = Hit.Spec && IsTriggerOriginated(Hit.Spec->Context.OriginTags);
	const UArcAbilityDefinition* Ability = Hit.Spec ? Hit.Spec->Context.SourceAbility.Get() : nullptr;

	for (const FArcIfUse& Use : Hit.Used)
	{
		FArcActiveEffect* Host = FindHostEffect(EntityManager, Use.Owner, Use.HostHandle);
		FArcEffectTask_TriggerIf* Task = FindTask<FArcEffectTask_TriggerIf>(Host, Use.TaskIndex);
		if (!Task)
		{
			continue;
		}

		const FString Description = DescribeTask(*Host, Use.TaskIndex, Kind);
		if (IsSpentOnlyOnCrit(*Task, Hit))
		{
			RefundCharge(Task->ChargeMode, Use.Outcome, Task->RemainingCharges, Task->PendingHostStackConsumes);
			UE_LOGF(LogArcMassMechanics, Verbose, "CommitIf %ls: no crit, charge %ls refunded, effects skipped (hit %ls)",
				*Description, GetChargeResultName(Use.Outcome.Result), *GetHitName(Hit));
			continue;
		}

		const bool bFireEffects = !bTriggerOriginated && !Task->Effects.IsEmpty();
		UE_LOGF(LogArcMassMechanics, Verbose, "CommitIf %ls: fire effects %d (trigger-originated hit %d), charge %ls (hit %ls)",
			*Description, bFireEffects ? 1 : 0, bTriggerOriginated ? 1 : 0, GetChargeResultName(Use.Outcome.Result), *GetHitName(Hit));
		if (bFireEffects || Use.Outcome.Result == EChargeResult::Consumed)
		{
			FireDeferred(EntityManager, Use.Owner, Use.HostHandle, Use.TaskIndex, GetOther(Use.Owner, Hit), Ability, Use.Outcome, bFireEffects);
		}
	}

	if (bTriggerOriginated)
	{
		return;
	}

	if (Hit.Source == Hit.Target)
	{
		CommitEffectsForOwner(EntityManager, Hit, Hit.Source, true, true);
		return;
	}
	CommitEffectsForOwner(EntityManager, Hit, Hit.Source, true, false);
	CommitEffectsForOwner(EntityManager, Hit, Hit.Target, false, true);
}
