// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectFunctions.h"
#include "ArcMassAbilitiesLogs.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectComp_Inhibition.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectApplyRequest.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Events/ArcGameEventTags.h"
#include "Events/ArcEffectEventPayloads.h"
#include "Debugger/ArcMassAbilityTrace.h"
#include "Attributes/ArcAttributeNotification.h"
#include "Attributes/ArcInstantModifier.h"
#include "StructUtils/SharedStruct.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"

namespace ArcEffects
{

namespace Private
{

void RunComponentHook(const TArray<FInstancedStruct>& Components,
                      TFunctionRef<void(const FArcEffectComponent&)> Func)
{
	for (const FInstancedStruct& CompStruct : Components)
	{
		if (CompStruct.IsValid())
		{
			if (const FArcEffectComponent* Comp = CompStruct.GetPtr<FArcEffectComponent>())
			{
				Func(*Comp);
			}
		}
	}
}

void QueueEffectEvent(UArcGameEventSubsystem& EventBus, FGameplayTag Tag, FMassEntityHandle Target,
                      const FArcEffectSpec& Spec, FInstancedStruct&& Payload)
{
	FArcGameEvent Event;
	Event.Tag = Tag;
	Event.Instigator = Spec.Context.SourceEntity;
	Event.Target = Target;
	Event.SourceTags = Spec.CapturedSourceTags;
	Event.TargetTags = Spec.CapturedTargetTags;
	Event.Payload = MoveTemp(Payload);
	EventBus.Queue(MoveTemp(Event));
}

void QueueEffectApplied(FMassEntityManager& EntityManager, FMassEntityHandle Target, const FSharedStruct& SharedSpec,
                        FArcActiveEffectHandle Handle, int32 StackCount)
{
	UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld());
	if (!EventBus)
	{
		return;
	}

	const FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
	TRACE_ARCMA_EFFECT_APPLIED(EventBus, Target, Spec.Definition);

	FArcEffectAppliedPayload Payload;
	Payload.Spec = SharedSpec;
	Payload.Handle = Handle;
	Payload.StackCount = StackCount;
	QueueEffectEvent(*EventBus, ArcGameEventTags::EffectApplied, Target, Spec, FInstancedStruct::Make(Payload));
}

void QueueEffectExecuted(FMassEntityManager& EntityManager, FMassEntityHandle Target, const FSharedStruct& SharedSpec,
                         FArcActiveEffectHandle Handle)
{
	UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld());
	if (!EventBus)
	{
		return;
	}

	FArcEffectExecutedPayload Payload;
	Payload.Spec = SharedSpec;
	Payload.Handle = Handle;
	QueueEffectEvent(*EventBus, ArcGameEventTags::EffectExecuted, Target, SharedSpec.Get<FArcEffectSpec>(),
		FInstancedStruct::Make(Payload));
}

void QueueEffectRejected(FMassEntityManager& EntityManager, FMassEntityHandle Target, const FSharedStruct& SharedSpec,
                         FGameplayTag Reason)
{
	UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld());
	if (!EventBus)
	{
		return;
	}

	FArcEffectRejectedPayload Payload;
	Payload.Spec = SharedSpec;
	Payload.Reason = Reason;
	QueueEffectEvent(*EventBus, ArcGameEventTags::EffectRejected, Target, SharedSpec.Get<FArcEffectSpec>(),
		FInstancedStruct::Make(Payload));
}

void SendRecalculateSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
		TRACE_ARCMA_SIGNAL_RAISED(UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()), Entity,
			UE::ArcMassAbilities::Signals::AttributeRecalculate);
	}
}

void SendPendingOpsSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
		TRACE_ARCMA_SIGNAL_RAISED(UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()), Entity,
			UE::ArcMassAbilities::Signals::PendingAttributeOps);
	}
}

