// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_PlayMontage.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "ArcMassAbilitiesLogs.h"

namespace ArcAbilityTaskPlayMontage
{
	static UAnimInstance* ResolveAnimInstance(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (!ActorFragment || !ActorFragment->IsValid())
		{
			return nullptr;
		}

		AActor* Actor = ActorFragment->GetMutable();
		if (!Actor)
		{
			return nullptr;
		}

		USkeletalMeshComponent* SkelMeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
		if (!SkelMeshComp)
		{
			return nullptr;
		}

		return SkelMeshComp->GetAnimInstance();
	}
} // namespace ArcAbilityTaskPlayMontage

EStateTreeRunStatus FArcAbilityTask_PlayMontage::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_PlayMontageInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_PlayMontageInstanceData>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontage::ResolveAnimInstance(EntityManager, Entity);
	if (!AnimInst || !InstanceData.Montage)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] PlayMontage FAILED: no anim instance or null montage entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	AnimInst->Montage_Play(InstanceData.Montage, InstanceData.PlayRate);

	if (InstanceData.StartSection != NAME_None)
	{
		AnimInst->Montage_JumpToSection(InstanceData.StartSection, InstanceData.Montage);
	}

	const float ResolvedDuration = InstanceData.Duration > 0.f
		? InstanceData.Duration
		: InstanceData.Montage->GetPlayLength() / InstanceData.PlayRate;

	const bool bLoop = InstanceData.bLoop;

	InstanceData.ElapsedTime = 0.f;
	InstanceData.ResolvedDuration = ResolvedDuration;
	InstanceData.bInterrupted = false;

	ARCMA_VLOG(World, Log,
		TEXT("[Ability] PlayMontage enter entity=[%d:%d] montage=%s duration=%.2f playrate=%.2f"),
		Entity.Index, Entity.SerialNumber,
		*InstanceData.Montage->GetName(), ResolvedDuration, InstanceData.PlayRate);

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (ActorFragment && ActorFragment->IsValid())
	{
		if (AActor* Actor = ActorFragment->GetMutable())
		{
			ARCMA_VLOG_LOCATION(World, Actor->GetActorLocation(), 25.f, FColor::Cyan,
				TEXT("[Ability] PlayMontage %s entity=[%d:%d]"),
				*InstanceData.Montage->GetName(), Entity.Index, Entity.SerialNumber);
		}
	}

	// Bind the anim-end delegate so that early interrupts (another montage stomping ours,
	// Montage_Stop from an external system) immediately wake the StateTree instead of
	// waiting until the scheduled-tick fires.
	InstanceData.EndedDelegate.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Entity](UAnimMontage* InMontage, bool bInterrupted)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FArcAbilityTask_PlayMontageInstanceData* Data =
				StrongContext.GetInstanceDataPtr<FArcAbilityTask_PlayMontageInstanceData>();
			if (Data)
			{
				Data->bInterrupted = bInterrupted;
			}

			UObject* Owner = StrongContext.GetOwner().Get();
			UWorld* OwnerWorld = Owner ? Owner->GetWorld() : nullptr;
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(OwnerWorld);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
			}
		});
	AnimInst->Montage_SetEndDelegate(InstanceData.EndedDelegate, InstanceData.Montage);

	// Schedule the natural-end wake. Loop montages have no natural end so we skip the
	// scheduled request and rely solely on the interrupt delegate or an external ExitState.
	if (!bLoop)
	{
		InstanceData.ScheduledTickHandle = Context.AddScheduledTickRequest(
			FStateTreeScheduledTick::MakeCustomTickRate(ResolvedDuration));
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PlayMontage::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FArcAbilityTask_PlayMontageInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_PlayMontageInstanceData>(*this);

	if (InstanceData.bLoop)
	{
		return EStateTreeRunStatus::Running;
	}

	// Early-interrupt path: the anim-end delegate fired before Duration elapsed
	// (e.g. another montage stomped ours). Match the old polling behavior — both
	// the "anim_stopped" and "duration_elapsed" branches returned Succeeded.
	if (InstanceData.bInterrupted)
	{
		FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
		FMassEntityHandle Entity = AbilityContext.GetEntity();
		UWorld* World = AbilityContext.GetMassContext().GetWorld();
		ARCMA_VLOG(World, Log,
			TEXT("[Ability] PlayMontage: succeeded entity=[%d:%d] reason=interrupted"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime >= InstanceData.ResolvedDuration)
	{
		FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
		FMassEntityHandle Entity = AbilityContext.GetEntity();
		UWorld* World = AbilityContext.GetMassContext().GetWorld();
		ARCMA_VLOG(World, Log,
			TEXT("[Ability] PlayMontage: succeeded entity=[%d:%d] reason=duration_elapsed"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_PlayMontage::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_PlayMontageInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_PlayMontageInstanceData>(*this);

	// Cancel the pending scheduled wake if the state exits early (e.g. transition away).
	if (InstanceData.ScheduledTickHandle.IsValid())
	{
		Context.RemoveScheduledTickRequest(InstanceData.ScheduledTickHandle);
		InstanceData.ScheduledTickHandle = {};
	}

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontage::ResolveAnimInstance(EntityManager, Entity);
	if (AnimInst)
	{
		// Unbind the anim-end delegate before stopping to prevent a re-entrant signal
		// from a late callback firing after this task has been torn down.
		FOnMontageEnded EmptyDelegate;
		AnimInst->Montage_SetEndDelegate(EmptyDelegate, InstanceData.Montage);

		if (InstanceData.Montage && AnimInst->GetActiveInstanceForMontage(InstanceData.Montage))
		{
			ARCMA_VLOG(World, Log,
				TEXT("[Ability] PlayMontage: stopping montage entity=[%d:%d]"),
				Entity.Index, Entity.SerialNumber);
			AnimInst->Montage_Stop(0.2f, InstanceData.Montage);
		}
	}
	InstanceData.EndedDelegate.Unbind();
}
