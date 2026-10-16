// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_PlayCastMontageWithMarks.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "MassActorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCastMontageWithMarks, Log, All);

namespace ArcAbilityTaskPlayCastMontageWithMarks
{
struct FMontageTiming
{
	float PlayLength = 0.f;
	float RateScale = 1.f;
	float NaturalDuration = 0.f;
};

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

bool GetMontageTiming(const UAnimMontage* Montage, FMontageTiming& OutTiming)
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

	const float NaturalDuration = PlayLength / Montage->RateScale;
	if (!FMath::IsFinite(NaturalDuration) || NaturalDuration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutTiming.PlayLength = PlayLength;
	OutTiming.RateScale = Montage->RateScale;
	OutTiming.NaturalDuration = NaturalDuration;
	return true;
}

bool ComputeAdjustedRates(
	const FMontageTiming& Timing,
	const float PhaseDuration,
	float& OutInputPlayRate,
	float& OutEffectivePlayRate)
{
	if (!FMath::IsFinite(PhaseDuration) || PhaseDuration <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutInputPlayRate = Timing.PlayLength / (Timing.RateScale * PhaseDuration);
	OutEffectivePlayRate = Timing.RateScale * OutInputPlayRate;
	return FMath::IsFinite(OutInputPlayRate)
		&& OutInputPlayRate > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(OutEffectivePlayRate)
		&& OutEffectivePlayRate > KINDA_SMALL_NUMBER;
}

void ResetMarks(FArcAbilityCastMontagePhaseMarkRuntime& Runtime)
{
	Runtime.Times.Reset();
	Runtime.OriginalIndices.Reset();
	Runtime.NextIndex = 0;
}

void PrepareMarks(
	const TArray<FArcMontageMarkEntry>& Marks,
	const float PhaseDuration,
	const float EffectivePlayRate,
	const bool bScaleFromMontageTime,
	const TCHAR* PhaseName,
	FArcAbilityCastMontagePhaseMarkRuntime& OutRuntime)
{
	TArray<TPair<float, int32>> ValidMarks;
	ValidMarks.Reserve(Marks.Num());
	for (int32 MarkIndex = 0; MarkIndex < Marks.Num(); ++MarkIndex)
	{
		const float AuthoredTime = Marks[MarkIndex].Time;
		const float MarkTime = bScaleFromMontageTime ? AuthoredTime / EffectivePlayRate : AuthoredTime;
		if (FMath::IsFinite(MarkTime) && MarkTime >= 0.f && MarkTime < PhaseDuration)
		{
			ValidMarks.Emplace(MarkTime, MarkIndex);
		}
		else
		{
			UE_LOGF(LogArcCastMontageWithMarks, Warning,
				"PlayCastMontageWithMarks: %ls mark[%d] time %.3f is outside [0, %.3f); skipping.",
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
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
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
	FArcAbilityCastMontagePhaseMarkRuntime& Runtime,
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
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
	FArcAbilityCastMontagePhaseMarkRuntime& Runtime,
	const float IntervalStart,
	const float IntervalEnd,
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
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
	const float InputPlayRate,
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
{
	if (InstanceData.ActiveMontage != nullptr)
	{
		AnimInstance.Montage_Stop(0.f, InstanceData.ActiveMontage);
		InstanceData.ActiveMontage = nullptr;
	}

	if (Montage == nullptr || AnimInstance.Montage_Play(Montage, InputPlayRate) <= 0.f)
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

	// The task clock owns phase boundaries, so asset auto-blend-out must not start early.
	MontageInstance->bEnableAutoBlendOut = false;
	return true;
}

void SyncVisualPositionIfNeeded(
	UAnimInstance& AnimInstance,
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
{
	if (!InstanceData.bVisualPositionNeedsSync || InstanceData.ActiveMontage == nullptr)
	{
		return;
	}
	InstanceData.bVisualPositionNeedsSync = false;

	float PhasePosition = 0.f;
	switch (InstanceData.Phase)
	{
	case EArcAbilityCastMontagePhase::Start:
		PhasePosition = InstanceData.PhaseElapsedTime * InstanceData.StartEffectivePlayRate;
		break;
	case EArcAbilityCastMontagePhase::Loop:
		PhasePosition = InstanceData.LoopCycleElapsedTime * InstanceData.LoopEffectivePlayRate;
		break;
	case EArcAbilityCastMontagePhase::End:
		PhasePosition = InstanceData.PhaseElapsedTime * InstanceData.EndEffectivePlayRate;
		break;
	case EArcAbilityCastMontagePhase::Completed:
		return;
	}

	AnimInstance.Montage_SetPosition(
		InstanceData.ActiveMontage,
		FMath::Clamp(PhasePosition, 0.f, InstanceData.ActiveMontage->GetPlayLength()));
}

bool EnterEndPhase(
	FStateTreeExecutionContext& Context,
	UAnimInstance& AnimInstance,
	FArcAbilityTask_PlayCastMontageWithMarksInstanceData& InstanceData)
{
	InstanceData.Phase = EArcAbilityCastMontagePhase::End;
	InstanceData.PhaseElapsedTime = 0.f;
	InstanceData.EndMarkRuntime.NextIndex = 0;
	if (!PlayVisual(AnimInstance, InstanceData.EndMontage, InstanceData.EndInputPlayRate, InstanceData))
	{
		return false;
	}
	InstanceData.bVisualPositionNeedsSync = true;
	FireMarksAtPhaseStart(Context, InstanceData.EndMarks, InstanceData.EndMarkRuntime, InstanceData);
	return true;
}
}

EStateTreeRunStatus FArcAbilityTask_PlayCastMontageWithMarks::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.bTerminalDelegateFired = false;
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayCastMontageWithMarks::ResolveAnimInstance(
		EntityManager, AbilityContext.GetEntity());
	if (AnimInstance == nullptr)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bUseStart = false;
	InstanceData.bUseLoop = false;
	InstanceData.StartDuration = 0.f;
	InstanceData.LoopInterval = 0.f;
	InstanceData.EndDuration = 0.f;
	switch (InstanceData.CastTime)
	{
	case EArcAbilityCastTime::QuarterSecond:
		InstanceData.EndDuration = 0.25f;
		break;
	case EArcAbilityCastTime::HalfSecond:
		InstanceData.EndDuration = 0.5f;
		break;
	case EArcAbilityCastTime::ThreeQuarterSecond:
		InstanceData.bUseStart = true;
		InstanceData.StartDuration = 0.25f;
		InstanceData.EndDuration = 0.5f;
		break;
	case EArcAbilityCastTime::OneSecond:
		InstanceData.bUseStart = true;
		InstanceData.StartDuration = 0.5f;
		InstanceData.EndDuration = 0.5f;
		break;
	case EArcAbilityCastTime::TwoSeconds:
		InstanceData.bUseStart = true;
		InstanceData.bUseLoop = true;
		InstanceData.StartDuration = 0.5f;
		InstanceData.LoopInterval = 1.f;
		InstanceData.EndDuration = 0.5f;
		break;
	case EArcAbilityCastTime::Custom:
		if (!FMath::IsFinite(InstanceData.CustomCastTime) || InstanceData.CustomCastTime < 2.f)
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.bUseStart = true;
		InstanceData.bUseLoop = true;
		InstanceData.StartDuration = 0.5f;
		InstanceData.LoopInterval = InstanceData.CustomCastTime - 1.f;
		InstanceData.EndDuration = 0.5f;
		break;
	}

	ArcAbilityTaskPlayCastMontageWithMarks::FMontageTiming EndTiming;
	if (!ArcAbilityTaskPlayCastMontageWithMarks::GetMontageTiming(InstanceData.EndMontage, EndTiming)
		|| !ArcAbilityTaskPlayCastMontageWithMarks::ComputeAdjustedRates(
			EndTiming,
			InstanceData.EndDuration,
			InstanceData.EndInputPlayRate,
			InstanceData.EndEffectivePlayRate))
	{
		return EStateTreeRunStatus::Failed;
	}

	ArcAbilityTaskPlayCastMontageWithMarks::FMontageTiming StartTiming;
	if (InstanceData.bUseStart)
	{
		if (!ArcAbilityTaskPlayCastMontageWithMarks::GetMontageTiming(InstanceData.StartMontage, StartTiming)
			|| FMath::Abs(StartTiming.NaturalDuration - EndTiming.NaturalDuration) > 0.01f
			|| !ArcAbilityTaskPlayCastMontageWithMarks::ComputeAdjustedRates(
				StartTiming,
				InstanceData.StartDuration,
				InstanceData.StartInputPlayRate,
				InstanceData.StartEffectivePlayRate))
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	if (InstanceData.bUseLoop)
	{
		ArcAbilityTaskPlayCastMontageWithMarks::FMontageTiming LoopTiming;
		if (!FMath::IsFinite(InstanceData.LoopInterval) || InstanceData.LoopInterval <= KINDA_SMALL_NUMBER
			|| !ArcAbilityTaskPlayCastMontageWithMarks::GetMontageTiming(InstanceData.LoopMontage, LoopTiming))
		{
			return EStateTreeRunStatus::Failed;
		}
		InstanceData.LoopDuration = LoopTiming.NaturalDuration;
		InstanceData.LoopEffectivePlayRate = LoopTiming.RateScale;
	}

	InstanceData.PhaseElapsedTime = 0.f;
	InstanceData.LoopCycleElapsedTime = 0.f;
	InstanceData.ActiveMontage = nullptr;
	InstanceData.bVisualPositionNeedsSync = false;
	InstanceData.LastFiredTag = FGameplayTag();
	InstanceData.LastFiredPayload.Reset();

	if (InstanceData.bUseStart)
	{
		ArcAbilityTaskPlayCastMontageWithMarks::PrepareMarks(
			InstanceData.StartMarks,
			InstanceData.StartDuration,
			InstanceData.StartEffectivePlayRate,
			InstanceData.bSyncMarksFromMontagesOnSave,
			TEXT("Start"),
			InstanceData.StartMarkRuntime);
	}
	else
	{
		ArcAbilityTaskPlayCastMontageWithMarks::ResetMarks(InstanceData.StartMarkRuntime);
	}

	if (InstanceData.bUseLoop)
	{
		ArcAbilityTaskPlayCastMontageWithMarks::PrepareMarks(
			InstanceData.LoopMarks,
			InstanceData.LoopDuration,
			InstanceData.LoopEffectivePlayRate,
			InstanceData.bSyncMarksFromMontagesOnSave,
			TEXT("Loop"),
			InstanceData.LoopMarkRuntime);
	}
	else
	{
		ArcAbilityTaskPlayCastMontageWithMarks::ResetMarks(InstanceData.LoopMarkRuntime);
	}

	ArcAbilityTaskPlayCastMontageWithMarks::PrepareMarks(
		InstanceData.EndMarks,
		InstanceData.EndDuration,
		InstanceData.EndEffectivePlayRate,
		InstanceData.bSyncMarksFromMontagesOnSave,
		TEXT("End"),
		InstanceData.EndMarkRuntime);

	if (InstanceData.bUseStart)
	{
		InstanceData.Phase = EArcAbilityCastMontagePhase::Start;
		if (!ArcAbilityTaskPlayCastMontageWithMarks::PlayVisual(
			*AnimInstance,
			InstanceData.StartMontage,
			InstanceData.StartInputPlayRate,
			InstanceData))
		{
			return EStateTreeRunStatus::Failed;
		}
		ArcAbilityTaskPlayCastMontageWithMarks::FireMarksAtPhaseStart(
			Context, InstanceData.StartMarks, InstanceData.StartMarkRuntime, InstanceData);
	}
	else
	{
		InstanceData.Phase = EArcAbilityCastMontagePhase::End;
		if (!ArcAbilityTaskPlayCastMontageWithMarks::PlayVisual(
			*AnimInstance,
			InstanceData.EndMontage,
			InstanceData.EndInputPlayRate,
			InstanceData))
		{
			return EStateTreeRunStatus::Failed;
		}
		ArcAbilityTaskPlayCastMontageWithMarks::FireMarksAtPhaseStart(
			Context, InstanceData.EndMarks, InstanceData.EndMarkRuntime, InstanceData);
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PlayCastMontageWithMarks::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayCastMontageWithMarks::ResolveAnimInstance(
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
		case EArcAbilityCastMontagePhase::Start:
		{
			const float Step = FMath::Min(RemainingTime, InstanceData.StartDuration - InstanceData.PhaseElapsedTime);
			const float PreviousTime = InstanceData.PhaseElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayCastMontageWithMarks::FireMarksInRange(
				Context,
				InstanceData.StartMarks,
				InstanceData.StartMarkRuntime,
				PreviousTime,
				InstanceData.PhaseElapsedTime,
				InstanceData);
			if (InstanceData.PhaseElapsedTime < InstanceData.StartDuration)
			{
				ArcAbilityTaskPlayCastMontageWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
				return EStateTreeRunStatus::Running;
			}

			if (!InstanceData.bUseLoop)
			{
				if (!ArcAbilityTaskPlayCastMontageWithMarks::EnterEndPhase(Context, *AnimInstance, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				continue;
			}

			InstanceData.Phase = EArcAbilityCastMontagePhase::Loop;
			InstanceData.PhaseElapsedTime = 0.f;
			InstanceData.LoopCycleElapsedTime = 0.f;
			InstanceData.LoopMarkRuntime.NextIndex = 0;
			if (!ArcAbilityTaskPlayCastMontageWithMarks::PlayVisual(
				*AnimInstance, InstanceData.LoopMontage, 1.f, InstanceData))
			{
				return EStateTreeRunStatus::Failed;
			}
			InstanceData.bVisualPositionNeedsSync = true;
			ArcAbilityTaskPlayCastMontageWithMarks::FireMarksAtPhaseStart(
				Context, InstanceData.LoopMarks, InstanceData.LoopMarkRuntime, InstanceData);
			continue;
		}

		case EArcAbilityCastMontagePhase::Loop:
		{
			const float IntervalRemaining = InstanceData.LoopInterval - InstanceData.PhaseElapsedTime;
			const float CycleRemaining = InstanceData.LoopDuration - InstanceData.LoopCycleElapsedTime;
			const float Step = FMath::Min(RemainingTime, FMath::Min(IntervalRemaining, CycleRemaining));
			const float PreviousCycleTime = InstanceData.LoopCycleElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			InstanceData.LoopCycleElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayCastMontageWithMarks::FireMarksInRange(
				Context,
				InstanceData.LoopMarks,
				InstanceData.LoopMarkRuntime,
				PreviousCycleTime,
				InstanceData.LoopCycleElapsedTime,
				InstanceData);

			if (InstanceData.PhaseElapsedTime >= InstanceData.LoopInterval)
			{
				if (!ArcAbilityTaskPlayCastMontageWithMarks::EnterEndPhase(Context, *AnimInstance, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				continue;
			}

			if (InstanceData.LoopCycleElapsedTime >= InstanceData.LoopDuration)
			{
				InstanceData.LoopCycleElapsedTime = 0.f;
				InstanceData.LoopMarkRuntime.NextIndex = 0;
				if (!ArcAbilityTaskPlayCastMontageWithMarks::PlayVisual(
					*AnimInstance, InstanceData.LoopMontage, 1.f, InstanceData))
				{
					return EStateTreeRunStatus::Failed;
				}
				InstanceData.bVisualPositionNeedsSync = true;
				ArcAbilityTaskPlayCastMontageWithMarks::FireMarksAtPhaseStart(
					Context, InstanceData.LoopMarks, InstanceData.LoopMarkRuntime, InstanceData);
				continue;
			}

			ArcAbilityTaskPlayCastMontageWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
			return EStateTreeRunStatus::Running;
		}

		case EArcAbilityCastMontagePhase::End:
		{
			const float Step = FMath::Min(RemainingTime, InstanceData.EndDuration - InstanceData.PhaseElapsedTime);
			const float PreviousTime = InstanceData.PhaseElapsedTime;
			InstanceData.PhaseElapsedTime += Step;
			RemainingTime -= Step;
			ArcAbilityTaskPlayCastMontageWithMarks::FireMarksInRange(
				Context,
				InstanceData.EndMarks,
				InstanceData.EndMarkRuntime,
				PreviousTime,
				InstanceData.PhaseElapsedTime,
				InstanceData);
			if (InstanceData.PhaseElapsedTime >= InstanceData.EndDuration)
			{
				InstanceData.Phase = EArcAbilityCastMontagePhase::Completed;
				if (!InstanceData.bTerminalDelegateFired)
				{
					InstanceData.bTerminalDelegateFired = true;
					Context.BroadcastDelegate(InstanceData.OnCompleted);
				}
				return EStateTreeRunStatus::Succeeded;
			}

			ArcAbilityTaskPlayCastMontageWithMarks::SyncVisualPositionIfNeeded(*AnimInstance, InstanceData);
			return EStateTreeRunStatus::Running;
		}

		case EArcAbilityCastMontagePhase::Completed:
			return EStateTreeRunStatus::Succeeded;
		}
	}
}

void FArcAbilityTask_PlayCastMontageWithMarks::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	UAnimInstance* AnimInstance = ArcAbilityTaskPlayCastMontageWithMarks::ResolveAnimInstance(
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
