// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcEffectDurationProcessor.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectComponent.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectTask_StateTree.h"
#include "Effects/ArcEffectContext.h"
#include "Effects/ArcEffectSpec.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Signals/ArcAttributeRecalculateSignal.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Attributes/ArcAggregator.h"
#include "Attributes/ArcAttribute.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Mass/EntityView.h"
#include "Effects/ArcEffectExecution.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcAttributeCapture.h"
#include "Events/ArcGameEventSubsystem.h"
#include "Debugger/ArcMassAbilityTrace.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"
#include "Schema/ArcEffectStateTreeSchema.h"
#include "MassStateTreeDependency.h"
#include "MassCoreConcepts.h"
#include "StateTree.h"
#include "UObject/UObjectIterator.h"
#include "ArcMassAbilitiesLogs.h"

namespace ArcEffectDurationProcessor_Internal
{
	void ApplyDependency(FMassEntityQuery& Query, const FMassStateTreeDependency& Dep)
	{
		if (!Dep.Type)
		{
			return;
		}

		const UScriptStruct* AsStruct = Cast<const UScriptStruct>(Dep.Type.Get());
		if (AsStruct && UE::Mass::IsA<FMassFragment>(Dep.Type))
		{
			Query.AddRequirement(AsStruct, Dep.Access, EMassFragmentPresence::Optional);
		}
		else if (AsStruct && UE::Mass::IsA<FMassTag>(Dep.Type))
		{
			// Tags are not filtered on from the processor side — skip.
			UE_LOGF(LogArcMassAbilities, Log,"ArcEffectDurationProcessor: skipping tag dependency %ls", *Dep.Type->GetName());
		}
		else if (AsStruct && UE::Mass::IsA<FMassChunkFragment>(Dep.Type))
		{
			Query.AddChunkRequirement(AsStruct, Dep.Access, EMassFragmentPresence::Optional);
		}
		else if (AsStruct && UE::Mass::IsA<FMassSharedFragment>(Dep.Type))
		{
			Query.AddSharedRequirement(AsStruct, Dep.Access, EMassFragmentPresence::Optional);
		}
		else if (AsStruct && UE::Mass::IsA<FMassConstSharedFragment>(Dep.Type))
		{
			Query.AddConstSharedRequirement(AsStruct, EMassFragmentPresence::Optional);
		}
		else if (Dep.Type->IsChildOf(USubsystem::StaticClass()))
		{
			TSubclassOf<USubsystem> SubsystemClass = Cast<UClass>(const_cast<UStruct*>(Dep.Type.Get()));
			Query.AddSubsystemRequirement(SubsystemClass, Dep.Access);
		}
		else
		{
			UE_LOGF(LogArcMassAbilities, Warning,"ArcEffectDurationProcessor: unhandled tree dependency type %ls", *Dep.Type->GetName());
		}
	}
} // namespace ArcEffectDurationProcessor_Internal

UArcEffectDurationProcessor::UArcEffectDurationProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	bRequiresGameThreadExecution = true;
}

void UArcEffectDurationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);

	TSet<const UStruct*> SeenDependencies;

	for (TObjectIterator<UArcEffectDefinition> It; It; ++It)
	{
		UArcEffectDefinition* Definition = *It;
		if (!Definition || Definition->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}

		for (const FInstancedStruct& TaskStruct : Definition->Tasks)
		{
			const FArcEffectTask_StateTree* TreeTask = TaskStruct.GetPtr<FArcEffectTask_StateTree>();
			if (!TreeTask)
			{
				continue;
			}

			const UStateTree* Tree = TreeTask->StateTreeReference.GetStateTree();
			const UArcEffectStateTreeSchema* Schema = Tree ? Cast<UArcEffectStateTreeSchema>(Tree->GetSchema()) : nullptr;
			if (!Schema)
			{
				continue;
			}

			for (const FMassStateTreeDependency& Dep : Schema->GetDependencies())
			{
				if (!Dep.Type)
				{
					continue;
				}

				bool bAlreadyInSet = false;
				SeenDependencies.Add(Dep.Type.Get(), &bAlreadyInSet);
				if (bAlreadyInSet)
				{
					continue;
				}

				ArcEffectDurationProcessor_Internal::ApplyDependency(EntityQuery, Dep);
			}
		}
	}

	// NOTE: dependencies from effect definitions loaded after ConfigureQueries (streamed-in
	// asset packs) won't widen this query. Acceptable for v1 — most effect assets are
	// reachable from the AssetManager at boot.
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
		FArcActiveEffectHandle Handle;
	};

	struct FRemovedWork
	{
		FMassEntityHandle Entity;
		FArcActiveEffect Effect;
		EArcEffectRemovalReason Reason = EArcEffectRemovalReason::Expired;
	};

	TArray<FPeriodicWork> PeriodicWork;
	TArray<FRemovedWork> RemovedWork;
	TArray<FMassEntityHandle> ChangedEntities;

	EntityQuery.ForEachEntityChunk(Context, [DeltaTime, &PeriodicWork, &RemovedWork, &ChangedEntities](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcEffectStackFragment> EffectStacks = Ctx.GetMutableFragmentView<FArcEffectStackFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcEffectStackFragment& Stack = EffectStacks[EntityIt];
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);
			bool bChanged = false;

			TArray<int32, TInlineAllocator<4>> IndicesToRemove;
			TArray<EArcEffectRemovalReason, TInlineAllocator<4>> RemovalReasons;

			for (int32 Index = 0; Index < Stack.ActiveEffects.Num(); ++Index)
			{
				FArcActiveEffect& Active = Stack.ActiveEffects[Index];
				const FArcEffectSpec* Spec = Active.Spec.GetPtr<FArcEffectSpec>();

				if (!Spec || !Spec->Definition)
				{
					ARCMA_VLOG(Ctx.GetWorld(), Warning,
						TEXT("[Effect] Active effect has null spec/definition entity=%s — stripping"),
						*EntityHandle.DebugGetDescription());
					IndicesToRemove.Add(Index);
					RemovalReasons.Add(EArcEffectRemovalReason::Invalid);
					bChanged = true;
					continue;
				}

				const FArcEffectPolicy& Policy = Spec->Definition->StackingPolicy;

				if (Policy.Periodicity == EArcEffectPeriodicity::Periodic && Policy.Period > 0.f && !Active.bInhibited)
				{
					Active.PeriodTimer -= DeltaTime;
					if (Active.PeriodTimer <= 0.f)
					{
						// PeriodTimer += Period once per tick: ticks are dropped on hitches > 2x period (accepted, GAS-equivalent).
					Active.PeriodTimer += Policy.Period;

						FPeriodicWork& Work = PeriodicWork.AddDefaulted_GetRef();
						Work.Entity = EntityHandle;
						Work.Spec = Active.Spec;
						Work.Definition = Spec->Definition;
						Work.Handle = Active.Handle;

						ARCMA_VLOG(Ctx.GetWorld(), Log,
							TEXT("[Effect] Periodic execution definition=%s entity=%s period=%.2f"),
							*Spec->Definition->GetName(),
							*EntityHandle.DebugGetDescription(),
							Policy.Period);

						bChanged = true;
					}
				}

				if (Policy.DurationType == EArcEffectDuration::Duration)
				{
					Active.RemainingDuration -= DeltaTime * Active.DecayRate;
					if (Active.RemainingDuration <= 0.f)
					{
						ARCMA_VLOG(Ctx.GetWorld(), Log,
							TEXT("[Effect] Expired definition=%s handle=%u entity=%s"),
							*Spec->Definition->GetName(),
							GetTypeHash(Active.Handle),
							*EntityHandle.DebugGetDescription());

						IndicesToRemove.Add(Index);
						RemovalReasons.Add(EArcEffectRemovalReason::Expired);
						bChanged = true;
					}
				}
			}

			TArray<FArcActiveEffect> Extracted;
			ArcEffects::Private::ExtractActiveEffects(Stack.ActiveEffects, IndicesToRemove, Extracted);
			for (int32 ExtractedIdx = 0; ExtractedIdx < Extracted.Num(); ++ExtractedIdx)
			{
				FRemovedWork& Work = RemovedWork.AddDefaulted_GetRef();
				Work.Entity = EntityHandle;
				Work.Effect = MoveTemp(Extracted[ExtractedIdx]);
				Work.Reason = RemovalReasons[ExtractedIdx];
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
		                                    Work.Spec,
		                                    EArcEffectExecutionTrigger::PeriodicTick);

		FArcEffectContext TickCtx;
		TickCtx.EntityManager = &EntityManager;
		TickCtx.TargetEntity = Work.Entity;
		TickCtx.SourceEntity = SpecPtr->Context.SourceEntity;
		TickCtx.EffectDefinition = Work.Definition;
		TickCtx.StackCount = SpecPtr->Context.StackCount;
		TickCtx.SourceData = SpecPtr->Context.SourceData;
		TickCtx.OriginTags = SpecPtr->Context.OriginTags;
		TickCtx.SourceAbility = SpecPtr->Context.SourceAbility;
		TickCtx.SourceAbilityHandle = SpecPtr->Context.SourceAbilityHandle;
		TickCtx.EffectHandle = Work.Handle;

		ArcEffects::Private::RunComponentHook(Work.Definition->Components,
			[&TickCtx](const FArcEffectComponent& Comp) { Comp.OnExecuted(TickCtx); });

		if (EntityManager.IsEntityValid(Work.Entity))
		{
			FMassEntityView PeriodView(EntityManager, Work.Entity);
			FArcEffectStackFragment* PeriodStack = PeriodView.GetFragmentDataPtr<FArcEffectStackFragment>();
			if (PeriodStack)
			{
				FArcActiveEffect* Active = ArcEffects::FindActiveEffect(*PeriodStack, Work.Handle);
				if (Active)
				{
					for (FInstancedStruct& TaskStruct : Active->Tasks)
					{
						if (FArcEffectTask* Task = TaskStruct.GetMutablePtr<FArcEffectTask>())
						{
							Task->OnExecute(*Active);
						}
					}
				}
			}
		}

		TRACE_ARCMA_EFFECT_EXECUTED(UWorld::GetSubsystem<UArcGameEventSubsystem>(EntityManager.GetWorld()), Work.Entity, Work.Definition);
		ArcEffects::Private::QueueEffectExecuted(EntityManager, Work.Entity, Work.Spec, Work.Handle);
	}

	// Pass 2 — expired and invalid effect removal
	for (FRemovedWork& Work : RemovedWork)
	{
		ArcEffects::CleanupActiveEffect(EntityManager, Work.Entity, MoveTemp(Work.Effect), Work.Reason);
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
