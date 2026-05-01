// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcAttributeCapture.h"
#include "Effects/ArcMagnitudeCalculation.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcImmunityFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Attributes/ArcAttributeNotification.h"
#include "StructUtils/SharedStruct.h"
#include "MassEntityManager.h"
#include "MassEntityView.h"
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

bool RunPreApplicationGate(const TArray<FInstancedStruct>& Components, const FArcEffectContext& Context)
{
	for (const FInstancedStruct& CompStruct : Components)
	{
		if (CompStruct.IsValid())
		{
			if (const FArcEffectComponent* Comp = CompStruct.GetPtr<FArcEffectComponent>())
			{
				if (!Comp->OnPreApplication(Context))
				{
					return false;
				}
			}
		}
	}
	return true;
}

void SendRecalculateSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::AttributeRecalculate, Entity);
	}
}

void SendPendingOpsSignal(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(UE::ArcMassAbilities::Signals::PendingAttributeOps, Entity);
	}
}

void RunExecutions(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                   const TArray<FArcEffectExecutionEntry>& Executions,
                   const FArcEffectSpec& Spec,
                   const FSharedStruct& SpecStruct,
                   const FGameplayTagContainer& PassedInTags)
{
	FMassEntityView EntityView(EntityManager, Target);

	FArcEffectExecutionContext ExecCtx;
	ExecCtx.OwningSpec = &Spec;
	ExecCtx.EntityManager = &EntityManager;
	ExecCtx.TargetEntity = Target;
	ExecCtx.PassedInTags = PassedInTags;

	FArcPendingAttributeOpsFragment* Pending = EntityView.GetFragmentDataPtr<FArcPendingAttributeOpsFragment>();

	for (const FArcEffectExecutionEntry& Entry : Executions)
	{
		if (Entry.CustomExecution.IsValid())
		{
			const FArcEffectExecution* Exec = Entry.CustomExecution.GetPtr<FArcEffectExecution>();
			if (!Exec)
			{
				continue;
			}

			TArray<FArcEffectModifierOutput> Outputs;
			const_cast<FArcEffectExecution*>(Exec)->Execute(ExecCtx, Outputs);

			if (Pending)
			{
				for (const FArcEffectModifierOutput& Output : Outputs)
				{
					if (!Output.Attribute.IsValid())
					{
						continue;
					}
					FArcPendingAttributeOp& PendingOp = Pending->PendingOps.AddDefaulted_GetRef();
					PendingOp.Attribute = Output.Attribute;
					PendingOp.Operation = Output.Operation;
					PendingOp.Magnitude = Output.Magnitude;
					PendingOp.OpType = EArcAttributeOpType::Execute;
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
					ResolvedMag = Mod.Magnitude.GetValue();
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
                          const TArray<float>* ResolvedMagnitudes = nullptr)
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
			: Modifier.Magnitude.GetValue();
		float EffectiveMag = BaseMag;

		switch (Modifier.Operation)
		{
		case EArcModifierOp::Add:
		case EArcModifierOp::AddFinal:
		case EArcModifierOp::MultiplyAdditive:
		case EArcModifierOp::DivideAdditive:
			EffectiveMag = BaseMag * StackCount;
			break;
		case EArcModifierOp::MultiplyCompound:
			EffectiveMag = FMath::Pow(BaseMag, static_cast<float>(StackCount));
			break;
		case EArcModifierOp::Override:
			EffectiveMag = BaseMag;
			break;
		}

		FArcAggregator& Agg = AggFragment.FindOrAddAggregator(Modifier.Attribute);
		FArcModifierHandle Handle = Agg.AddMod(
			Modifier.Operation,
			EffectiveMag,
			Source,
			nullptr,
			&Modifier.TagFilter,
			Modifier.Channel);
		OutHandles.Add(Handle);
	}
}

void RemoveModsFromAggregator(FArcAggregatorFragment& AggFragment,
                              const TArray<FArcModifierHandle>& Handles)
{
	for (const FArcModifierHandle& Handle : Handles)
	{
		for (TPair<FArcAttributeRef, FArcAggregator>& Pair : AggFragment.Aggregators)
		{
			Pair.Value.RemoveMod(Handle);
		}
	}
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

int32 FindOldestEffectIndex(const FArcEffectStackFragment& Stack, UArcEffectDefinition* Effect,
                            FMassEntityHandle Source)
{
	const FArcEffectPolicy& Policy = Effect->StackingPolicy;

	for (int32 Index = 0; Index < Stack.ActiveEffects.Num(); ++Index)
	{
		const FArcActiveEffect& Active = Stack.ActiveEffects[Index];
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || Spec->Definition != Effect)
		{
			continue;
		}

		if (Policy.Grouping == EArcStackGrouping::BySource && Spec->Context.SourceEntity != Source)
		{
			continue;
		}

		return Index;
	}

	return INDEX_NONE;
}

// Populates an active effect entry: assigns handle, spec, timing, clones tasks, pushes modifiers.
void InitActiveEffect(FArcActiveEffect& ActiveEffect, UArcEffectDefinition* Definition,
                      const FSharedStruct& SharedSpec, const FArcEffectSpec& Spec,
                      FMassEntityHandle Target, UWorld* World, FMassEntityHandle Source,
                      FArcAggregatorFragment* AggFragment)
{
	ActiveEffect.Handle = FArcActiveEffectHandle::Generate();
	ActiveEffect.Spec = SharedSpec;
	ActiveEffect.RemainingDuration = Definition->StackingPolicy.Duration;
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
	}

	if (AggFragment)
	{
		PushModsToAggregator(*AggFragment, Definition, Source, 1, ActiveEffect.ModifierHandles,
		                     &Spec.ResolvedMagnitudes);
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

	if (bRunExecutionsNow)
	{
		RunExecutions(EntityManager, Target, Effect->Executions, Spec, SharedSpec);
	}

	RunComponentHook(Effect->Components,
		[&Context](const FArcEffectComponent& Comp) { Comp.OnApplied(Context); });

	SendRecalculateSignal(EntityManager, Target);
	SendPendingOpsSignal(EntityManager, Target);

	UArcEffectEventSubsystem* EventSys = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
	if (EventSys)
	{
		EventSys->BroadcastEffectApplied(Target, Effect);
	}
}

// Applies stacking refresh/period policy to an existing active effect without changing stack count.
void RefreshDurationAndPeriod(FArcActiveEffect& Active, const FArcEffectPolicy& Policy)
{
	switch (Policy.RefreshPolicy)
	{
	case EArcStackDurationRefresh::Refresh:
		Active.RemainingDuration = Policy.Duration;
		break;
	case EArcStackDurationRefresh::Extend:
		Active.RemainingDuration += Policy.Duration;
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

// Tears down a single active effect — runs OnRemoved on tasks and components, removes modifiers.
// Does NOT remove the effect from the ActiveEffects array or send recalculate signals.
void CleanupActiveEffect(FArcActiveEffect& Active, FArcAggregatorFragment* AggFragment,
                         FMassEntityManager& EntityManager, FMassEntityHandle Target)
{
	const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
	UArcEffectDefinition* Effect = Spec ? Spec->Definition : nullptr;

	for (FInstancedStruct& TaskStruct : Active.Tasks)
	{
		if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
		{
			Task->OnRemoved(Active);
		}
	}

	if (Effect)
	{
		FArcEffectContext Context;
		Context.EntityManager = &EntityManager;
		Context.TargetEntity = Target;
		Context.SourceEntity = Spec->Context.SourceEntity;
		Context.EffectDefinition = Effect;
		Context.StackCount = Spec->Context.StackCount;
		Context.SourceData = Spec->Context.SourceData;

		Private::RunComponentHook(Effect->Components,
			[&Context](const FArcEffectComponent& Comp) { Comp.OnRemoved(Context); });
	}

	if (AggFragment)
	{
		Private::RemoveModsFromAggregator(*AggFragment, Active.ModifierHandles);
	}
}

FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    UArcEffectDefinition* Effect, FMassEntityHandle Source,
                    const FInstancedStruct& SourceData)
{
	FArcEffectContext Context;
	Context.SourceEntity = Source;
	Context.SourceData = SourceData;
	return TryApplyEffect(EntityManager, Target, Effect, Context);
}

FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    UArcEffectDefinition* Effect, const FArcEffectContext& InContext)
{
	// --- Gating: validate inputs, check immunity, run pre-application gate, build spec ---
	if (!Effect || !EntityManager.IsEntityValid(Target))
	{
		return FArcActiveEffectHandle();
	}

	{
		FMassEntityView ImmunityView(EntityManager, Target);
		const FArcImmunityFragment* ImmFrag = ImmunityView.GetFragmentDataPtr<FArcImmunityFragment>();
		if (ImmFrag && ImmFrag->ImmunityTags.Num() > 0)
		{
			FGameplayTagContainer EffectTags = ArcEffectHelpers::GetEffectTags(Effect);
			if (EffectTags.HasAny(ImmFrag->ImmunityTags))
			{
				return FArcActiveEffectHandle();
			}
		}
	}

	const FMassEntityHandle Source = InContext.SourceEntity;

	FArcEffectContext Context;
	Context.EntityManager = &EntityManager;
	Context.TargetEntity = Target;
	Context.SourceEntity = Source;
	Context.EffectDefinition = Effect;
	Context.StackCount = 1;
	Context.SourceData = InContext.SourceData;
	Context.SourceAbility = InContext.SourceAbility;
	Context.SourceAbilityHandle = InContext.SourceAbilityHandle;

	if (!Private::RunPreApplicationGate(Effect->Components, Context))
	{
		return FArcActiveEffectHandle();
	}

	FSharedStruct SharedSpec = ArcEffects::MakeSpec(Effect, Context);
	const FArcEffectSpec* SpecPtr = SharedSpec.GetPtr<FArcEffectSpec>();
	if (!SpecPtr)
	{
		return FArcActiveEffectHandle();
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();

	// --- Instant effects: run executions and hooks, no active effect stored ---
	if (Effect->StackingPolicy.DurationType == EArcEffectDuration::Instant)
	{
		Private::RunApplicationEpilogue(EntityManager, Target, Effect, *SpecPtr, SharedSpec, Context);
		return FArcActiveEffectHandle::Generate();
	}

	// --- Duration/Infinite/Periodic: resolve stacking against existing effects ---
	if (EffectStack)
	{
		const FArcEffectPolicy& Policy = Effect->StackingPolicy;
		const int32 EffectiveMaxStacks = FMath::Max(1, Policy.MaxStacks);

		// Counter stacking: one logical entry per effect, stack count tracked on spec
		if (Policy.StackType == EArcStackType::Counter)
		{
			FArcActiveEffect* Existing = Private::FindExistingEffect(*EffectStack, Effect, Source);

			if (Existing)
			{
				const FArcEffectSpec* ExistingSpec = Existing->Spec.GetPtr<FArcEffectSpec>();

				switch (Policy.StackPolicy)
				{
				case EArcStackPolicy::DenyNew:
					return FArcActiveEffectHandle();

				case EArcStackPolicy::Replace:
				{
					FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
					CleanupActiveEffect(*Existing, AggFragment, EntityManager, Target);
					Existing->ModifierHandles.Empty();
					Existing->Tasks.Empty();

					Private::InitActiveEffect(*Existing, Effect, SharedSpec, *SpecPtr,
					                          Target, EntityManager.GetWorld(), Source, AggFragment);

					Private::RunApplicationEpilogue(EntityManager, Target, Effect, *SpecPtr, SharedSpec, Context);
					return Existing->Handle;
				}

				case EArcStackPolicy::Aggregate:
				{
					const int32 CurrentStackCount = ExistingSpec ? ExistingSpec->Context.StackCount : 1;

					// At max stacks: refresh duration/period only, no modifier change
					if (CurrentStackCount >= EffectiveMaxStacks)
					{
						Private::RefreshDurationAndPeriod(*Existing, Policy);

						UArcEffectEventSubsystem* EventSys = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
						if (EventSys)
						{
							EventSys->BroadcastEffectApplied(Target, Effect);
						}
						return Existing->Handle;
					}

					// Below max: increment stack count, rebuild modifiers at new count
					const int32 OldCount = CurrentStackCount;
					const int32 NewStackCount = OldCount + 1;

					FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
					if (AggFragment)
					{
						Private::RemoveModsFromAggregator(*AggFragment, Existing->ModifierHandles);
					}
					Existing->ModifierHandles.Empty();

					Existing->Spec.Get<FArcEffectSpec>().Context.StackCount = NewStackCount;

					if (AggFragment)
					{
						Private::PushModsToAggregator(*AggFragment, Effect, Source, NewStackCount,
						                              Existing->ModifierHandles,
						                              &Existing->Spec.GetPtr<FArcEffectSpec>()->ResolvedMagnitudes);
					}

					Private::RefreshDurationAndPeriod(*Existing, Policy);

					FArcEffectContext StackContext = Context;
					StackContext.StackCount = NewStackCount;
					Private::RunComponentHook(Effect->Components,
						[&StackContext, OldCount](const FArcEffectComponent& Comp)
						{
							Comp.OnStackChanged(StackContext, OldCount, StackContext.StackCount);
						});

					Private::SendRecalculateSignal(EntityManager, Target);

					UArcEffectEventSubsystem* EventSys = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
					if (EventSys)
					{
						EventSys->BroadcastEffectApplied(Target, Effect);
					}
					return Existing->Handle;
				}
				}
			}
		}
		else // Instance stacking: each application creates a separate entry
		{
			const int32 InstanceCount = Private::CountEffectInstances(*EffectStack, Effect, Source);

			if (InstanceCount >= EffectiveMaxStacks)
			{
				if (Policy.OverflowPolicy == EArcStackOverflowPolicy::Deny)
				{
					return FArcActiveEffectHandle();
				}

				const int32 OldestIndex = Private::FindOldestEffectIndex(*EffectStack, Effect, Source);
				if (OldestIndex != INDEX_NONE)
				{
					FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
					CleanupActiveEffect(EffectStack->ActiveEffects[OldestIndex], AggFragment, EntityManager, Target);
					EffectStack->ActiveEffects.RemoveAt(OldestIndex);
				}
			}
		}
	}

	// --- Add new active effect entry (reached by first-application and Instance stacking) ---
	FArcActiveEffectHandle ResultHandle;
	if (EffectStack)
	{
		FArcActiveEffect& NewEffect = EffectStack->ActiveEffects.AddDefaulted_GetRef();
		FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

		Private::InitActiveEffect(NewEffect, Effect, SharedSpec, *SpecPtr,
		                          Target, EntityManager.GetWorld(), Source, AggFragment);
		ResultHandle = NewEffect.Handle;
	}

	Private::RunApplicationEpilogue(EntityManager, Target, Effect, *SpecPtr, SharedSpec, Context);
	return ResultHandle;
}

FArcActiveEffectHandle TryApplyEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                    const FSharedStruct& SpecStruct)
{
	const FArcEffectSpec* Spec = SpecStruct.GetPtr<FArcEffectSpec>();
	if (!Spec || !Spec->Definition)
	{
		return FArcActiveEffectHandle();
	}

	FArcEffectContext Context = Spec->Context;
	Context.EffectDefinition = Spec->Definition;

	FArcActiveEffectHandle Handle = TryApplyEffect(EntityManager, Target, Spec->Definition, Context);

	if (Handle)
	{
		FMassEntityView EntityView(EntityManager, Target);
		FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
		if (EffectStack)
		{
			FArcActiveEffect* Active = FindActiveEffect(*EffectStack, Handle);
			if (Active)
			{
				Active->Spec = SpecStruct;
			}
		}
	}

	return Handle;
}

void RemoveEffect(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                  UArcEffectDefinition* Effect)
{
	if (!Effect || !EntityManager.IsEntityValid(Target))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return;
	}

	FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	TArray<int32, TInlineAllocator<4>> IndicesToRemove;

	for (int32 Index = 0; Index < EffectStack->ActiveEffects.Num(); ++Index)
	{
		FArcActiveEffect& Active = EffectStack->ActiveEffects[Index];
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || Spec->Definition != Effect)
		{
			continue;
		}

		CleanupActiveEffect(Active, AggFragment, EntityManager, Target);
		IndicesToRemove.Add(Index);
	}

	if (IndicesToRemove.IsEmpty())
	{
		return;
	}

	for (int32 RemoveIdx = IndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
	{
		EffectStack->ActiveEffects.RemoveAtSwap(IndicesToRemove[RemoveIdx]);
	}

	Private::SendRecalculateSignal(EntityManager, Target);
}

void RemoveEffectBySource(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                          UArcEffectDefinition* Effect, FMassEntityHandle Source)
{
	if (!Effect || !EntityManager.IsEntityValid(Target))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return;
	}

	FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	TArray<int32, TInlineAllocator<4>> IndicesToRemove;

	for (int32 Index = 0; Index < EffectStack->ActiveEffects.Num(); ++Index)
	{
		FArcActiveEffect& Active = EffectStack->ActiveEffects[Index];
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || Spec->Definition != Effect || Spec->Context.SourceEntity != Source)
		{
			continue;
		}

		CleanupActiveEffect(Active, AggFragment, EntityManager, Target);
		IndicesToRemove.Add(Index);
	}

	if (IndicesToRemove.IsEmpty())
	{
		return;
	}

	for (int32 RemoveIdx = IndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
	{
		EffectStack->ActiveEffects.RemoveAtSwap(IndicesToRemove[RemoveIdx]);
	}

	Private::SendRecalculateSignal(EntityManager, Target);
}