void RunExecutions(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                   const TArray<FArcEffectExecutionEntry>& Executions,
                   const FArcEffectSpec& Spec,
                   const FSharedStruct& SpecStruct,
                   EArcEffectExecutionTrigger Trigger,
                   const FGameplayTagContainer& PassedInTags)
{
	FMassEntityView EntityView(EntityManager, Target);

	FArcEffectExecutionContext ExecCtx;
	ExecCtx.OwningSpec = &Spec;
	ExecCtx.EntityManager = &EntityManager;
	ExecCtx.TargetEntity = Target;
	ExecCtx.PassedInTags = PassedInTags;
	ExecCtx.Trigger = Trigger;

	FArcPendingAttributeOpsFragment* Pending = EntityView.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();

	for (int32 ExecIdx = 0; ExecIdx < Executions.Num(); ++ExecIdx)
	{
		const FArcEffectExecutionEntry& Entry = Executions[ExecIdx];

		if (Entry.CustomExecution.IsValid())
		{
			const FArcEffectExecution* Exec = Entry.CustomExecution.GetPtr<FArcEffectExecution>();
			if (!Exec)
			{
				continue;
			}

			ExecCtx.ExecutionState = Spec.ExecutionStates.IsValidIndex(ExecIdx) ? &Spec.ExecutionStates[ExecIdx] : nullptr;

			TArray<FArcEffectModifierOutput> Outputs;
			Exec->Execute(ExecCtx, Outputs);

			for (const FArcEffectModifierOutput& Output : Outputs)
			{
				if (!Output.Attribute.IsValid())
				{
					continue;
				}
				FGameplayTagContainer OutputSourceTags = Spec.CapturedSourceTags;
				OutputSourceTags.AppendTags(Output.SourceTags);
				if (Output.Side == EArcEffectOutputSide::Source)
				{
					const FMassEntityHandle SourceEntity = Spec.Context.SourceEntity;
					const bool bQueued = EntityManager.IsEntityValid(SourceEntity)
						&& ArcAttributes::EnqueueInstantModifier(EntityManager, SourceEntity, Output.Attribute,
							Output.Operation, Output.Magnitude, EArcAttributeOpType::Execute, SourceEntity,
							OutputSourceTags, Spec.CapturedTargetTags);
					if (!bQueued)
					{
						ARCMA_VLOG(EntityManager.GetWorld(), Warning,
							TEXT("[Effect] Source output dropped, source has no pending ops effect=%s attr=%s source=%s"),
							Spec.Definition ? *Spec.Definition->GetName() : TEXT("<null>"),
							*Output.Attribute.PropertyName.ToString(),
							*SourceEntity.DebugGetDescription());
					}
					continue;
				}
				if (Pending)
				{
					FArcPendingAttributeOp& PendingOp = Pending->PendingOps.AddDefaulted_GetRef();
					PendingOp.Attribute = Output.Attribute;
					PendingOp.Operation = Output.Operation;
					PendingOp.Magnitude = Output.Magnitude;
					PendingOp.OpType = EArcAttributeOpType::Execute;
					PendingOp.SourceEntity = Spec.Context.SourceEntity;
					PendingOp.CapturedSourceTags = MoveTemp(OutputSourceTags);
					PendingOp.CapturedTargetTags = Spec.CapturedTargetTags;
				}
			}
		}
		else
		{
			for (const FArcEffectModifier& Mod : Entry.Modifiers)
			{
				if (!Mod.Attribute.IsValid())
				{
					continue;
				}

				float ResolvedMag = 0.f;
				switch (Mod.Type)
				{
				case EArcModifierType::Simple:
					ResolvedMag = ArcEffects::ResolveScalableMagnitude(Mod.Magnitude, Mod.RangeResolution);
					break;
				case EArcModifierType::SetByCaller:
				{
					const float* Found = Spec.SetByCallerMagnitudes.Find(Mod.SetByCallerTag);
					ResolvedMag = Found ? *Found : 0.f;
					break;
				}
				case EArcModifierType::Custom:
				{
					if (Mod.CustomMagnitude.IsValid())
					{
						const FArcMagnitudeCalculation* Calc = Mod.CustomMagnitude.GetPtr<FArcMagnitudeCalculation>();
						if (Calc)
						{
							ResolvedMag = Calc->Calculate(Spec.Context);
						}
					}
					break;
				}
				}

				if (Pending)
				{
					FArcPendingAttributeOp& PendingOp = Pending->PendingOps.AddDefaulted_GetRef();
					PendingOp.Attribute = Mod.Attribute;
					PendingOp.Operation = Mod.Operation;
					PendingOp.Magnitude = ResolvedMag;
					PendingOp.OpType = EArcAttributeOpType::Execute;
					PendingOp.SourceEntity = Spec.Context.SourceEntity;
					PendingOp.CapturedSourceTags = Spec.CapturedSourceTags;
					PendingOp.CapturedTargetTags = Spec.CapturedTargetTags;
				}
			}
		}
	}
}

void PushModsToAggregator(FArcAggregatorFragment& AggFragment,
                          UArcEffectDefinition* Effect,
                          FMassEntityHandle Source,
                          int32 StackCount,
                          TArray<FArcModifierHandle>& OutHandles,
                          const TArray<float>* ResolvedMagnitudes)
{
	for (int32 ModIdx = 0; ModIdx < Effect->Modifiers.Num(); ++ModIdx)
	{
		const FArcEffectModifier& Modifier = Effect->Modifiers[ModIdx];

		if (!Modifier.Attribute.IsValid())
		{
			continue;
		}

		const float BaseMag = (ResolvedMagnitudes && ResolvedMagnitudes->IsValidIndex(ModIdx))
			? (*ResolvedMagnitudes)[ModIdx]
			: ArcEffects::ResolveScalableMagnitude(Modifier.Magnitude, Modifier.RangeResolution);
		const float EffectiveMag = ArcAggregatorEval::ScaleByStack(Modifier.Operation, BaseMag, StackCount);

		const bool bLive = Modifier.Type == EArcModifierType::Custom
			&& Modifier.CustomMagnitude.GetPtr<FArcLiveMagnitude>() != nullptr;
		if (bLive)
		{
			const FConstSharedStruct LiveMagnitude = FConstSharedStruct::Make(
				Modifier.CustomMagnitude.GetScriptStruct(), Modifier.CustomMagnitude.GetMemory());
			OutHandles.Add(AggFragment.AddLiveMod(Modifier.Attribute, Modifier.Operation, LiveMagnitude,
			                                      StackCount, EffectiveMag, Source, &Modifier.SourceTagRequirements,
			                                      &Modifier.TargetTagRequirements, Modifier.Channel));
			continue;
		}

		FArcAggregator& Agg = AggFragment.FindOrAddAggregator(Modifier.Attribute);
		FArcModifierHandle Handle = Agg.AddMod(
			Modifier.Operation,
			EffectiveMag,
			Source,
			&Modifier.SourceTagRequirements,
			&Modifier.TargetTagRequirements,
			Modifier.Channel);
		OutHandles.Add(Handle);
	}
}

void RemoveModsFromAggregator(FArcAggregatorFragment& AggFragment,
                              const TArray<FArcModifierHandle>& Handles)
{
	for (const FArcModifierHandle& Handle : Handles)
	{
		AggFragment.RemoveMod(Handle);
	}
}

void RebuildEffectMods(FArcAggregatorFragment& AggFragment, FArcActiveEffect& Effect)
{
	RemoveModsFromAggregator(AggFragment, Effect.ModifierHandles);
	Effect.ModifierHandles.Reset();

	// An effect's mods are in the aggregator iff it is not inhibited.
	const FArcEffectSpec* Spec = Effect.Spec.GetPtr<FArcEffectSpec>();
	if (!Spec || !Spec->Definition || Effect.bInhibited)
	{
		return;
	}

	PushModsToAggregator(AggFragment, Spec->Definition, Spec->Context.SourceEntity, Spec->Context.StackCount,
	                     Effect.ModifierHandles, &Spec->ResolvedMagnitudes);
}

