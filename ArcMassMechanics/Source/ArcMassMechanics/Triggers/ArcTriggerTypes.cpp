// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectTask.h"
#include "Events/ArcGameEventTags.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassEntitySubsystem.h"
#include "Modifiers/ArcModifierFunctions.h"

#define LOCTEXT_NAMESPACE "ArcTriggerTypes"

namespace ArcTriggerTypesPrivate
{
	void AddWhileOtherError(EArcTriggerParty Party, const FText& Error, TArray<FText>& OutErrors)
	{
		if (Party == EArcTriggerParty::Other)
		{
			OutErrors.Add(Error);
		}
	}

	void ValidateTagsCommon(const FArcTriggerCondition_Tags& Condition, TArray<FText>& OutErrors)
	{
		if (Condition.Requirements.IsEmpty())
		{
			OutErrors.Add(LOCTEXT("TagsEmpty", "Tags condition has no requirements."));
		}
	}

	void ValidateThresholdCommon(const FArcTriggerCondition_AttributeThreshold& Condition, TArray<FText>& OutErrors)
	{
		if (!Condition.Attribute.IsValid())
		{
			OutErrors.Add(LOCTEXT("ThresholdNoAttribute", "Attribute Threshold has no Attribute."));
		}
	}

	void ValidateAbilityCommon(const FArcTriggerCondition_Ability& Condition, TArray<FText>& OutErrors)
	{
		if (!Condition.Ability && Condition.Requirements.IsEmpty())
		{
			OutErrors.Add(LOCTEXT("AbilityEmpty", "Ability condition has neither an Ability nor requirements."));
		}
	}

	void ValidateApplyEffectCommon(const FArcTriggerEffect_ApplyEffect& Effect, TArray<FText>& OutErrors)
	{
		if (!Effect.Effect)
		{
			OutErrors.Add(LOCTEXT("ApplyNoEffect", "Apply Effect has no Effect."));
		}
		if (Effect.Duration.IsSet() && Effect.Duration.GetValue() <= 0.f)
		{
			OutErrors.Add(LOCTEXT("ApplyDurationNonPositive", "Apply Effect Duration must be above 0 when set; clear it to use the effect's own duration."));
		}
	}

	void ValidateCastAbilityCommon(const FArcTriggerEffect_CastAbility& Effect, TArray<FText>& OutErrors)
	{
		if (!Effect.InputTag.IsValid())
		{
			OutErrors.Add(LOCTEXT("CastNoInput", "Cast Ability has no InputTag."));
		}
	}

	int32 GetHostStackCount(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle)
	{
		const FArcActiveEffect* Host = ArcTriggers::FindHostEffect(EntityManager, Owner, HostHandle);
		const FArcEffectSpec* HostSpec = Host ? Host->Spec.GetPtr<FArcEffectSpec>() : nullptr;
		return HostSpec ? HostSpec->Context.StackCount : 0;
	}
}

bool ArcTriggers::Compare(float Lhs, EArcTriggerCompare Op, float Rhs)
{
	switch (Op)
	{
	case EArcTriggerCompare::Less:
		return Lhs < Rhs;
	case EArcTriggerCompare::LessOrEqual:
		return Lhs <= Rhs;
	case EArcTriggerCompare::Greater:
		return Lhs > Rhs;
	case EArcTriggerCompare::GreaterOrEqual:
		return Lhs >= Rhs;
	}
	return false;
}

bool ArcTriggers::IsTriggerOriginated(const FGameplayTagContainer& Tags)
{
	return Tags.HasTagExact(ArcMassMechanicsTags::TriggerOrigin);
}

bool ArcTriggers::IsTagChangeChannel(const FGameplayTag& EventTag)
{
	return EventTag == ArcGameEventTags::TagAdded.GetTag() || EventTag == ArcGameEventTags::TagRemoved.GetTag();
}

FMassEntityManager* ArcTriggers::GetEntityManager(const UWorld* World)
{
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	return EntitySubsystem ? &EntitySubsystem->GetMutableEntityManager() : nullptr;
}

FArcActiveEffect* ArcTriggers::FindHostEffect(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle)
{
	if (!EntityManager.IsEntityValid(Owner))
	{
		return nullptr;
	}
	FArcEffectStackFragment* Stack = FMassEntityView(EntityManager, Owner).GetFragmentDataPtr<FArcEffectStackFragment>();
	return Stack ? ArcEffects::FindActiveEffect(*Stack, HostHandle) : nullptr;
}

