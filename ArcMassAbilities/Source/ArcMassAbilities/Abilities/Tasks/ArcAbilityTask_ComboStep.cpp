// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_ComboStep.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Mass/ArcAbilityComboStepFragment.h"
#include "MassActorSubsystem.h"
#include "Mass/Commands.h"
#include "Mass/EntityManager.h"
#include "StateTreeExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_ComboStep::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_ComboStepInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_ComboStepInstanceData>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment || !ActorFragment->IsValid())
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] ComboStep FAILED: no valid actor fragment entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	AActor* Actor = ActorFragment->GetMutable();
	if (!Actor)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] ComboStep FAILED: actor is null entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	USkeletalMeshComponent* SkelMeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkelMeshComp)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] ComboStep FAILED: no skeletal mesh component entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	UAnimInstance* AnimInst = SkelMeshComp->GetAnimInstance();
	if (!AnimInst || !InstanceData.Montage)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] ComboStep FAILED: no anim instance or null montage entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
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

	const uint32 CapturedSerial = ArcAbilities::NextComboStepSerial();
	InstanceData.Serial = CapturedSerial;

	ARCMA_VLOG(World, Log,
		TEXT("[Ability] ComboStep enter entity=[%d:%d] montage=%s section=%s window=[%.2f,%.2f] input_tag=%s serial=%u"),
		Entity.Index, Entity.SerialNumber,
		*InstanceData.Montage->GetName(),
		(InstanceData.ComboSection != NAME_None ? *InstanceData.ComboSection.ToString() : TEXT("default")),
		ComboWindowStart, ComboWindowEnd,
		*InputTag.ToString(), CapturedSerial);

	ARCMA_VLOG_LOCATION(World, Actor->GetActorLocation(), 25.f, FColor::Cyan,
		TEXT("[Ability] ComboStep %s window=[%.2f,%.2f] entity=[%d:%d] serial=%u"),
		*InstanceData.Montage->GetName(), ComboWindowStart, ComboWindowEnd,
		Entity.Index, Entity.SerialNumber, CapturedSerial);

	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[Entity, CapturedSerial, MontageDuration, ComboWindowStart, ComboWindowEnd, InputTag, WeakMontage, WeakAnimInst, World](FMassEntityManager& Mgr)
		{
			FStructView View = Mgr.GetMutableSparseElementDataForEntity(
				FArcAbilityComboStepFragment::StaticStruct(), Entity);
			FArcAbilityComboStepFragment* ComboFrag = View.GetPtr<FArcAbilityComboStepFragment>();
			if (!ComboFrag)
			{
				ComboFrag = &Mgr.AddSparseElementToEntity<FArcAbilityComboStepFragment>(Entity);
				Mgr.AddSparseElementToEntity<FArcAbilityComboStepTag>(Entity);
			}
			ComboFrag->Serial = CapturedSerial;
			ComboFrag->MontageDuration = MontageDuration;
			ComboFrag->ElapsedTime = 0.f;
			ComboFrag->ComboWindowStart = ComboWindowStart;
			ComboFrag->ComboWindowEnd = ComboWindowEnd;
			ComboFrag->InputTag = InputTag;
			ComboFrag->bInputReceived = false;
			ComboFrag->Montage = WeakMontage;
			ComboFrag->AnimInstance = WeakAnimInst;
			ARCMA_VLOG(World, Log,
				TEXT("[Ability] ComboStep: fragment seeded serial=%u window=[%.2f,%.2f] entity=[%d:%d]"),
				CapturedSerial, ComboWindowStart, ComboWindowEnd, Entity.Index, Entity.SerialNumber);
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

	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	if (ComboFrag->bInputReceived)
	{
		ARCMA_VLOG(World, Log,
			TEXT("[Ability] ComboStep: input received, combo continued entity=[%d:%d] serial=%u"),
			Entity.Index, Entity.SerialNumber, ComboFrag->Serial);
		return EStateTreeRunStatus::Succeeded;
	}

	UAnimInstance* AnimInst = ComboFrag->AnimInstance.Get();
	if (!AnimInst || ComboFrag->ElapsedTime >= ComboFrag->MontageDuration)
	{
		ARCMA_VLOG(World, Log,
			TEXT("[Ability] ComboStep: window expired or anim done, failed entity=[%d:%d] elapsed=%.2f total=%.2f serial=%u"),
			Entity.Index, Entity.SerialNumber,
			ComboFrag->ElapsedTime, ComboFrag->MontageDuration, ComboFrag->Serial);
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_ComboStep::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityTask_ComboStepInstanceData& InstanceData =
		Context.GetInstanceData<FArcAbilityTask_ComboStepInstanceData>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FStructView ComboView = EntityManager.GetMutableSparseElementDataForEntity(
		FArcAbilityComboStepFragment::StaticStruct(), Entity);

	FArcAbilityComboStepFragment* ComboFrag = ComboView.GetPtr<FArcAbilityComboStepFragment>();

	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	// Only stop the montage if this task instance is still the owner — a
	// newer EnterState (next combo step) may have already claimed the slot
	// and re-played, and stopping that montage would interrupt the new step.
	if (ComboFrag && ComboFrag->Serial == InstanceData.Serial)
	{
		ARCMA_VLOG(World, Log,
			TEXT("[Ability] ComboStep: stopping montage serial=%u entity=[%d:%d]"),
			InstanceData.Serial, Entity.Index, Entity.SerialNumber);
		UAnimInstance* AnimInst = ComboFrag->AnimInstance.Get();
		UAnimMontage* Montage = ComboFrag->Montage.Get();
		if (AnimInst && Montage && AnimInst->Montage_IsPlaying(Montage))
		{
			AnimInst->Montage_Stop(0.2f, Montage);
		}
	}
	else
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] ComboStep: serial mismatch, skip stop my_serial=%u current=%u entity=[%d:%d]"),
			InstanceData.Serial,
			(ComboFrag ? ComboFrag->Serial : 0u),
			Entity.Index, Entity.SerialNumber);
	}

	const uint32 MySerial = InstanceData.Serial;
	EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>(
		[Entity, MySerial, World](FMassEntityManager& Mgr)
		{
			// Generation fence: skip if a newer EnterState has reclaimed the
			// fragment for the next combo step.
			FStructView View = Mgr.GetMutableSparseElementDataForEntity(
				FArcAbilityComboStepFragment::StaticStruct(), Entity);
			const FArcAbilityComboStepFragment* Frag = View.GetPtr<FArcAbilityComboStepFragment>();
			if (Frag && Frag->Serial == MySerial)
			{
				Mgr.RemoveSparseElementFromEntity<FArcAbilityComboStepFragment>(Entity);
				Mgr.RemoveSparseElementFromEntity<FArcAbilityComboStepTag>(Entity);
			}
			else
			{
				ARCMA_VLOG(World, Warning,
					TEXT("[Ability] ComboStep: serial mismatch, skip fragment remove my_serial=%u current=%u entity=[%d:%d]"),
					MySerial,
					(Frag ? Frag->Serial : 0u),
					Entity.Index, Entity.SerialNumber);
			}
		});
}