const FArcEffectComp_Inhibition* FindInhibitionComponent(const FArcActiveEffect& Effect)
{
	const FArcEffectSpec* Spec = Effect.Spec.GetPtr<FArcEffectSpec>();
	if (!Spec || !Spec->Definition)
	{
		return nullptr;
	}

	for (const FInstancedStruct& CompStruct : Spec->Definition->Components)
	{
		if (const FArcEffectComp_Inhibition* InhibComp = CompStruct.GetPtr<FArcEffectComp_Inhibition>())
		{
			return InhibComp;
		}
	}
	return nullptr;
}

bool ShouldInhibitEffect(const FArcActiveEffect& Effect, const FArcTagCountContainer& OwnedTags)
{
	const FArcEffectComp_Inhibition* InhibComp = FindInhibitionComponent(Effect);
	return InhibComp && OwnedTags.HasAny(InhibComp->InhibitedByTags);
}

void NotifyInhibitionChanged(FArcActiveEffect& Effect, bool bInhibited)
{
	for (FInstancedStruct& TaskStruct : Effect.Tasks)
	{
		if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
		{
			Task->OnInhibitionChanged(Effect, bInhibited);
		}
	}
}

void ExtractActiveEffects(TArray<FArcActiveEffect>& ActiveEffects,
                          TConstArrayView<int32> Indices,
                          TArray<FArcActiveEffect>& OutExtracted)
{
	if (Indices.IsEmpty())
	{
		return;
	}

	TBitArray<> RemoveMask(false, ActiveEffects.Num());
	OutExtracted.Reserve(OutExtracted.Num() + Indices.Num());
	for (const int32 Index : Indices)
	{
		RemoveMask[Index] = true;
		OutExtracted.Add(MoveTemp(ActiveEffects[Index]));
	}

	int32 WriteIndex = 0;
	for (int32 ReadIndex = 0; ReadIndex < ActiveEffects.Num(); ++ReadIndex)
	{
		if (RemoveMask[ReadIndex])
		{
			continue;
		}
		if (WriteIndex != ReadIndex)
		{
			ActiveEffects[WriteIndex] = MoveTemp(ActiveEffects[ReadIndex]);
		}
		++WriteIndex;
	}
	ActiveEffects.SetNum(WriteIndex);
}

FArcActiveEffect* FindExistingEffect(FArcEffectStackFragment& Stack, UArcEffectDefinition* Effect,
                                     FMassEntityHandle Source)
{
	const FArcEffectPolicy& Policy = Effect->StackingPolicy;

	for (FArcActiveEffect& Active : Stack.ActiveEffects)
	{
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || Spec->Definition != Effect)
		{
			continue;
		}

		if (Policy.Grouping == EArcStackGrouping::BySource && Spec->Context.SourceEntity != Source)
		{
			continue;
		}

		return &Active;
	}

	return nullptr;
}

int32 CountEffectInstances(const FArcEffectStackFragment& Stack, UArcEffectDefinition* Effect,
                           FMassEntityHandle Source)
{
	const FArcEffectPolicy& Policy = Effect->StackingPolicy;
	int32 Count = 0;

	for (const FArcActiveEffect& Active : Stack.ActiveEffects)
	{
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || Spec->Definition != Effect)
		{
			continue;
		}

		if (Policy.Grouping == EArcStackGrouping::BySource && Spec->Context.SourceEntity != Source)
		{
			continue;
		}

		++Count;
	}

	return Count;
}

// Populates an active effect entry: assigns handle, spec, timing, clones tasks, pushes modifiers.
void InitActiveEffect(FArcActiveEffect& ActiveEffect, FArcActiveEffectHandle Handle, UArcEffectDefinition* Definition,
                      const FSharedStruct& SharedSpec, const FArcEffectSpec& Spec,
                      FMassEntityHandle Target, UWorld* World,
                      FArcAggregatorFragment* AggFragment, const FArcTagCountContainer* OwnedTags)
{
	ActiveEffect.Handle = Handle;
	ActiveEffect.Spec = SharedSpec;
	ActiveEffect.bInhibited = OwnedTags && ShouldInhibitEffect(ActiveEffect, *OwnedTags);
	ActiveEffect.RemainingDuration = Spec.Duration;
	ActiveEffect.PeriodTimer = Definition->StackingPolicy.Period;
	ActiveEffect.OwnerEntity = Target;
	ActiveEffect.OwnerWorld = World;

	if (Definition->Tasks.Num() > 0)
	{
		ActiveEffect.Tasks.Reserve(Definition->Tasks.Num());
		for (const FInstancedStruct& TaskTemplate : Definition->Tasks)
		{
			FInstancedStruct& TaskCopy = ActiveEffect.Tasks.AddDefaulted_GetRef();
			TaskCopy.InitializeAs(TaskTemplate.GetScriptStruct(), TaskTemplate.GetMemory());
		}

		for (FInstancedStruct& TaskStruct : ActiveEffect.Tasks)
		{
			if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
			{
				Task->OnApplication(ActiveEffect);
			}
		}

		if (ActiveEffect.bInhibited)
		{
			NotifyInhibitionChanged(ActiveEffect, true);
		}
	}

	if (AggFragment)
	{
		RebuildEffectMods(*AggFragment, ActiveEffect);
	}
}