int32 ArcTriggers::FindTaskIndex(const FArcActiveEffect& HostEffect, const FArcEffectTask* Task)
{
	for (int32 Index = 0; Index < HostEffect.Tasks.Num(); ++Index)
	{
		if (HostEffect.Tasks[Index].GetPtr<FArcEffectTask>() == Task)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool ArcTriggers::AreConditionsMet(TConstArrayView<FInstancedStruct> Conditions, const FArcTriggerContext& Context)
{
	for (const FInstancedStruct& ConditionStruct : Conditions)
	{
		const FArcTriggerCondition* Condition = ConditionStruct.GetPtr<FArcTriggerCondition>();
		if (Condition && !Condition->IsMet(Context))
		{
			return false;
		}
	}
	return true;
}

void ArcTriggers::FireEffects(TConstArrayView<FInstancedStruct> Effects, const FArcTriggerContext& Context,
	FArcTriggerAppliedList* OutWhileApplied, FArcTriggerAppliedList* OutRemoveWithHost)
{
	for (const FInstancedStruct& EffectStruct : Effects)
	{
		if (const FArcTriggerEffect* TriggerEffect = EffectStruct.GetPtr<FArcTriggerEffect>())
		{
			TriggerEffect->Fire(Context, OutWhileApplied, OutRemoveWithHost);
		}
	}
}

void ArcTriggers::RemoveAppliedEffects(FMassEntityManager& EntityManager, const FArcTriggerAppliedList& Applied)
{
	for (const TPair<FMassEntityHandle, FArcActiveEffectHandle>& Pair : Applied)
	{
		if (!EntityManager.IsEntityValid(Pair.Key))
		{
			continue;
		}
		FArcEffectRemovalQuery Query;
		Query.Handle = Pair.Value;
		Query.Reason = EArcEffectRemovalReason::Removed;
		ArcEffects::RemoveEffects(EntityManager, Pair.Key, Query);
	}
}

void ArcTriggers::PruneAppliedEffects(FMassEntityManager& EntityManager, FArcTriggerAppliedList& Applied)
{
	Applied.RemoveAllSwap([&EntityManager](const TPair<FMassEntityHandle, FArcActiveEffectHandle>& Pair)
	{
		return FindHostEffect(EntityManager, Pair.Key, Pair.Value) == nullptr;
	});
}

void ArcTriggers::DeferRemoveAppliedEffects(UWorld* World, FArcTriggerAppliedList&& Applied)
{
	FMassEntityManager* EntityManager = GetEntityManager(World);
	if (!EntityManager || Applied.IsEmpty())
	{
		return;
	}

	EntityManager->Defer().PushCommand<FMassDeferredSetCommand>([Applied = MoveTemp(Applied)](FMassEntityManager& DeferredManager)
	{
		RemoveAppliedEffects(DeferredManager, Applied);
	});
}

int32 ArcTriggers::GetInitialCharges(EArcTriggerChargeMode ChargeMode, int32 Charges)
{
	return ChargeMode == EArcTriggerChargeMode::Trigger ? FMath::Max(1, Charges) : -1;
}

ArcTriggers::FChargeOutcome ArcTriggers::TryConsumeCharge(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle,
	EArcTriggerChargeMode ChargeMode, int32& RemainingCharges, int32& PendingHostStackConsumes, const FString& Description)
{
	FChargeOutcome Outcome;
	if (ChargeMode == EArcTriggerChargeMode::HostStack)
	{
		const int32 HostStacks = ArcTriggerTypesPrivate::GetHostStackCount(EntityManager, Owner, HostHandle);
		if (PendingHostStackConsumes >= HostStacks)
		{
			UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: spent (host stacks %d, reserved %d)",
				*Description, HostStacks, PendingHostStackConsumes);
			Outcome.Result = EChargeResult::Spent;
			return Outcome;
		}
		++PendingHostStackConsumes;
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: reserved a host stack (host stacks %d, reserved %d)",
			*Description, HostStacks, PendingHostStackConsumes);
		Outcome.Result = EChargeResult::Consumed;
		return Outcome;
	}

	if (RemainingCharges < 0)
	{
		return Outcome;
	}
	if (RemainingCharges == 0)
	{
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: spent (0 remaining)", *Description);
		Outcome.Result = EChargeResult::Spent;
		return Outcome;
	}

	--RemainingCharges;
	Outcome.Result = EChargeResult::Consumed;
	Outcome.bAllSpent = RemainingCharges == 0;
	UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: reserved (%d remaining)", *Description, RemainingCharges);
	return Outcome;
}

