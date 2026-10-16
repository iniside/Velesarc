// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_PlayMontageStartLoopEndWithMarks.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "MassActorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcMontageStartLoopEndWithMarks, Log, All);

namespace ArcAbilityTaskPlayMontageStartLoopEndWithMarks
{
UAnimInstance* ResolveAnimInstance(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (ActorFragment == nullptr || !ActorFragment->IsValid())
	{
		return nullptr;
	}

	AActor* Actor = ActorFragment->GetMutable();
	if (Actor == nullptr)
	{
		return nullptr;
	}

	USkeletalMeshComponent* SkeletalMeshComponent = Actor->FindComponentByClass<USkeletalMeshComponent>();
	return SkeletalMeshComponent != nullptr ? SkeletalMeshComponent->GetAnimInstance() : nullptr;
}

bool GetNaturalDuration(const UAnimMontage* Montage, float& OutRateScale, float& OutDuration)
{
	if (!IsValid(Montage) || !FMath::IsFinite(Montage->RateScale) || Montage->RateScale <= 0.f)
	{
		return false;
	}

	const float PlayLength = Montage->GetPlayLength();
	if (!FMath::IsFinite(PlayLength) || PlayLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutRateScale = Montage->RateScale;
	OutDuration = PlayLength / OutRateScale;
	return FMath::IsFinite(OutDuration) && OutDuration > KINDA_SMALL_NUMBER;
}

void PrepareMarks(
	const TArray<FArcMontageMarkEntry>& Marks,
	const float PhaseDuration,
	const float RateScale,
	const bool bScaleFromMontageTime,
	const TCHAR* PhaseName,
	FArcAbilityMontagePhaseMarkRuntime& OutRuntime)
{
	TArray<TPair<float, int32>> ValidMarks;
	ValidMarks.Reserve(Marks.Num());
	for (int32 MarkIndex = 0; MarkIndex < Marks.Num(); ++MarkIndex)
	{
		const float AuthoredTime = Marks[MarkIndex].Time;
		const float MarkTime = bScaleFromMontageTime ? AuthoredTime / RateScale : AuthoredTime;
		if (FMath::IsFinite(MarkTime) && MarkTime >= 0.f && MarkTime < PhaseDuration)
		{
			ValidMarks.Emplace(MarkTime, MarkIndex);
		}
		else
		{
			UE_LOGF(LogArcMontageStartLoopEndWithMarks, Warning,
				"PlayMontageStartLoopEndWithMarks: %ls mark[%d] time %.3f is outside [0, %.3f); skipping.",
				PhaseName, MarkIndex, MarkTime, PhaseDuration);
		}
	}

	ValidMarks.Sort([](const TPair<float, int32>& Left, const TPair<float, int32>& Right)
	{
		return Left.Key < Right.Key || (Left.Key == Right.Key && Left.Value < Right.Value);
	});

	OutRuntime.Times.Reset(ValidMarks.Num());
	OutRuntime.OriginalIndices.Reset(ValidMarks.Num());
	for (const TPair<float, int32>& Mark : ValidMarks)
	{
		OutRuntime.Times.Add(Mark.Key);
		OutRuntime.OriginalIndices.Add(Mark.Value);
	}
	OutRuntime.NextIndex = 0;
}

void FireMark(
	FStateTreeExecutionContext& Context,
	const TArray<FArcMontageMarkEntry>& Marks,
	const int32 OriginalIndex,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	if (!Marks.IsValidIndex(OriginalIndex))
	{
		return;
	}

	const FArcMontageMarkEntry& Entry = Marks[OriginalIndex];
	const FStateTreeDelegateDispatcher EntryDispatcher = Entry.Dispatcher;
	InstanceData.LastFiredTag = Entry.Tag;
	InstanceData.LastFiredPayload = Entry.Payload;
	Context.BroadcastDelegate(InstanceData.OnMark);
	Context.BroadcastDelegate(EntryDispatcher);
}

void FireMarksAtPhaseStart(
	FStateTreeExecutionContext& Context,
	const TArray<FArcMontageMarkEntry>& Marks,
	FArcAbilityMontagePhaseMarkRuntime& Runtime,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	while (Runtime.NextIndex < Runtime.Times.Num() && Runtime.Times[Runtime.NextIndex] <= 0.f)
	{
		FireMark(Context, Marks, Runtime.OriginalIndices[Runtime.NextIndex], InstanceData);
		++Runtime.NextIndex;
	}
}

void FireMarksInRange(
	FStateTreeExecutionContext& Context,
	const TArray<FArcMontageMarkEntry>& Marks,
	FArcAbilityMontagePhaseMarkRuntime& Runtime,
	const float IntervalStart,
	const float IntervalEnd,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	while (Runtime.NextIndex < Runtime.Times.Num() && Runtime.Times[Runtime.NextIndex] <= IntervalStart)
	{
		++Runtime.NextIndex;
	}

	while (Runtime.NextIndex < Runtime.Times.Num() && Runtime.Times[Runtime.NextIndex] <= IntervalEnd)
	{
		FireMark(Context, Marks, Runtime.OriginalIndices[Runtime.NextIndex], InstanceData);
		++Runtime.NextIndex;
	}
}

bool PlayVisual(
	UAnimInstance& AnimInstance,
	UAnimMontage* Montage,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	if (InstanceData.ActiveMontage != nullptr)
	{
		AnimInstance.Montage_Stop(0.f, InstanceData.ActiveMontage);
		InstanceData.ActiveMontage = nullptr;
	}

	if (Montage == nullptr || AnimInstance.Montage_Play(Montage, 1.f) <= 0.f)
	{
		return false;
	}

	InstanceData.ActiveMontage = Montage;
	FAnimMontageInstance* MontageInstance = AnimInstance.GetActiveInstanceForMontage(Montage);
	if (MontageInstance == nullptr)
	{
		InstanceData.ActiveMontage = nullptr;
		return false;
	}

	// The task clock owns the phase boundary, so asset auto-blend-out must not start early.
	MontageInstance->bEnableAutoBlendOut = false;
	return true;
}

void SyncVisualPositionIfNeeded(
	UAnimInstance& AnimInstance,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	if (!InstanceData.bVisualPositionNeedsSync || InstanceData.ActiveMontage == nullptr)
	{
		return;
	}
	InstanceData.bVisualPositionNeedsSync = false;

	float PhasePosition = 0.f;
	switch (InstanceData.Phase)
	{
	case EArcAbilityMontageStartLoopEndPhase::Start:
		PhasePosition = InstanceData.PhaseElapsedTime * InstanceData.StartRateScale;
		break;
	case EArcAbilityMontageStartLoopEndPhase::Loop:
		PhasePosition = InstanceData.LoopCycleElapsedTime * InstanceData.LoopRateScale;
		break;
	case EArcAbilityMontageStartLoopEndPhase::End:
		PhasePosition = InstanceData.PhaseElapsedTime * InstanceData.EndRateScale;
		break;
	case EArcAbilityMontageStartLoopEndPhase::Completed:
		return;
	}

	AnimInstance.Montage_SetPosition(
		InstanceData.ActiveMontage,
		FMath::Clamp(PhasePosition, 0.f, InstanceData.ActiveMontage->GetPlayLength()));
}

bool EnterEndPhase(
	FStateTreeExecutionContext& Context,
	UAnimInstance& AnimInstance,
	FArcAbilityTask_PlayMontageStartLoopEndWithMarksInstanceData& InstanceData)
{
	InstanceData.Phase = EArcAbilityMontageStartLoopEndPhase::End;
	InstanceData.PhaseElapsedTime = 0.f;
	InstanceData.EndMarkRuntime.NextIndex = 0;
	if (!PlayVisual(AnimInstance, InstanceData.EndMontage, InstanceData))
	{
		return false;
	}
	InstanceData.bVisualPositionNeedsSync = true;

	FireMarksAtPhaseStart(Context, InstanceData.EndMarks, InstanceData.EndMarkRuntime, InstanceData);
	return true;
}
}

EStateTreeRunStatus FArcAbilityTask_PlayMontageStartLoopEndWithMarks::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.bTerminalDelegateFired = false;
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayMontageStartLoopEndWithMarks::ResolveAnimInstance(
		EntityManager, AbilityContext.GetEntity());
	if (AnimInstance == nullptr
		|| !ArcAbilityTaskPlayMontageStartLoopEndWithMarks::GetNaturalDuration(
			InstanceData.StartMontage, InstanceData.StartRateScale, InstanceData.StartDuration)
		|| !ArcAbilityTaskPlayMontageStartLoopEndWithMarks::GetNaturalDuration(
			InstanceData.LoopMontage, InstanceData.LoopRateScale, InstanceData.LoopDuration)
		|| !ArcAbilityTaskPlayMontageStartLoopEndWithMarks::GetNaturalDuration(
			InstanceData.EndMontage, InstanceData.EndRateScale, InstanceData.EndDuration))
	{
		return EStateTreeRunStatus::Failed;
	}