// Post-application sequence: runs executions, component OnApplied hook, signals, and broadcasts.
void RunApplicationEpilogue(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                            UArcEffectDefinition* Effect, const FArcEffectSpec& Spec,
                            const FSharedStruct& SharedSpec, const FArcEffectContext& Context)
{
	const FArcEffectPolicy& Policy = Effect->StackingPolicy;
	const bool bRunExecutionsNow = (Policy.Periodicity == EArcEffectPeriodicity::NonPeriodic)
		|| (Policy.PeriodicExecPolicy == EArcPeriodicExecutionPolicy::PeriodAndApplication);

	QueueEffectApplied(EntityManager, Target, SharedSpec, Context.EffectHandle, Context.StackCount);

	if (bRunExecutionsNow)
	{
		RunExecutions(EntityManager, Target, Effect->Executions, Spec, SharedSpec, EArcEffectExecutionTrigger::Application);
		if (Policy.DurationType == EArcEffectDuration::Instant || Effect->Executions.Num() > 0)
		{
			QueueEffectExecuted(EntityManager, Target, SharedSpec, Context.EffectHandle);
		}
	}

	RunComponentHook(Effect->Components,
		[&Context](const FArcEffectComponent& Comp) { Comp.OnApplied(Context); });

	SendRecalculateSignal(EntityManager, Target);
	SendPendingOpsSignal(EntityManager, Target);
}

// Applies stacking refresh/period policy to an existing active effect without changing stack count.
void RefreshDurationAndPeriod(FArcActiveEffect& Active, const FArcEffectPolicy& Policy, float IncomingDuration)
{
	switch (Policy.RefreshPolicy)
	{
	case EArcStackDurationRefresh::Refresh:
		Active.RemainingDuration = IncomingDuration;
		break;
	case EArcStackDurationRefresh::Extend:
		Active.RemainingDuration += IncomingDuration;
		break;
	case EArcStackDurationRefresh::Longer:
		Active.RemainingDuration = FMath::Max(Active.RemainingDuration, IncomingDuration);
		break;
	case EArcStackDurationRefresh::Independent:
		break;
	}

	if (Policy.PeriodPolicy == EArcStackPeriodPolicy::Reset)
	{
		Active.PeriodTimer = Policy.Period;
	}
}

} // namespace Private

void CleanupActiveEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                         FArcActiveEffect&& Effect, EArcEffectRemovalReason Reason)
{
	FArcActiveEffect Removed = MoveTemp(Effect);
	const FArcEffectSpec* Spec = Removed.Spec.GetPtr<FArcEffectSpec>();
	UArcEffectDefinition* Definition = Spec ? Spec->Definition : nullptr;
	const FMassEntityHandle Source = Spec ? Spec->Context.SourceEntity : FMassEntityHandle();

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Effect] Removed definition=%s handle=%u target=%s reason=%s"),
		Definition ? *Definition->GetName() : TEXT("<null>"),
		GetTypeHash(Removed.Handle),
		*Target.DebugGetDescription(),
		*UEnum::GetValueAsString(Reason));

	for (FInstancedStruct& TaskStruct : Removed.Tasks)
	{
		if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
		{
			Task->OnRemoved(Removed, Reason);
		}
	}

	if (Definition)
	{
		FArcEffectContext Context;
		Context.EntityManager = &EntityManager;
		Context.TargetEntity = Target;
		Context.SourceEntity = Source;
		Context.EffectDefinition = Definition;
		Context.StackCount = Spec->Context.StackCount;
		Context.SourceData = Spec->Context.SourceData;
		Context.OriginTags = Spec->Context.OriginTags;
		Context.SourceAbility = Spec->Context.SourceAbility;
		Context.SourceAbilityHandle = Spec->Context.SourceAbilityHandle;
		Context.EffectHandle = Removed.Handle;

		Private::RunComponentHook(Definition->Components,
			[&Context, Reason](const FArcEffectComponent& Comp) { Comp.OnRemoved(Context, Reason); });
	}

	// Hooks may destroy the entity or move its fragments; re-resolve before touching mods.
	if (EntityManager.IsEntityValid(Target))
	{
		FMassEntityView EntityView(EntityManager, Target);
		if (FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>())
		{
			Private::RemoveModsFromAggregator(*AggFragment, Removed.ModifierHandles);
		}
	}

	if (UArcGameEventSubsystem* EventBus = UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()))
	{
		FArcEffectRemovedPayload Payload;
		Payload.Spec = Removed.Spec;
		Payload.Handle = Removed.Handle;
		Payload.Reason = Reason;

		FArcGameEvent Event;
		if (Spec)
		{
			Event.SourceTags = Spec->CapturedSourceTags;
			Event.TargetTags = Spec->CapturedTargetTags;
		}

		Event.Tag = ArcGameEventTags::EffectRemoved;
		Event.Instigator = Source;
		Event.Target = Target;
		Event.Payload = FInstancedStruct::Make(Payload);
		EventBus->Queue(MoveTemp(Event));
	}
}