void ArcTriggers::RefundCharge(EArcTriggerChargeMode ChargeMode, const FChargeOutcome& Outcome, int32& RemainingCharges, int32& PendingHostStackConsumes)
{
	if (Outcome.Result != EChargeResult::Consumed)
	{
		return;
	}
	if (ChargeMode == EArcTriggerChargeMode::HostStack)
	{
		PendingHostStackConsumes = FMath::Max(0, PendingHostStackConsumes - 1);
		return;
	}
	if (RemainingCharges >= 0)
	{
		++RemainingCharges;
	}
}

void ArcTriggers::PayCharge(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle,
	EArcTriggerChargeMode ChargeMode, const FChargeOutcome& Outcome, bool bRemoveHostWhenChargesSpent, const FString& Description)
{
	if (Outcome.Result != EChargeResult::Consumed || !EntityManager.IsEntityValid(Owner))
	{
		return;
	}

	if (ChargeMode == EArcTriggerChargeMode::HostStack)
	{
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: paid one host stack", *Description);
		ArcEffects::ConsumeStack(EntityManager, Owner, HostHandle, 1);
		return;
	}

	if (Outcome.bAllSpent && bRemoveHostWhenChargesSpent)
	{
		UE_LOGF(LogArcMassMechanics, Verbose, "Trigger %ls charge: all charges spent, removing the host", *Description);
		FArcEffectRemovalQuery Query;
		Query.Handle = HostHandle;
		Query.Reason = EArcEffectRemovalReason::Removed;
		ArcEffects::RemoveEffects(EntityManager, Owner, Query);
	}
}

const TCHAR* ArcTriggers::GetChargeResultName(EChargeResult Result)
{
	switch (Result)
	{
	case EChargeResult::Unlimited:
		return TEXT("Unlimited");
	case EChargeResult::Consumed:
		return TEXT("Consumed");
	case EChargeResult::Spent:
		return TEXT("Spent");
	}
	return TEXT("Unknown");
}

FString ArcTriggers::DescribeTask(const FArcActiveEffect& HostEffect, int32 TaskIndex, const TCHAR* Kind)
{
	const FArcEffectSpec* Spec = HostEffect.Spec.GetPtr<FArcEffectSpec>();
	return FString::Printf(TEXT("%s[%d] %s owner %s"), *GetNameSafe(Spec ? Spec->Definition.Get() : nullptr), TaskIndex, Kind,
		*HostEffect.OwnerEntity.DebugGetDescription());
}

FMassEntityHandle FArcTriggerContext::GetEntity(EArcTriggerParty Party) const
{
	switch (Party)
	{
	case EArcTriggerParty::Owner:
		return Owner;
	case EArcTriggerParty::Other:
		return Other;
	case EArcTriggerParty::HostSource:
		return HostSource;
	}
	return FMassEntityHandle();
}

FGameplayTagContainer FArcTriggerContext::GetTags(EArcTriggerParty Party) const
{
	if (Party == EArcTriggerParty::Owner && OwnerTags)
	{
		return *OwnerTags;
	}
	if (Party == EArcTriggerParty::Other && OtherTags)
	{
		return *OtherTags;
	}

	const FMassEntityHandle Entity = GetEntity(Party);
	if (!EntityManager || !EntityManager->IsEntityValid(Entity))
	{
		return FGameplayTagContainer();
	}

	const FMassEntityView EntityView(*EntityManager, Entity);
	const FArcOwnedTagsFragment* OwnedTags = EntityView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
	return OwnedTags ? OwnedTags->GetTags().GetTagContainer() : FGameplayTagContainer();
}

bool FArcTriggerCondition_Tags::IsMet(const FArcTriggerContext& Context) const
{
	return Requirements.RequirementsMet(Context.GetTags(Party));
}

bool FArcTriggerCondition_Tags::GetAsTagRequirements(EArcTriggerParty& OutParty, FArcGameplayTagRequirements& OutRequirements) const
{
	OutParty = Party;
	OutRequirements = Requirements;
	return true;
}

void FArcTriggerCondition_Tags::ValidateInOn(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateTagsCommon(*this, OutErrors);
}

void FArcTriggerCondition_Tags::ValidateInWhile(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateTagsCommon(*this, OutErrors);
	ArcTriggerTypesPrivate::AddWhileOtherError(Party,
		LOCTEXT("TagsWhileOther", "A While has no other party; its Tags condition must read the owner or the host source."), OutErrors);
}

