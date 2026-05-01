// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectDurationProcessor.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttribute.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassEntityView.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcAttributeCapture.h"
#include "Events/ArcEffectEventSubsystem.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

UArcEffectDurationProcessor::UArcEffectDurationProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcEffectDurationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcEffectDurationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEffectDuration);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	struct FPeriodicWork
	{
		FMassEntityHandle Entity;
		FSharedStruct Spec;
		UArcEffectDefinition* Definition;
	};

	struct FExpiredWork
	{
		FMassEntityHandle Entity;
		FSharedStruct Spec;
		UArcEffectDefinition* Definition;
		TArray<FArcModifierHandle> ModifierHandles;
		TArray<FInstancedStruct> Tasks;
		FMassEntityHandle OwnerEntity;
		TObjectPtr<UWorld> OwnerWorld = nullptr;
	};

	TArray<FPeriodicWork> PeriodicWork;
	TArray<FExpiredWork> ExpiredWork;
	TArray<FMassEntityHandle> ChangedEntities;

	EntityQuery.ForEachEntityChunk(Context, [DeltaTime, &PeriodicWork, &ExpiredWork, &ChangedEntities, &EntityManager](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcEffectStackFragment> EffectStacks = Ctx.GetMutableFragmentView<FArcEffectStackFragment>();
		TArrayView<FArcAggregatorFragment> AggregatorFragments = Ctx.GetMutableFragmentView<FArcAggregatorFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcEffectStackFragment& Stack = EffectStacks[EntityIt];
			FArcAggregatorFragment& AggFrag = AggregatorFragments[EntityIt];
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			bool bChanged = false;

			TArray<int32, TInlineAllocator<4>> IndicesToRemove;

			for (int32 Index = 0; Index < Stack.ActiveEffects.Num(); ++Index)
			{
				FArcActiveEffect& Active = Stack.ActiveEffects[Index];
				const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();

				if (!Spec || !Spec->Definition)
				{
					for (const FArcModifierHandle& Handle : Active.ModifierHandles)
					{
						for (TPair<FArcAttributeRef, FArcAggregator>& AggPair : AggFrag.Aggregators)
						{
							AggPair.Value.RemoveMod(Handle);
						}
					}
					IndicesToRemove.Add(Index);
					continue;
				}

				const FArcEffectPolicy& Policy = Spec->Definition->StackingPolicy;

				if (Policy.Periodicity == EArcEffectPeriodicity::Periodic && Policy.Period > 0.f && !Active.bInhibited)
				{
					Active.PeriodTimer -= DeltaTime;
					if (Active.PeriodTimer <= 0.f)
					{
						Active.PeriodTimer += Policy.Period;

						FPeriodicWork& Work = PeriodicWork.AddDefaulted_GetRef();
						Work.Entity = EntityHandle;
						Work.Spec = Active.Spec;
						Work.Definition = Spec->Definition;

						bChanged = true;
					}
				}

				if (Policy.DurationType == EArcEffectDuration::Duration)
				{
					Active.RemainingDuration -= DeltaTime;
					if (Active.RemainingDuration <= 0.f)
					{
						FExpiredWork& Work = ExpiredWork.AddDefaulted_GetRef();
						Work.Entity = EntityHandle;
						Work.Spec = Active.Spec;
						Work.Definition = Spec->Definition;
						Work.ModifierHandles = MoveTemp(Active.ModifierHandles);
						Work.Tasks = MoveTemp(Active.Tasks);
						Work.OwnerEntity = Active.OwnerEntity;
						Work.OwnerWorld = Active.OwnerWorld;

						IndicesToRemove.Add(Index);
						bChanged = true;
					}
				}
			}

			for (int32 RemoveIdx = IndicesToRemove.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
			{
				Stack.ActiveEffects.RemoveAtSwap(IndicesToRemove[RemoveIdx]);
			}

			if (bChanged)
			{
				ChangedEntities.Add(EntityHandle);
			}
		}
	});

	// Pass 1 — periodic executions
	for (const FPeriodicWork& Work : PeriodicWork)
	{
		const FArcEffectSpec* SpecPtr = Work.Spec.GetPtr<FArcEffectSpec>();
		if (!SpecPtr)
		{
			continue;
		}

		ArcEffects::Private::RunExecutions(EntityManager, Work.Entity,
		                                    Work.Definition->Executions,
		                                    *SpecPtr,
		                                    Work.Spec);

		FArcEffectContext TickCtx;
		TickCtx.EntityManager = &EntityManager;
		TickCtx.TargetEntity = Work.Entity;
		TickCtx.SourceEntity = SpecPtr->Context.SourceEntity;
		TickCtx.EffectDefinition = Work.Definition;
		TickCtx.StackCount = SpecPtr->Context.StackCount;
		TickCtx.SourceData = SpecPtr->Context.SourceData;
		TickCtx.SourceAbility = SpecPtr->Context.SourceAbility;
		TickCtx.SourceAbilityHandle = SpecPtr->Context.SourceAbilityHandle;

		ArcEffects::Private::RunComponentHook(Work.Definition->Components,
			[&TickCtx](const FArcEffectComponent& Comp) { Comp.OnExecuted(TickCtx); });

		if (EntityManager.IsEntityValid(Work.Entity))
		{
			FMassEntityView PeriodView(EntityManager, Work.Entity);
			FArcEffectStackFragment* PeriodStack = PeriodView.GetFragmentDataPtr<FArcEffectStackFragment>();
			if (PeriodStack)
			{
				for (FArcActiveEffect& Active : PeriodStack->ActiveEffects)
				{
					const FArcEffectSpec* ActiveSpec = Active.Spec.GetPtr<FArcEffectSpec>();
					if (ActiveSpec && ActiveSpec->Definition == Work.Definition
						&& ActiveSpec->Context.SourceEntity == SpecPtr->Context.SourceEntity)
					{
						for (FInstancedStruct& TaskStruct : Active.Tasks)
						{
							if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
							{
								Task->OnExecute(Active);
							}
						}
						break;
					}
				}
			}
		}

		UArcEffectEventSubsystem* EventSys = UWorld::GetSubsystem<UArcEffectEventSubsystem>(EntityManager.GetWorld());
		if (EventSys)
		{
			EventSys->BroadcastEffectExecuted(Work.Entity, Work.Definition);
		}
	}

	// Pass 2 — expired effect removal
	for (FExpiredWork& Work : ExpiredWork)
	{
		const FArcEffectSpec* SpecPtr = Work.Spec.GetPtr<FArcEffectSpec>();

		FArcActiveEffect TempForTasks;
		TempForTasks.Spec = Work.Spec;
		TempForTasks.OwnerEntity = Work.OwnerEntity;
		TempForTasks.OwnerWorld = Work.OwnerWorld;
		TempForTasks.Tasks = MoveTemp(Work.Tasks);

		for (FInstancedStruct& TaskStruct : TempForTasks.Tasks)
		{
			if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
			{
				Task->OnRemoved(TempForTasks);
			}
		}

		FArcEffectContext RemoveCtx;
		RemoveCtx.EntityManager = &EntityManager;
		RemoveCtx.TargetEntity = Work.Entity;
		RemoveCtx.SourceEntity = SpecPtr ? SpecPtr->Context.SourceEntity : FMassEntityHandle();
		RemoveCtx.EffectDefinition = Work.Definition;
		RemoveCtx.StackCount = SpecPtr ? SpecPtr->Context.StackCount : 0;
		RemoveCtx.SourceData = SpecPtr ? SpecPtr->Context.SourceData : FInstancedStruct();

		ArcEffects::Private::RunComponentHook(Work.Definition->Components,
			[&RemoveCtx](const FArcEffectComponent& Comp) { Comp.OnRemoved(RemoveCtx); });

		FMassEntityView EntityView(EntityManager, Work.Entity);
		FArcAggregatorFragment* AggFrag = EntityView.GetFragmentDataPtr<FArcAggregatorFragment>();
		if (AggFrag)
		{
			ArcEffects::Private::RemoveModsFromAggregator(*AggFrag, Work.ModifierHandles);
		}
	}

	// Pass 3 — batch signal
	if (ChangedEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::AttributeRecalculate, ChangedEntities);
			SignalSubsystem->SignalEntities(UE::ArcMassAbilities::Signals::PendingAttributeOps, ChangedEntities);
		}
	}
}
