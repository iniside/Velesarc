// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_WaitTargetingResult.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "MassAbilities/ArcAbilitySourceData_Item.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "TargetingSystem/TargetingPreset.h"
#include "StateTreeAsyncExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"

EStateTreeRunStatus FArcAbilityTask_WaitTargetingResult::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_WaitTargetingResultInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_WaitTargetingResultInstanceData>(*this);

    InstanceData.RequestHandle.Reset();

    if (!InstanceData.TargetingPreset)
    {
        UE_LOG(LogTemp, Warning, TEXT("FArcAbilityTask_WaitTargetingResult: TargetingPreset is null."));
        return EStateTreeRunStatus::Failed;
    }

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment)
    {
        UE_LOG(LogTemp, Warning, TEXT("FArcAbilityTask_WaitTargetingResult: Entity has no FMassActorFragment."));
        return EStateTreeRunStatus::Failed;
    }

    AActor* Actor = ActorFragment->GetMutable();
    if (!Actor)
    {
        UE_LOG(LogTemp, Warning, TEXT("FArcAbilityTask_WaitTargetingResult: FMassActorFragment has no valid actor."));
        return EStateTreeRunStatus::Failed;
    }

    UWorld* World = Actor->GetWorld();
    UTargetingSubsystem* TargetingSubsystem = UTargetingSubsystem::Get(World);
    if (!TargetingSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("FArcAbilityTask_WaitTargetingResult: UTargetingSubsystem not available."));
        return EStateTreeRunStatus::Failed;
    }

    FArcTargetingSourceContext SourceContext;
    SourceContext.SourceActor = Actor;
    SourceContext.InstigatorActor = Actor;

    // Populate from ability SourceData if available
    const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(EntityManager, Entity, AbilityContext.GetAbilityHandle());
    if (ActiveAbility)
    {
        const FArcAbilitySourceData_Item* ItemSource = ActiveAbility->SourceData.GetPtr<FArcAbilitySourceData_Item>();
        if (ItemSource)
        {
            SourceContext.SourceItemId = ItemSource->ItemId;
            SourceContext.SourceItemPtr = const_cast<FArcItemData*>(ItemSource->ItemDataHandle.Get());

            UArcCoreAbilitySystemComponent* ASC = Actor->FindComponentByClass<UArcCoreAbilitySystemComponent>();
            if (ASC)
            {
                SourceContext.ArcASC = ASC;
            }

            if (ItemSource->ItemDataHandle.IsValid())
            {
                SourceContext.ItemsStoreComponent = const_cast<FArcItemData*>(ItemSource->ItemDataHandle.Get())->GetItemsStoreComponent();
            }
        }
    }

    InstanceData.RequestHandle = Arcx::MakeTargetRequestHandle(InstanceData.TargetingPreset, SourceContext);

    FTargetingRequestDelegate Delegate;
    Delegate.BindLambda([WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](FTargetingRequestHandle CompletedHandle)
    {
        FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
        if (!StrongContext.IsValid())
        {
            return;
        }

        FArcAbilityTask_WaitTargetingResultInstanceData* Data =
            StrongContext.GetInstanceDataPtr<FArcAbilityTask_WaitTargetingResultInstanceData>();
        if (!Data)
        {
            return;
        }

        FTargetingDefaultResultsSet* ResultsSet = FTargetingDefaultResultsSet::Find(CompletedHandle);
        if (ResultsSet)
        {
            TArray<FHitResult> HitResults;
            HitResults.Reserve(ResultsSet->TargetResults.Num());
            for (const FTargetingDefaultResultData& ResultData : ResultsSet->TargetResults)
            {
                HitResults.Add(ResultData.HitResult);
            }
            Data->TargetDataHandle.SetHitResults(MoveTemp(HitResults));
        }

        StrongContext.CopyOutputBindings();
        StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

        UWorld* World = StrongContext.GetOwner()->GetWorld();
        UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
        if (SignalSub)
        {
            SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
        }
    });

    TargetingSubsystem->StartAsyncTargetingRequestWithHandle(InstanceData.RequestHandle, Delegate);

    return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WaitTargetingResult::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_WaitTargetingResultInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_WaitTargetingResultInstanceData>(*this);

    if (InstanceData.RequestHandle.IsValid())
    {
        UWorld* World = Context.GetWorld();
        UTargetingSubsystem* TargetingSubsystem = UTargetingSubsystem::Get(World);
        if (TargetingSubsystem)
        {
            TargetingSubsystem->RemoveAsyncTargetingRequestWithHandle(InstanceData.RequestHandle);
        }
        InstanceData.RequestHandle.Reset();
    }
}