namespace ArcEffectsApply
{

bool PassesImmunity(FMassEntityManager& EntityManager, FMassEntityHandle Target, const FSharedStruct& SharedSpec)
{
	const FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
	FMassEntityView ImmunityView(EntityManager, Target);
	const FArcImmunityFragment* ImmFrag = ImmunityView.GetSparseFragmentDataPtr<FArcImmunityFragment>();
	if (!ImmFrag || ImmFrag->ImmunityTags.IsEmpty())
	{
		return true;
	}

	const FGameplayTagContainer& EffectTags = ArcEffectHelpers::GetEffectTags(Spec.Definition);
	const FGameplayTagContainer& ImmunityTags = ImmFrag->ImmunityTags.GetTagContainer();
	if (!EffectTags.HasAny(ImmunityTags))
	{
		return true;
	}

	ARCMA_VLOG(EntityManager.GetWorld(), Warning,
		TEXT("[Effect] Blocked by immunity effect=%s blocking_tags=%s target=%s"),
		*Spec.Definition->GetName(),
		*EffectTags.Filter(ImmunityTags).ToString(),
		*Target.DebugGetDescription());
	ArcEffects::Private::QueueEffectRejected(EntityManager, Target, SharedSpec, ArcGameEventTags::EffectRejectedImmunity);
	return false;
}

bool RunPreApplication(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                       const FArcEffectApplyRequest& Request, const FSharedStruct& SharedSpec)
{
	const FArcEffectSpec& Spec = SharedSpec.Get<FArcEffectSpec>();
	FArcEffectPreApplicationResult Result;
	for (const FInstancedStruct& CompStruct : Spec.Definition->Components)
	{
		const FArcEffectComponent* Comp = CompStruct.GetPtr<FArcEffectComponent>();
		if (!Comp)
		{
			continue;
		}

		Comp->OnPreApplication(Request, Spec, Result);

		if (Result.bVeto)
		{
			const FGameplayTag Reason = Result.RejectReason.IsValid() ? Result.RejectReason : ArcGameEventTags::EffectRejectedGate.GetTag();
			ARCMA_VLOG(EntityManager.GetWorld(), Warning,
				TEXT("[Effect] PreApplication blocked effect=%s reason=%s target=%s"),
				*Spec.Definition->GetName(), *Reason.ToString(), *Target.DebugGetDescription());
			ArcEffects::Private::QueueEffectRejected(EntityManager, Target, SharedSpec, Reason);
			return false;
		}
	}

	return true;
}

} // namespace ArcEffectsApply

FArcEffectContext MakeChildContext(const FArcEffectContext& Parent, const FGameplayTagContainer& ExtraOriginTags,
                                   const FGameplayTagContainer& ExcludedOriginTags)
{
	FArcEffectContext Child;
	Child.EntityManager = Parent.EntityManager;
	Child.SourceEntity = Parent.SourceEntity;
	Child.SourceData = Parent.SourceData;
	Child.SourceAbility = Parent.SourceAbility;
	Child.SourceAbilityHandle = Parent.SourceAbilityHandle;
	Child.OriginTags = Parent.OriginTags;
	Child.OriginTags.RemoveTags(ExcludedOriginTags);
	Child.OriginTags.AppendTags(ExtraOriginTags);
	return Child;
}

FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    UArcEffectDefinition* Effect, FMassEntityHandle Source,
                    const FInstancedStruct& SourceData)
{
	FArcEffectContext Context;
	Context.SourceEntity = Source;
	Context.SourceData = SourceData;
	return TryApplyEffect(EntityManager, Target, Effect, Context);
}

namespace ArcEffectsApply
{

FArcEffectApplyResult MergeIntoExisting(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                                        FArcActiveEffect& Existing, const FSharedStruct& IncomingSharedSpec,
                                        const FArcEffectContext& Context)
{
	const FArcEffectSpec& IncomingSpec = IncomingSharedSpec.Get<FArcEffectSpec>();
	FArcEffectSpec& ExistingSpec = Existing.Spec.Get<FArcEffectSpec>();
	UArcEffectDefinition* Definition = ExistingSpec.Definition;
	const FArcEffectPolicy& Policy = Definition->StackingPolicy;
	const int32 EffectiveMaxStacks = FMath::Max(1, Policy.MaxStacks);

	// Hooks may reallocate the stack: capture the handle before them and never read Existing after.
	const FArcActiveEffectHandle ExistingHandle = Existing.Handle;
	const int32 OldCount = ExistingSpec.Context.StackCount;

	if (OldCount >= EffectiveMaxStacks)
	{
		ARCMA_VLOG(EntityManager.GetWorld(), Log,
			TEXT("[Effect] Stack at max (%d) refresh-only definition=%s target=%s"),
			EffectiveMaxStacks, *Definition->GetName(), *Target.DebugGetDescription());

		Private::RefreshDurationAndPeriod(Existing, Policy, IncomingSpec.Duration);

		Private::QueueEffectApplied(EntityManager, Target, IncomingSharedSpec, ExistingHandle, OldCount);
		return FArcEffectApplyResult{true, ExistingHandle};
	}

	const int32 NewStackCount = OldCount + 1;
	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Effect] Stack %s %d->%d target=%s"),
		*Definition->GetName(), OldCount, NewStackCount, *Target.DebugGetDescription());

	ExistingSpec.Context.StackCount = NewStackCount;

	FMassEntityView EntityView(EntityManager, Target);
	if (FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>())
	{
		Private::RebuildEffectMods(*AggFragment, Existing);
	}

	Private::RefreshDurationAndPeriod(Existing, Policy, IncomingSpec.Duration);

	FArcEffectContext StackContext = Context;
	StackContext.EffectDefinition = Definition;
	StackContext.StackCount = NewStackCount;
	StackContext.EffectHandle = ExistingHandle;
	Private::RunComponentHook(Definition->Components,
		[&StackContext, OldCount](const FArcEffectComponent& Comp)
		{
			Comp.OnStackChanged(StackContext, OldCount, StackContext.StackCount);
		});

	Private::SendRecalculateSignal(EntityManager, Target);

	Private::QueueEffectApplied(EntityManager, Target, IncomingSharedSpec, ExistingHandle, NewStackCount);
	return FArcEffectApplyResult{true, ExistingHandle};
}