void FArcTriggerCondition_Tags::ValidateInIf(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateTagsCommon(*this, OutErrors);
}

bool FArcTriggerCondition_AttributeThreshold::IsMet(const FArcTriggerContext& Context) const
{
	const FMassEntityHandle Entity = Context.GetEntity(Party);
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Entity))
	{
		return false;
	}

	const FArcAttribute* Current = ArcAttributes::ResolveAttribute(*Context.EntityManager, Entity, Attribute);
	if (!Current)
	{
		return false;
	}

	float Measured = Current->GetCurrentValue();
	if (MaxAttribute.IsValid())
	{
		const FArcAttribute* Max = ArcAttributes::ResolveAttribute(*Context.EntityManager, Entity, MaxAttribute);
		if (!Max || Max->GetCurrentValue() <= 0.f)
		{
			return false;
		}
		Measured /= Max->GetCurrentValue();
	}

	return ArcTriggers::Compare(Measured, Compare, Value);
}

bool FArcTriggerCondition_AttributeThreshold::GetWatchedAttributes(EArcTriggerParty& OutParty, TArray<FArcAttributeRef>& OutAttributes) const
{
	OutParty = Party;
	if (Attribute.IsValid())
	{
		OutAttributes.Add(Attribute);
	}
	if (MaxAttribute.IsValid())
	{
		OutAttributes.Add(MaxAttribute);
	}
	return true;
}

void FArcTriggerCondition_AttributeThreshold::ValidateInOn(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateThresholdCommon(*this, OutErrors);
	OutErrors.Add(LOCTEXT("ThresholdOn", "An On trigger cannot use a stat threshold as a condition; use a While, or an event published when the threshold is crossed."));
}

void FArcTriggerCondition_AttributeThreshold::ValidateInWhile(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateThresholdCommon(*this, OutErrors);
	ArcTriggerTypesPrivate::AddWhileOtherError(Party,
		LOCTEXT("ThresholdWhileOther", "A While has no other party; its Attribute Threshold must read the owner or the host source."), OutErrors);
}

void FArcTriggerCondition_AttributeThreshold::ValidateInIf(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateThresholdCommon(*this, OutErrors);
}

bool FArcTriggerCondition_EventMagnitude::IsMet(const FArcTriggerContext& Context) const
{
	return Context.Event && ArcTriggers::Compare(Context.Event->Magnitude, Compare, Value);
}

void FArcTriggerCondition_EventMagnitude::ValidateInWhile(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("MagnitudeNotOn", "Event Magnitude needs an On trigger."));
}

void FArcTriggerCondition_EventMagnitude::ValidateInIf(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("MagnitudeNotOn", "Event Magnitude needs an On trigger."));
}

bool FArcTriggerCondition_HitResult::IsMet(const FArcTriggerContext& Context) const
{
	if (!Context.bHasHit)
	{
		return false;
	}
	if (!Context.bCritRolled && (Crit != EArcTriggerCritFilter::Any || !Zones.IsEmpty()))
	{
		return false;
	}
	if ((Crit == EArcTriggerCritFilter::Crit && !Context.bCrit) || (Crit == EArcTriggerCritFilter::NotCrit && Context.bCrit))
	{
		return false;
	}
	if (!Zones.IsEmpty() && !(Context.Zone.IsValid() && Context.Zone.MatchesAny(Zones)))
	{
		return false;
	}
	if (!DamageTypes.IsEmpty() && !(Context.DamageType.IsValid() && Context.DamageType.MatchesAny(DamageTypes)))
	{
		return false;
	}
	return true;
}

void FArcTriggerCondition_HitResult::ValidateInWhile(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("HitWhile", "Hit Result needs an On or If trigger."));
}

bool FArcTriggerCondition_EffectTags::IsMet(const FArcTriggerContext& Context) const
{
	return Context.EventEffect && Requirements.RequirementsMet(Context.EventEffect->GetEffectTags());
}

void FArcTriggerCondition_EffectTags::ValidateInWhile(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("EffectTagsNotOn", "Effect Tags needs an On trigger on an effect event (ArcEvent.Effect.Applied, Executed, Removed, Rejected), a hit outcome event (ArcMech.Hit.Outcome.Hit, Miss, Dodge, Parry, Block) or ArcMech.Damage.Dealt."));
}

void FArcTriggerCondition_EffectTags::ValidateInIf(TArray<FText>& OutErrors) const
{
	OutErrors.Add(LOCTEXT("EffectTagsNotOn", "Effect Tags needs an On trigger on an effect event (ArcEvent.Effect.Applied, Executed, Removed, Rejected), a hit outcome event (ArcMech.Hit.Outcome.Hit, Miss, Dodge, Parry, Block) or ArcMech.Damage.Dealt."));
}

