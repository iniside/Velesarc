// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_ExecuteTargeting.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Sources/ArcSourceItemResolver.h"
#include "Items/ArcItemData.h"
#include "Targeting/ArcTargetingItemContext.h"
#include "ArcTargetingContext.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingSubsystem.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "StateTreeAsyncExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"

EStateTreeRunStatus FArcAbilityTask_ExecuteTargeting::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_ExecuteTargetingInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ExecuteTargetingInstanceData>(*this);

    if (!InstanceData.TargetingPreset)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_ExecuteTargeting: TargetingPreset is null.");
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_ExecuteTargeting: Entity has no FMassActorFragment.");
        return EStateTreeRunStatus::Failed;
    }

    AActor* Actor = ActorFragment->GetMutable();
    if (!Actor)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_ExecuteTargeting: FMassActorFragment has no valid actor.");
        return EStateTreeRunStatus::Failed;
    }

    UWorld* World = Actor->GetWorld();
    UArcTargetingSubsystem* TargetingSubsystem = UArcTargetingSubsystem::Get(World);
    if (!TargetingSubsystem)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_ExecuteTargeting: UArcTargetingSubsystem not available.");
        return EStateTreeRunStatus::Failed;
    }

    FArcTargetingContext SeedContext;
    SeedContext.SourceActor = Actor;
    SeedContext.InstigatorActor = Actor;
    SeedContext.SourceEntity = Entity;

    // Project-specific payload goes through ExtraData so ArcTargeting stays free of ArcCore types.
    if (const FArcItemData* ItemData = ArcAbilityKit::FindSourceItem(EntityManager, Entity, AbilityContext.GetAbilityHandle()))
    {
        FArcTargetingItemContext& ItemCtx = SeedContext.FindOrAddExtra<FArcTargetingItemContext>();
        ItemCtx.SourceItemId = ItemData->GetItemId();
        ItemCtx.SourceItemPtr = const_cast<FArcItemData*>(ItemData);
    }

    InstanceData.RequestHandle = TargetingSubsystem->MakeRequest(InstanceData.TargetingPreset, SeedContext);

    FArcTargetingCompletedDelegate Delegate;
    Delegate.BindLambda(
        [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](FArcTargetingHandle CompletedHandle)
        {
            FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
            if (!StrongContext.IsValid())
            {
                return;
            }

            FArcAbilityTask_ExecuteTargetingInstanceData* Data =
                StrongContext.GetInstanceDataPtr<FArcAbilityTask_ExecuteTargetingInstanceData>();
            if (!Data)
            {
                return;
            }

            UWorld* World = StrongContext.GetOwner()->GetWorld();
            UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(World);
            if (Sub)
            {
                if (const FArcTargetingContext* Ctx = Sub->GetContext(CompletedHandle))
                {
                    TArray<FHitResult> HitResults;
                    HitResults.Reserve(Ctx->Candidates.Num());
                    for (const FArcTargetCandidate& Candidate : Ctx->Candidates)
                    {
                        HitResults.Add(Candidate.HitResult);
                    }
                    Data->TargetDataHandle.SetHitResults(MoveTemp(HitResults));
                }
            }

            StrongContext.CopyOutputBindings();
            StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

            UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
            if (SignalSub)
            {
                SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
            }
        });

    TargetingSubsystem->StartAsync(InstanceData.RequestHandle, Delegate);

    return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_ExecuteTargeting::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_ExecuteTargetingInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_ExecuteTargetingInstanceData>(*this);

    if (InstanceData.RequestHandle.IsValid())
    {
        UWorld* World = Context.GetWorld();
        if (UArcTargetingSubsystem* TargetingSubsystem = UArcTargetingSubsystem::Get(World))
        {
            TargetingSubsystem->Cancel(InstanceData.RequestHandle);
            TargetingSubsystem->Release(InstanceData.RequestHandle);
        }
        InstanceData.RequestHandle.Reset();
    }
}