FArcActiveEffectHandle StoreActiveEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                                         UArcEffectDefinition* Effect, const FSharedStruct& SharedSpec,
                                         const FArcEffectSpec& Spec, FArcActiveEffectHandle Handle)
{
	FMassEntityView AddView(EntityManager, Target);
	FArcEffectStackFragment* AddStack = AddView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!AddStack)
	{
		ARCMA_VLOG(EntityManager.GetWorld(), Warning,
			TEXT("[Effect] Duration effect on entity without effect stack — applied as instant, no handle. definition=%s target=%s"),
			*Effect->GetName(), *Target.DebugGetDescription());
		return FArcActiveEffectHandle();
	}

	FArcActiveEffect& NewEffect = AddStack->ActiveEffects.AddDefaulted_GetRef();
	NewEffect.ApplicationSequence = ++AddStack->NextApplicationSequence;
	FArcAggregatorFragment* AggFragment = AddView.GetFragmentDataPtr<FArcAggregatorFragment>();
	const FArcOwnedTagsFragment* OwnedTags = AddView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();

	Private::InitActiveEffect(NewEffect, Handle, Effect, SharedSpec, Spec,
	                          Target, EntityManager.GetWorld(), AggFragment,
	                          OwnedTags ? &OwnedTags->Tags : nullptr);
	return Handle;
}

void CaptureExecutionStates(FMassEntityManager& EntityManager, FMassEntityHandle Target, UArcEffectDefinition* Effect,
                            FArcEffectSpec& Spec)
{
	Spec.ExecutionStates.Reset();
	Spec.ExecutionStates.SetNum(Effect->Executions.Num());

	FArcEffectExecutionContext ExecCtx;
	ExecCtx.OwningSpec = &Spec;
	ExecCtx.EntityManager = &EntityManager;
	ExecCtx.TargetEntity = Target;

	for (int32 ExecIdx = 0; ExecIdx < Effect->Executions.Num(); ++ExecIdx)
	{
		if (const FArcEffectExecution* Exec = Effect->Executions[ExecIdx].CustomExecution.GetPtr<FArcEffectExecution>())
		{
			Exec->CaptureApplicationState(ExecCtx, Spec.ExecutionStates[ExecIdx]);
		}
	}
}

FArcEffectApplyResult FinishApplication(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                                        UArcEffectDefinition* Effect, const FSharedStruct& SharedSpec,
                                        const FArcEffectSpec& Spec, const FArcEffectContext& Context,
                                        FArcActiveEffectHandle Handle)
{
	CaptureExecutionStates(EntityManager, Target, Effect, SharedSpec.Get<FArcEffectSpec>());

	FArcEffectContext AppliedContext = Context;
	AppliedContext.EffectHandle = Handle;
	Private::RunApplicationEpilogue(EntityManager, Target, Effect, Spec, SharedSpec, AppliedContext);

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Effect] Applied handle=%u definition=%s target=%s"),
		GetTypeHash(Handle),
		*Effect->GetName(), *Target.DebugGetDescription());

	return FArcEffectApplyResult{true, Handle};
}

FArcEffectApplyResult ApplyEffectInternal(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                                          UArcEffectDefinition* Effect, const FSharedStruct& SharedSpec,
                                          const FArcEffectSpec* SpecPtr, const FArcEffectContext& Context)
{
	const FMassEntityHandle Source = Context.SourceEntity;

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Effect] Spec built definition=%s target=%s magnitudes=%d duration=%.2f"),
		*Effect->GetName(), *Target.DebugGetDescription(),
		SpecPtr->ResolvedMagnitudes.Num(),
		SpecPtr->Duration);

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();

	if (Effect->StackingPolicy.DurationType == EArcEffectDuration::Instant)
	{
		ArcEffects::Private::RunApplicationEpilogue(EntityManager, Target, Effect, *SpecPtr, SharedSpec, Context);

		FArcActiveEffect InstantEffect;
		InstantEffect.Spec = SharedSpec;
		InstantEffect.OwnerEntity = Target;
		InstantEffect.OwnerWorld = EntityManager.GetWorld();
		InstantEffect.Tasks.Reserve(Effect->Tasks.Num());

		for (const FInstancedStruct& TaskTemplate : Effect->Tasks)
		{
			FInstancedStruct& TaskCopy = InstantEffect.Tasks.AddDefaulted_GetRef();
			TaskCopy.InitializeAs(TaskTemplate.GetScriptStruct(), TaskTemplate.GetMemory());
		}

		for (FInstancedStruct& TaskStruct : InstantEffect.Tasks)
		{
			if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
			{
				Task->OnExecute(InstantEffect);
			}
		}

		return FArcEffectApplyResult{true, FArcActiveEffectHandle()};
	}

	if (EffectStack)
	{
		const FArcEffectPolicy& Policy = Effect->StackingPolicy;

		const int32 EffectiveMaxStacks = FMath::Max(1, Policy.MaxStacks);

		if (Policy.StackType == EArcStackType::Counter)
		{
			FArcActiveEffect* Existing = Private::FindExistingEffect(*EffectStack, Effect, Source);

			if (Existing)
			{
				switch (Policy.StackPolicy)
				{
				case EArcStackPolicy::DenyNew:
					ARCMA_VLOG(EntityManager.GetWorld(), Warning,
						TEXT("[Effect] Stack policy DenyNew rejected effect=%s target=%s"),
						*Effect->GetName(), *Target.DebugGetDescription());
					Private::QueueEffectRejected(EntityManager, Target, SharedSpec, ArcGameEventTags::EffectRejectedDenyNew);
					return FArcEffectApplyResult{};

				case EArcStackPolicy::Replace:
				{
					FArcEffectRemovalQuery ReplaceQuery;
					ReplaceQuery.Handle = Existing->Handle;
					ReplaceQuery.Reason = EArcEffectRemovalReason::Replaced;
					RemoveEffects(EntityManager, Target, ReplaceQuery);
					break;
				}

				case EArcStackPolicy::Aggregate:
					return MergeIntoExisting(EntityManager, Target, *Existing, SharedSpec, Context);
				}
			}
		}
		else
		{
			const int32 InstanceCount = Private::CountEffectInstances(*EffectStack, Effect, Source);

			if (InstanceCount >= EffectiveMaxStacks)
			{
				if (Policy.OverflowPolicy == EArcStackOverflowPolicy::Deny)
				{
					ARCMA_VLOG(EntityManager.GetWorld(), Warning,
						TEXT("[Effect] Instance overflow Deny effect=%s target=%s"),
						*Effect->GetName(), *Target.DebugGetDescription());
					Private::QueueEffectRejected(EntityManager, Target, SharedSpec, ArcGameEventTags::EffectRejectedOverflow);
					return FArcEffectApplyResult{};
				}

				FArcEffectRemovalQuery OverflowQuery;
				OverflowQuery.Definition = Effect;
				if (Policy.Grouping == EArcStackGrouping::BySource)
				{
					OverflowQuery.Source = Source;
				}
				OverflowQuery.MaxCount = 1;
				OverflowQuery.Order = EArcEffectRemovalOrder::OldestFirst;
				OverflowQuery.Reason = EArcEffectRemovalReason::Overflow;
				RemoveEffects(EntityManager, Target, OverflowQuery);
			}
		}
	}

	if (!EntityManager.IsEntityValid(Target))
	{
		return FArcEffectApplyResult{};
	}

	const FArcActiveEffectHandle ResultHandle = StoreActiveEffect(EntityManager, Target, Effect, SharedSpec, *SpecPtr,
		FArcActiveEffectHandle::Generate());
	return FinishApplication(EntityManager, Target, Effect, SharedSpec, *SpecPtr, Context, ResultHandle);
}

} // namespace ArcEffectsApply