void RemoveAllEffects(FMassEntityManager& EntityManager, FMassEntityHandle Target)
{
	if (!EntityManager.IsEntityValid(Target))
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return;
	}

	FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();

	for (int32 Index = EffectStack->ActiveEffects.Num() - 1; Index >= 0; --Index)
	{
		CleanupActiveEffect(EffectStack->ActiveEffects[Index], AggFragment, EntityManager, Target);
	}

	EffectStack->ActiveEffects.Empty();
	Private::SendRecalculateSignal(EntityManager, Target);
}

void RemoveEffectsByTag(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                        const FGameplayTagContainer& Tags)
{
	if (!EntityManager.IsEntityValid(Target) || Tags.IsEmpty())
	{
		return;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return;
	}

	FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	TArray<int32, TInlineAllocator<4>> IndicesToRemove;

	for (int32 Index = 0; Index < EffectStack->ActiveEffects.Num(); ++Index)
	{
		FArcActiveEffect& Active = EffectStack->ActiveEffects[Index];
		const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();
		if (!Spec || !Spec->Definition)
		{
			continue;
		}

		FGameplayTagContainer EffectTags = ArcEffectHelpers::GetEffectTags(Spec->Definition);
		if (!EffectTags.HasAny(Tags))
		{
			continue;
		}

		CleanupActiveEffect(Active, AggFragment, EntityManager, Target);
		IndicesToRemove.Add(Index);
	}

	if (IndicesToRemove.IsEmpty())
	{
		return;
	}

	for (int32 RemoveIdx = IndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
	{
		EffectStack->ActiveEffects.RemoveAtSwap(IndicesToRemove[RemoveIdx]);
	}

	Private::SendRecalculateSignal(EntityManager, Target);
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

bool RemoveEffectByHandle(FMassEntityManager& EntityManager, FMassEntityHandle Target,
                          FArcActiveEffectHandle Handle)
{
	if (!Handle.IsValid() || !EntityManager.IsEntityValid(Target))
	{
		return false;
	}

	FMassEntityView EntityView(EntityManager, Target);
	FArcEffectStackFragment* EffectStack = EntityView.GetFragmentDataPtr<FArcEffectStackFragment>();
	if (!EffectStack)
	{
		return false;
	}

	int32 FoundIndex = INDEX_NONE;
	for (int32 Index = 0; Index < EffectStack->ActiveEffects.Num(); ++Index)
	{
		if (EffectStack->ActiveEffects[Index].Handle == Handle)
		{
			FoundIndex = Index;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		return false;
	}

	FArcAggregatorFragment* AggFragment = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
	CleanupActiveEffect(EffectStack->ActiveEffects[FoundIndex], AggFragment, EntityManager, Target);
	EffectStack->ActiveEffects.RemoveAtSwap(FoundIndex);
	Private::SendRecalculateSignal(EntityManager, Target);

	return true;
}

} // namespace ArcEffects