	const float MinimumDuration = InstanceData.StartDuration + InstanceData.EndDuration;
	if (InstanceData.bUseDuration
		&& (!FMath::IsFinite(InstanceData.Duration) || InstanceData.Duration < MinimumDuration))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.LoopInterval = InstanceData.bUseDuration
		? InstanceData.Duration - MinimumDuration
		: InstanceData.LoopDuration;
	InstanceData.PhaseElapsedTime = 0.f;
	InstanceData.LoopCycleElapsedTime = 0.f;
	InstanceData.Phase = EArcAbilityMontageStartLoopEndPhase::Start;
	InstanceData.ActiveMontage = nullptr;
	InstanceData.bVisualPositionNeedsSync = false;
	InstanceData.LastFiredTag = FGameplayTag();
	InstanceData.LastFiredPayload.Reset();

	ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PrepareMarks(
		InstanceData.StartMarks,
		InstanceData.StartDuration,
		InstanceData.StartRateScale,
		InstanceData.bSyncMarksFromMontagesOnSave,
		TEXT("Start"),
		InstanceData.StartMarkRuntime);
	ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PrepareMarks(
		InstanceData.LoopMarks,
		InstanceData.LoopDuration,
		InstanceData.LoopRateScale,
		InstanceData.bSyncMarksFromMontagesOnSave,
		TEXT("Loop"),
		InstanceData.LoopMarkRuntime);
	ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PrepareMarks(
		InstanceData.EndMarks,
		InstanceData.EndDuration,
		InstanceData.EndRateScale,
		InstanceData.bSyncMarksFromMontagesOnSave,
		TEXT("End"),
		InstanceData.EndMarkRuntime);