namespace ArcEffectsApply
{

FArcEffectApplyResult ApplyBuiltSpec(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                                     const FArcEffectApplyRequest& Request, const FSharedStruct& SharedSpec)
{
	const FArcEffectSpec* SpecPtr = SharedSpec.GetPtr<FArcEffectSpec>();
	if (!SpecPtr || !SpecPtr->Definition)
	{
		return FArcEffectApplyResult{};
	}

	// Immunity runs before components so an immune target never sees a pre-application side effect (rolls, events, child effects).
	if (!PassesImmunity(EntityManager, Target, SharedSpec))
	{
		return FArcEffectApplyResult{};
	}

	if (!RunPreApplication(EntityManager, Target, Request, SharedSpec))
	{
		return FArcEffectApplyResult{};
	}

	if (!EntityManager.IsEntityValid(Target))
	{
		return FArcEffectApplyResult{};
	}

	return ApplyEffectInternal(EntityManager, Target, SpecPtr->Definition, SharedSpec, SpecPtr, SpecPtr->Context);
}

} // namespace ArcEffectsApply

FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    UArcEffectDefinition* Effect, const FArcEffectContext& InContext)
{
	FArcEffectApplyRequest Request;
	Request.Definition = Effect;
	Request.Context = MakeChildContext(InContext);
	return TryApplyEffect(EntityManager, Target, Request);
}

FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    const FArcEffectApplyRequest& InRequest)
{
	if (!InRequest.Definition || !EntityManager.IsEntityValid(Target))
	{
		ARCMA_VLOG(EntityManager.GetWorld(), Warning,
			TEXT("[Effect] TryApply rejected effect=%s target=%s reason=invalid"),
			InRequest.Definition ? *InRequest.Definition->GetName() : TEXT("<null>"),
			*Target.DebugGetDescription());
		return FArcEffectApplyResult{};
	}

	FArcEffectApplyRequest Request = InRequest;
	Request.Context.EntityManager = &EntityManager;
	Request.Context.TargetEntity = Target;
	Request.Context.EffectDefinition = Request.Definition;
	Request.Context.StackCount = FMath::Max(1, Request.Context.StackCount);

	const FSharedStruct SharedSpec = ArcEffects::MakeSpec(Request);
	return ArcEffectsApply::ApplyBuiltSpec(EntityManager, Target, Request, SharedSpec);
}

FArcEffectApplyResult TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    const FSharedStruct& SpecStruct)
{
	const FArcEffectSpec* Spec = SpecStruct.GetPtr<FArcEffectSpec>();
	if (!Spec || !Spec->Definition || !EntityManager.IsEntityValid(Target))
	{
		return FArcEffectApplyResult{};
	}

	FSharedStruct AppliedSpec = FSharedStruct::Make<FArcEffectSpec>(*Spec);
	FArcEffectSpec& Applied = AppliedSpec.Get<FArcEffectSpec>();
	ArcEffects::RebindSpecTarget(Applied, EntityManager, Target);

	FArcEffectApplyRequest Request;
	Request.Definition = Applied.Definition;
	Request.Context = Applied.Context;
	Request.SetByCallerMagnitudes = Applied.SetByCallerMagnitudes;

	return ArcEffectsApply::ApplyBuiltSpec(EntityManager, Target, Request, AppliedSpec);
}

namespace ArcEffectsRemoval
{

bool MatchesQuery(const FArcActiveEffect& Active, const FArcEffectRemovalQuery& Query)
{
	if (Query.ExcludeHandle.IsValid() && Active.Handle == Query.ExcludeHandle)
	{
		return false;
	}

	if (Query.Handle.IsValid() && Active.Handle != Query.Handle)
	{
		return false;
	}

	const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();

	if (Query.Definition && (!Spec || Spec->Definition != Query.Definition))
	{
		return false;
	}

	if (Query.Source.IsSet() && (!Spec || Spec->Context.SourceEntity != Query.Source.GetValue()))
	{
		return false;
	}

	if (!Query.TagQuery.IsEmpty())
	{
		if (!Spec || !Spec->Definition)
		{
			return false;
		}
		if (!Query.TagQuery.Matches(ArcEffectHelpers::GetEffectTags(Spec->Definition)))
		{
			return false;
		}
	}

	return true;
}

} // namespace ArcEffectsRemoval

