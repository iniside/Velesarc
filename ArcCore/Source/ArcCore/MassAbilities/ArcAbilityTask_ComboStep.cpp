// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_ComboStep.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "MassAbilities/ArcAbilityComboStepFragment.h"
#include "MassActorSubsystem.h"
#include "MassCommands.h"
#include "MassEntityManager.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

EStateTreeRunStatus FArcAbilityTask_ComboStep::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_ComboStepInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_ComboStepInstanceData>(*this);

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

	float PlayRate = 1.0f;
	AnimInst->Montage_Play(InstanceData.Montage, PlayRate);

	if (InstanceData.ComboSection != NAME_None)
	{
		AnimInst->Montage_JumpToSection(InstanceData.ComboSection, InstanceData.Montage);
	}

	float MontageDuration = InstanceData.Montage->GetPlayLength();
	float ComboWindowStart = InstanceData.ComboWindowStart;
	float ComboWindowEnd = InstanceData.ComboWindowEnd;
	FGameplayTag InputTag = InstanceData.InputTag;
	TWeakObjectPtr<UAnimMontage> WeakMontage = InstanceData.Montage;
	TWeakObjectPtr<UAnimInstance> WeakAnimInst = AnimInst;

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
		[Entity, MontageDuration, ComboWindowStart, ComboWindowEnd, InputTag, WeakMontage, WeakAnimInst](FMassEntityManager& Mgr)
		{
			FArcAbilityComboStepFragment& ComboFrag = Mgr.AddSparseElementToEntity<FArcAbilityComboStepFragment>(Entity);
			ComboFrag.MontageDuration = MontageDuration;
			ComboFrag.ElapsedTime = 0.f;
			ComboFrag.ComboWindowStart = ComboWindowStart;
			ComboFrag.ComboWindowEnd = ComboWindowEnd;
			ComboFrag.InputTag = InputTag;
			ComboFrag.bInputReceived = false;
			ComboFrag.Montage = WeakMontage;
			ComboFrag.AnimInstance = WeakAnimInst;
			Mgr.AddSparseElementToEntity<FArcAbilityComboStepTag>(Entity);
		});

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_ComboStep::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FStructView ComboView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityComboStepFragment::StaticStruct(), AbilityContext.GetEntity());

	FArcAbilityComboStepFragment* ComboFrag = ComboView.GetPtr<FArcAbilityComboStepFragment>();
	if (!ComboFrag)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (ComboFrag->bInputReceived)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	UAnimInstance* AnimInst = ComboFrag->AnimInstance.Get();
	if (!AnimInst || ComboFrag->ElapsedTime >= ComboFrag->MontageDuration)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_ComboStep::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FStructView ComboView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityComboStepFragment::StaticStruct(), Entity);

	FArcAbilityComboStepFragment* ComboFrag = ComboView.GetPtr<FArcAbilityComboStepFragment>();
	if (ComboFrag)
	{
		UAnimInstance* AnimInst = ComboFrag->AnimInstance.Get();
		UAnimMontage* Montage = ComboFrag->Montage.Get();
		if (AnimInst && Montage && AnimInst->Montage_IsPlaying(Montage))
		{
			AnimInst->Montage_Stop(0.2f, Montage);
		}
	}

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity](FMassEntityManager& Mgr)
		{
			Mgr.RemoveSparseElementFromEntity<FArcAbilityComboStepFragment>(Entity);
			Mgr.RemoveSparseElementFromEntity<FArcAbilityComboStepTag>(Entity);
		});
}
