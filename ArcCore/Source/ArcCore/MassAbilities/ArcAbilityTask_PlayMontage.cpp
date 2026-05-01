// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_PlayMontage.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassAbilities/ArcAbilityMontageFragment.h"
#include "MassActorSubsystem.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

EStateTreeRunStatus FArcAbilityTask_PlayMontage::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_PlayMontageInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_PlayMontageInstanceData>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment || !ActorFragment->IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	AActor* Actor = ActorFragment->GetMutable();
	if (!Actor)
	{
		return EStateTreeRunStatus::Failed;
	}

	USkeletalMeshComponent* SkelMeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMeshComp)
	{
		return EStateTreeRunStatus::Failed;
	}

	UAnimInstance* AnimInst = SkelMeshComp->GetAnimInstance();
	if (!AnimInst || !InstanceData.Montage)
	{
		return EStateTreeRunStatus::Failed;
	}

	AnimInst->Montage_Play(InstanceData.Montage, InstanceData.PlayRate);

	if (InstanceData.StartSection != NAME_None)
	{
		AnimInst->Montage_JumpToSection(InstanceData.StartSection, InstanceData.Montage);
	}

	float ResolvedDuration = InstanceData.Duration > 0.f
		? InstanceData.Duration
		: InstanceData.Montage->GetPlayLength() / InstanceData.PlayRate;

	bool bLoop = InstanceData.bLoop;
	TWeakObjectPtr<UAnimMontage> WeakMontage = InstanceData.Montage;
	TWeakObjectPtr<UAnimInstance> WeakAnimInst = AnimInst;

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
		[Entity, ResolvedDuration, bLoop, WeakMontage, WeakAnimInst](FMassEntityManager& Mgr)
		{
			FArcAbilityMontageFragment& MontageFrag = Mgr.AddSparseElementToEntity<FArcAbilityMontageFragment>(Entity);
			MontageFrag.Duration = ResolvedDuration;
			MontageFrag.ElapsedTime = 0.f;
			MontageFrag.bLoop = bLoop;
			MontageFrag.Montage = WeakMontage;
			MontageFrag.AnimInstance = WeakAnimInst;
			Mgr.AddSparseElementToEntity<FArcAbilityMontageTag>(Entity);
		});

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

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FStructView MontageView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityMontageFragment::StaticStruct(), AbilityContext.GetEntity());

	FArcAbilityMontageFragment* MontageFrag = MontageView.GetPtr<FArcAbilityMontageFragment>();
	if (!MontageFrag)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	UAnimInstance* AnimInst = MontageFrag->AnimInstance.Get();
	if (!AnimInst || !AnimInst->Montage_IsPlaying(MontageFrag->Montage.Get()))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (MontageFrag->ElapsedTime >= MontageFrag->Duration)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_PlayMontage::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FStructView MontageView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityMontageFragment::StaticStruct(), Entity);
	FArcAbilityMontageFragment* MontageFrag = MontageView.GetPtr<FArcAbilityMontageFragment>();

	if (MontageFrag)
	{
		UAnimInstance* AnimInst = MontageFrag->AnimInstance.Get();
		UAnimMontage* Montage = MontageFrag->Montage.Get();
		if (AnimInst && Montage && AnimInst->Montage_IsPlaying(Montage))
		{
			AnimInst->Montage_Stop(0.2f, Montage);
		}
	}

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity](FMassEntityManager& Mgr)
		{
			Mgr.RemoveSparseElementFromEntity<FArcAbilityMontageFragment>(Entity);
			Mgr.RemoveSparseElementFromEntity<FArcAbilityMontageTag>(Entity);
		});
}