int32 RemoveEffects(FMassEntityManager& EntityManager, FMassEntityHandle Target, const FArcEffectRemovalQuery& Query)
{
	const bool bHasPredicate = Query.Definition != nullptr
		|| Query.Source.IsSet()
		|| Query.Handle.IsValid()
		|| !Query.TagQuery.IsEmpty();
	if (!bHasPredicate && !Query.bAll)
	{
		return 0;
	}

	if (!EntityManager.IsEntityValid(Target))
	{
		return 0;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return 0;
	}

	const TArray<FArcActiveEffect>& ActiveEffects = EffectStack->ActiveEffects;
	TArray<int32, TInlineAllocator<8>> Matches;
	for (int32 Index = 0; Index < ActiveEffects.Num(); ++Index)
	{
		if (ArcEffectsRemoval::MatchesQuery(ActiveEffects[Index], Query))
		{
			Matches.Add(Index);
		}
	}

	if (Matches.IsEmpty())
	{
		return 0;
	}

	const bool bOldestFirst = Query.Order == EArcEffectRemovalOrder::OldestFirst;
	Matches.Sort([&ActiveEffects, bOldestFirst](const int32 A, const int32 B) -> bool
	{
		const uint64 SequenceA = ActiveEffects[A].ApplicationSequence;
		const uint64 SequenceB = ActiveEffects[B].ApplicationSequence;
		return bOldestFirst ? SequenceA < SequenceB : SequenceA > SequenceB;
	});

	if (Query.MaxCount > 0 && Matches.Num() > Query.MaxCount)
	{
		Matches.SetNum(Query.MaxCount);
	}

	TArray<FArcActiveEffect> Extracted;
	Private::ExtractActiveEffects(EffectStack->ActiveEffects, Matches, Extracted);

	for (FArcActiveEffect& Removed : Extracted)
	{
		CleanupActiveEffect(EntityManager, Target, MoveTemp(Removed), Query.Reason);
	}

	if (EntityManager.IsEntityValid(Target))
	{
		Private::SendRecalculateSignal(EntityManager, Target);
	}

	return Extracted.Num();
}

FArcActiveEffect* FindActiveEffect(FArcEffectStackFragment& EffectStack, FArcActiveEffectHandle Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	for (FArcActiveEffect& Active : EffectStack.ActiveEffects)
	{
		if (Active.Handle == Handle)
		{
			return &Active;
		}
	}

	return nullptr;
}

int32 ConsumeStack(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                   FArcActiveEffectHandle Handle, int32 Count)
{
	if (!EntityManager.IsEntityValid(Target))
	{
		return 0;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	FArcActiveEffect* Active = EffectStack ? FindActiveEffect(*EffectStack, Handle) : nullptr;
	if (!Active || !Active->Spec.GetPtr<FArcEffectSpec>() || !Active->Spec.Get<FArcEffectSpec>().Definition)
	{
		return 0;
	}

	FArcEffectSpec* Spec = &Active->Spec.Get<FArcEffectSpec>();

	const int32 OldCount = Spec->Context.StackCount;
	if (Count <= 0)
	{
		return OldCount;
	}

	const int32 NewCount = FMath::Max(0, OldCount - Count);
	if (NewCount == 0)
	{
		FArcEffectRemovalQuery Query;
		Query.Handle = Handle;
		Query.Reason = EArcEffectRemovalReason::Removed;
		RemoveEffects(EntityManager, Target, Query);
		return 0;
	}

	UArcEffectDefinition* Definition = Spec->Definition;
	FArcEffectContext StackContext;
	StackContext.EntityManager = &EntityManager;
	StackContext.TargetEntity = Target;
	StackContext.SourceEntity = Spec->Context.SourceEntity;
	StackContext.EffectDefinition = Definition;
	StackContext.StackCount = NewCount;
	StackContext.SourceData = Spec->Context.SourceData;
	StackContext.OriginTags = Spec->Context.OriginTags;
	StackContext.SourceAbility = Spec->Context.SourceAbility;
	StackContext.SourceAbilityHandle = Spec->Context.SourceAbilityHandle;
	StackContext.EffectHandle = Handle;

	Spec->Context.StackCount = NewCount;
	if (FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>())
	{
		Private::RebuildEffectMods(*AggFragment, *Active);
	}

	ARCMA_VLOG(EntityManager.GetWorld(), Log,
		TEXT("[Effect] Stack consumed %s %d->%d target=%s"),
		*Definition->GetName(), OldCount, NewCount, *Target.DebugGetDescription());

	Private::RunComponentHook(Definition->Components,
		[&StackContext, OldCount](const FArcEffectComponent& Comp)
		{
			Comp.OnStackChanged(StackContext, OldCount, StackContext.StackCount);
		});

	if (EntityManager.IsEntityValid(Target))
	{
		Private::SendRecalculateSignal(EntityManager, Target);
	}

	return NewCount;
}

bool SetEffectDecayRate(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                        FArcActiveEffectHandle Handle, float Rate)
{
	if (!EntityManager.IsEntityValid(Target))
	{
		return false;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	FArcActiveEffect* Active = EffectStack ? FindActiveEffect(*EffectStack, Handle) : nullptr;
	if (!Active)
	{
		return false;
	}

	Active->DecayRate = FMath::Max(0.f, Rate);
	return true;
}

float ResolveScalableMagnitude(const FArcScalableFloat& Mag, EArcRangeResolution Mode)
{
	switch (Mode)
	{
	case EArcRangeResolution::Min: return Mag.GetRangeMin();
	case EArcRangeResolution::Max: return Mag.GetRangeMax();
	case EArcRangeResolution::Mid: return Mag.GetRangeMid();
	case EArcRangeResolution::Roll:
	default:                       return Mag.GetValue();
	}
}

} // namespace ArcEffects