	if (!ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PlayVisual(
		*AnimInstance, InstanceData.StartMontage, InstanceData))
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.bVisualPositionNeedsSync = false;

	ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksAtPhaseStart(
		Context, InstanceData.StartMarks, InstanceData.StartMarkRuntime, InstanceData);
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PlayMontageStartLoopEndWithMarks::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayMontageStartLoopEndWithMarks::ResolveAnimInstance(
		EntityManager, AbilityContext.GetEntity());
	if (AnimInstance == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	float RemainingTime = FMath::Max(DeltaTime, 0.f);
	for (;;)
	{
		switch (InstanceData.Phase)
		{
		case EArcAbilityMontageStartLoopEndPhase::Start:
		{
			const float Step = FMath::Min(RemainingTime, InstanceData.StartDuration - InstanceData.PhaseElapsedTime);
			const float PreviousTime = InstanceData.PhaseElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksInRange(
				Context,
				InstanceData.StartMarks,
				InstanceData.StartMarkRuntime,
				PreviousTime,
				InstanceData.PhaseElapsedTime,
				InstanceData);
			if (InstanceData.PhaseElapsedTime < InstanceData.StartDuration)
			{
				ArcAbilityTaskPlayMontageStartLoopEndWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
				return EStateTreeRunStatus::Running;
			}

			if (InstanceData.LoopInterval <= 0.f)
			{
				if (!ArcAbilityTaskPlayMontageStartLoopEndWithMarks::EnterEndPhase(Context, *AnimInstance, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				continue;
			}

			InstanceData.Phase = EArcAbilityMontageStartLoopEndPhase::Loop;
			InstanceData.PhaseElapsedTime = 0.f;
			InstanceData.LoopCycleElapsedTime = 0.f;
			InstanceData.LoopMarkRuntime.NextIndex = 0;
			if (!ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PlayVisual(
				*AnimInstance, InstanceData.LoopMontage, InstanceData))
			{
				return EStateTreeRunStatus::Failed;
			}
			InstanceData.bVisualPositionNeedsSync = true;
			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksAtPhaseStart(
				Context, InstanceData.LoopMarks, InstanceData.LoopMarkRuntime, InstanceData);
			continue;
		}

		case EArcAbilityMontageStartLoopEndPhase::Loop:
		{
			const float IntervalRemaining = InstanceData.LoopInterval - InstanceData.PhaseElapsedTime;
			const float CycleRemaining = InstanceData.LoopDuration - InstanceData.LoopCycleElapsedTime;
			const float Step = FMath::Min(RemainingTime, FMath::Min(IntervalRemaining, CycleRemaining));
			const float PreviousCycleTime = InstanceData.LoopCycleElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			InstanceData.LoopCycleElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksInRange(
				Context,
				InstanceData.LoopMarks,
				InstanceData.LoopMarkRuntime,
				PreviousCycleTime,
				InstanceData.LoopCycleElapsedTime,
				InstanceData);

			if (InstanceData.PhaseElapsedTime >= InstanceData.LoopInterval)
			{
				if (!ArcAbilityTaskPlayMontageStartLoopEndWithMarks::EnterEndPhase(Context, *AnimInstance, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				continue;
			}

			if (InstanceData.LoopCycleElapsedTime >= InstanceData.LoopDuration)
			{
				InstanceData.LoopCycleElapsedTime = 0.f;
				InstanceData.LoopMarkRuntime.NextIndex = 0;
				if (!ArcAbilityTaskPlayMontageStartLoopEndWithMarks::PlayVisual(
					*AnimInstance, InstanceData.LoopMontage, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				InstanceData.bVisualPositionNeedsSync = true;
				ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksAtPhaseStart(
					Context, InstanceData.LoopMarks, InstanceData.LoopMarkRuntime, InstanceData);
				continue;
			}

			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
			return EStateTreeRunStatus::Running;
		}

		case EArcAbilityMontageStartLoopEndPhase::End:
		{
			const float Step = FMath::Min(RemainingTime, InstanceData.EndDuration - InstanceData.PhaseElapsedTime);
			const float PreviousTime = InstanceData.PhaseElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::FireMarksInRange(
				Context,
				InstanceData.EndMarks,
				InstanceData.EndMarkRuntime,
				PreviousTime,
				InstanceData.PhaseElapsedTime,
				InstanceData);
			if (InstanceData.PhaseElapsedTime >= InstanceData.EndDuration)
			{
				InstanceData.Phase = EArcAbilityMontageStartLoopEndPhase::Completed;
				if (!InstanceData.bTerminalDelegateFired)
				{
					InstanceData.bTerminalDelegateFired = true;
					Context.BroadcastDelegate(InstanceData.OnCompleted);
				}
				return EStateTreeRunStatus::Succeeded;
			}

			ArcAbilityTaskPlayMontageStartLoopEndWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
			return EStateTreeRunStatus::Running;
		}

		case EArcAbilityMontageStartLoopEndPhase::Completed:
			return EStateTreeRunStatus::Succeeded;
		}
	}
}

void FArcAbilityTask_PlayMontageStartLoopEndWithMarks::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayMontageStartLoopEndWithMarks::ResolveAnimInstance(
		EntityManager, AbilityContext.GetEntity());
	if (AnimInstance != nullptr && InstanceData.ActiveMontage != nullptr)
	{
		AnimInstance->Montage_Stop(0.2f, InstanceData.ActiveMontage);
	}

	InstanceData.ActiveMontage = nullptr;
	InstanceData.bVisualPositionNeedsSync = false;
	if (!InstanceData.bTerminalDelegateFired)
	{
		InstanceData.bTerminalDelegateFired = true;
		Context.BroadcastDelegate(InstanceData.OnInterrupted);
	}
}
