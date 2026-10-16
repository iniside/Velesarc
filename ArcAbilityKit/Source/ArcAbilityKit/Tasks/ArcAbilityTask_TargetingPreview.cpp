// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_TargetingPreview.h"
#include "Tasks/ArcAbilityTaskHelpers.h"
#include "Targeting/ArcTargetingPreview.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcTargetingContext.h"
#include "ArcTargetingSubsystem.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "StateTreeAsyncExecutionContext.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"

namespace ArcTargetingPreviewTask
{
    static bool HasAnyCandidate(const FArcTargetingContext& Ctx)
    {
        return Ctx.Candidates.Num() > 0;
    }

    static FTransform ExtractPrimaryPreviewTransform(const FArcTargetingContext& Ctx)
    {
        const FHitResult& Hit = Ctx.Candidates[0].HitResult;
        const FVector Location = Hit.ImpactPoint;
        const FQuat Rotation = FRotationMatrix::MakeFromZ(Hit.ImpactNormal).ToQuat();
        const FVector Scale = FVector::OneVector;
        return FTransform(Rotation, Location, Scale);
    }
}

EStateTreeRunStatus FArcAbilityTask_TargetingPreview::EnterState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
    FArcAbilityTask_TargetingPreviewInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_TargetingPreviewInstanceData>(*this);

    FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
    const FMassEntityHandle Entity = AbilityContext.GetEntity();

    // (1) Owning-client gate — server, simulated, or actor-less entity → skip.
    if (ArcAbilityKit::GetLocalPlayerForEntity(EntityManager, Entity) == nullptr)
    {
        return EStateTreeRunStatus::Failed;
    }

    // (2) Validate inputs.
    if (!InstanceData.TargetingPreset || !InstanceData.PreviewActorClass)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_TargetingPreview: TargetingPreset or PreviewActorClass is null.");
        return EStateTreeRunStatus::Failed;
    }

    // (3) Resolve world from ActorFragment (already validated by the gate above).
    FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    if (!ActorFragment)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_TargetingPreview: Entity has no FMassActorFragment.");
        return EStateTreeRunStatus::Failed;
    }

    AActor* OwnerActor = ActorFragment->GetMutable();
    if (!OwnerActor)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_TargetingPreview: FMassActorFragment has no valid actor.");
        return EStateTreeRunStatus::Failed;
    }

    UWorld* World = OwnerActor->GetWorld();

    // (4) Spawn preview actor hidden. Owner = OwnerActor so it is cleaned up if the pawn dies mid-preview.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwnerActor;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Preview = World->SpawnActor<AActor>(InstanceData.PreviewActorClass, FTransform::Identity, SpawnParams);
    if (!Preview)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_TargetingPreview: Failed to spawn preview actor.");
        return EStateTreeRunStatus::Failed;
    }

    InstanceData.SpawnedActor = Preview;

    IArcTargetingPreview::Execute_SetActive(Preview, false);
    IArcTargetingPreview::Execute_SetShape(Preview, InstanceData.ShapeParams);

    // (5) Kick a continuous targeting request.
    UArcTargetingSubsystem* TargetingSubsystem = UArcTargetingSubsystem::Get(World);
    if (!TargetingSubsystem)
    {
        UE_LOGF(LogTemp, Warning,"FArcAbilityTask_TargetingPreview: UArcTargetingSubsystem not available.");
        Preview->Destroy();
        InstanceData.SpawnedActor.Reset();
        return EStateTreeRunStatus::Failed;
    }

    FArcTargetingContext SeedContext;
    SeedContext.SourceActor = OwnerActor;
    SeedContext.InstigatorActor = OwnerActor;
    SeedContext.SourceEntity = Entity;

    InstanceData.RequestHandle = TargetingSubsystem->MakeRequest(InstanceData.TargetingPreset, SeedContext);

    FArcTargetingCompletedDelegate Delegate;
    Delegate.BindLambda(
        [WeakContext = Context.MakeWeakExecutionContext()](FArcTargetingHandle CompletedHandle)
        {
            FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
            if (!StrongContext.IsValid())
            {
                return;
            }

            FArcAbilityTask_TargetingPreviewInstanceData* Data =
                StrongContext.GetInstanceDataPtr<FArcAbilityTask_TargetingPreviewInstanceData>();
            if (!Data)
            {
                return;
            }

            AActor* PreviewActor = Data->SpawnedActor.Get();
            if (!PreviewActor)
            {
                return;
            }

            UWorld* LambdaWorld = StrongContext.GetOwner()->GetWorld();
            UArcTargetingSubsystem* Sub = UArcTargetingSubsystem::Get(LambdaWorld);
            if (!Sub)
            {
                return;
            }

            const FArcTargetingContext* Ctx = Sub->GetContext(CompletedHandle);
            if (Ctx && ArcTargetingPreviewTask::HasAnyCandidate(*Ctx))
            {
                const FTransform Xf = ArcTargetingPreviewTask::ExtractPrimaryPreviewTransform(*Ctx);
                PreviewActor->SetActorTransform(Xf);
                IArcTargetingPreview::Execute_SetActive(PreviewActor, true);
            }
            else
            {
                IArcTargetingPreview::Execute_SetActive(PreviewActor, false);
            }
            // No CopyOutputBindings — no output to publish.
            // No FinishTask — task stays Running until ExitState.
            // No AbilityStateTreeTickRequired signal — cosmetic only, no state change.
        });

    TargetingSubsystem->StartAsync(InstanceData.RequestHandle, Delegate, /*bRequeueOnCompletion=*/true);

    return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_TargetingPreview::ExitState(
    FStateTreeExecutionContext& Context,
    const FStateTreeTransitionResult& Transition) const
{
    FArcAbilityTask_TargetingPreviewInstanceData& InstanceData =
        Context.GetInstanceData<FArcAbilityTask_TargetingPreviewInstanceData>(*this);

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

    if (AActor* Preview = InstanceData.SpawnedActor.Get())
    {
        Preview->Destroy();
    }
    InstanceData.SpawnedActor.Reset();
}