bool FArcTriggerCondition_Ability::IsMet(const FArcTriggerContext& Context) const
{
	if (!Context.Ability)
	{
		return false;
	}
	if (Ability && Context.Ability != Ability)
	{
		return false;
	}
	return Requirements.IsEmpty() || Requirements.RequirementsMet(Context.Ability->AbilityTags);
}

void FArcTriggerCondition_Ability::ValidateInOn(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateAbilityCommon(*this, OutErrors);
}

void FArcTriggerCondition_Ability::ValidateInWhile(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateAbilityCommon(*this, OutErrors);
	OutErrors.Add(LOCTEXT("AbilityWhile", "Ability is an action filter and needs an On or If trigger."));
}

void FArcTriggerCondition_Ability::ValidateInIf(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateAbilityCommon(*this, OutErrors);
}

void FArcTriggerEffect_ApplyEffect::Fire(const FArcTriggerContext& Context, FArcTriggerAppliedList* OutWhileApplied,
	FArcTriggerAppliedList* OutRemoveWithHost) const
{
	const FMassEntityHandle Target = Context.GetEntity(Recipient);
	if (!Effect || !Context.HostContext || !Context.EntityManager || !Context.EntityManager->IsEntityValid(Target))
	{
		return;
	}

	FMassEntityManager& EntityManager = *Context.EntityManager;

	FArcEffectApplyRequest Request;
	Request.Definition = Effect;
	Request.Context = ArcEffects::MakeChildContext(*Context.HostContext,
		FGameplayTagContainer(ArcMassMechanicsTags::TriggerOrigin));
	Request.Context.SourceEntity = Context.GetEntity(Instigator);
	Request.DurationOverride = Duration;

	const FArcEffectApplyResult Result = ArcEffects::TryApplyEffect(EntityManager, Target, Request);
	if (!Result.Handle.IsValid())
	{
		return;
	}

	if (OutWhileApplied)
	{
		OutWhileApplied->Emplace(Target, Result.Handle);
	}
	else if (bRemoveWithHost && OutRemoveWithHost)
	{
		OutRemoveWithHost->Emplace(Target, Result.Handle);
	}
}

void FArcTriggerEffect_ApplyEffect::ValidateInOn(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateApplyEffectCommon(*this, OutErrors);
}

void FArcTriggerEffect_ApplyEffect::ValidateInWhile(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateApplyEffectCommon(*this, OutErrors);
	if (Recipient == EArcTriggerParty::Other || Instigator == EArcTriggerParty::Other)
	{
		OutErrors.Add(LOCTEXT("ApplyWhileOther", "A While has no other party; Apply Effect cannot use Other as recipient or instigator."));
	}
}

void FArcTriggerEffect_ApplyEffect::ValidateInIf(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateApplyEffectCommon(*this, OutErrors);
}

void FArcTriggerEffect_CastAbility::Fire(const FArcTriggerContext& Context, FArcTriggerAppliedList* OutWhileApplied,
	FArcTriggerAppliedList* OutRemoveWithHost) const
{
	const FMassEntityHandle Entity = Context.GetEntity(Caster);
	if (!InputTag.IsValid() || !Context.EntityManager || !Context.EntityManager->IsEntityValid(Entity))
	{
		return;
	}

	ArcAbilities::PushInput(*Context.EntityManager, Entity, InputTag, FGameplayTagContainer(ArcMassMechanicsTags::TriggerOrigin));
	if (bReleaseInput)
	{
		ArcAbilities::ReleaseInput(*Context.EntityManager, Entity, InputTag);
	}
}

void FArcTriggerEffect_CastAbility::ValidateInOn(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateCastAbilityCommon(*this, OutErrors);
}

void FArcTriggerEffect_CastAbility::ValidateInWhile(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateCastAbilityCommon(*this, OutErrors);
	ArcTriggerTypesPrivate::AddWhileOtherError(Caster,
		LOCTEXT("CastWhileOther", "A While has no other party; Cast Ability cannot use Other as caster."), OutErrors);
}

void FArcTriggerEffect_CastAbility::ValidateInIf(TArray<FText>& OutErrors) const
{
	ArcTriggerTypesPrivate::ValidateCastAbilityCommon(*this, OutErrors);
}

#undef LOCTEXT_NAMESPACE
