// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_PlayMontageWithMarks.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Signals/ArcAbilitySignals.h"
#include "MassActorSubsystem.h"
#include "MassSignalSubsystem.h"
#include "StateTreeAsyncExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

namespace ArcAbilityTaskPlayMontageWithMarks
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
} // namespace ArcAbilityTaskPlayMontageWithMarks

EStateTreeRunStatus FArcAbilityTask_PlayMontageWithMarks::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontageWithMarks::ResolveAnimInstance(EntityManager, Entity);
	if (!AnimInst || !InstanceData.Montage)
	{
		return EStateTreeRunStatus::Failed;
	}
	const float EffectivePlayRate = InstanceData.PlayRate * InstanceData.Montage->RateScale;
	if (InstanceData.PlayRate <= KINDA_SMALL_NUMBER
		|| (InstanceData.bSyncMarksFromMontageOnSave && EffectivePlayRate <= KINDA_SMALL_NUMBER))
	{
		return EStateTreeRunStatus::Failed;
	}

	AnimInst->Montage_Play(InstanceData.Montage, InstanceData.PlayRate);

	if (InstanceData.StartSection != NAME_None)
	{
		AnimInst->Montage_JumpToSection(InstanceData.StartSection, InstanceData.Montage);
	}

	const float Duration = InstanceData.Montage->GetPlayLength()
		/ (InstanceData.bSyncMarksFromMontageOnSave ? EffectivePlayRate : InstanceData.PlayRate);

	// Build sorted parallel arrays from Marks; drop out-of-range entries.
	TArray<TPair<float, int32>> ValidPairs;
	ValidPairs.Reserve(InstanceData.Marks.Num());
	for (int32 Idx = 0; Idx < InstanceData.Marks.Num(); ++Idx)
	{
		const float AuthoredMarkTime = InstanceData.Marks[Idx].Time;
		const float MarkTime = InstanceData.bSyncMarksFromMontageOnSave
			? AuthoredMarkTime / EffectivePlayRate
			: AuthoredMarkTime;
		if (MarkTime >= 0.f && MarkTime < Duration)
		{
			ValidPairs.Add(TPair<float, int32>(MarkTime, Idx));
		}
		else
		{
			UE_LOGF(LogTemp, Warning,"FArcAbilityTask_PlayMontageWithMarks: Mark[%d] time %.3f is out of range [0, %.3f); skipping.", Idx, MarkTime, Duration);
		}
	}

	ValidPairs.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		return A.Key < B.Key;
	});

	InstanceData.SortedMarkTimes.Reset(ValidPairs.Num());
	InstanceData.SortedMarkOriginalIndices.Reset(ValidPairs.Num());
	for (const TPair<float, int32>& Pair : ValidPairs)
	{
		InstanceData.SortedMarkTimes.Add(Pair.Key);
		InstanceData.SortedMarkOriginalIndices.Add(Pair.Value);
	}

	InstanceData.Duration = Duration;
	InstanceData.ElapsedTime = 0.f;
	InstanceData.NextMarkIdx = 0;

	InstanceData.BlendingOutDelegate.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Entity](UAnimMontage* InMontage, bool bInterrupted)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* Data = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (Data)
			{
				StrongContext.BroadcastDelegate(Data->OnBlendingOut);
				
				UWorld* World = StrongContext.GetOwner()->GetWorld();
				UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
				if (SignalSub)
				{
					SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
				}
			}
		});
	AnimInst->Montage_SetBlendingOutDelegate(InstanceData.BlendingOutDelegate, InstanceData.Montage);

	InstanceData.EndedDelegate.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Entity](UAnimMontage* InMontage, bool bInterrupted)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* Data = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (Data)
			{
				StrongContext.BroadcastDelegate(Data->OnEnded);
			}

			StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

			UWorld* World = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
			}
		});
	AnimInst->Montage_SetEndDelegate(InstanceData.EndedDelegate, InstanceData.Montage);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PlayMontageWithMarks::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	// Mark crossing detection.
	//
	// Each authored mark has a sorted time in task-local seconds. On every
	// Tick we ask "which marks did the playhead cross during this frame?" and
	// broadcast each mark's per-entry Dispatcher.
	//
	// The window is (PrevElapsed, CurrentElapsed] — half-open on the left so a
	// mark exactly on a frame boundary fires once, not twice (right edge of
	// frame N == left edge of frame N+1).
	//
	// NextMarkIdx is a monotonic cursor into SortedMarkTimes / OriginalIndices;
	// it walks forward and never revisits a mark in the same cycle. For looping
	// montages the cursor resets to 0 every time the playhead wraps past
	// Duration (see the bLoop branch below).
	const float PrevElapsed = InstanceData.ElapsedTime;
	float CurrentElapsed = PrevElapsed + DeltaTime;
	const int32 MarkCount = InstanceData.SortedMarkTimes.Num();

	const TFunction<void(float, float)> FireRange = [&](float IntervalStart, float IntervalEnd)
	{
		// Broadcast every mark whose time lies in (IntervalStart, IntervalEnd].
		// SortedMarkTimes is ascending and NextMarkIdx never moves backward, so a
		// single forward walk from NextMarkIdx is sufficient.
		while (InstanceData.NextMarkIdx < MarkCount
			&& InstanceData.SortedMarkTimes[InstanceData.NextMarkIdx] >  IntervalStart
			&& InstanceData.SortedMarkTimes[InstanceData.NextMarkIdx] <= IntervalEnd)
		{
			const int32 OriginalIdx = InstanceData.SortedMarkOriginalIndices[InstanceData.NextMarkIdx];
			if (InstanceData.Marks.IsValidIndex(OriginalIdx))
			{
				const FArcMontageMarkEntry& Entry = InstanceData.Marks[OriginalIdx];
				InstanceData.LastFiredTag = Entry.Tag;
				InstanceData.LastFiredPayload = Entry.Payload;
				Context.BroadcastDelegate(Entry.Dispatcher);
			}
			++InstanceData.NextMarkIdx;
		}
	};

	if (InstanceData.bLoop && InstanceData.Duration > 0.f)
	{
		// Looping path. Unlike PlayMontageManual we DO wrap ElapsedTime via Fmod
		// because nothing downstream needs a monotonic accumulator here. Walk
		// the frame's elapsed window in cycle-sized chunks:
		//   1. Fire from CycleStart up to the cycle boundary (Duration).
		//   2. Reset the mark cursor — the next cycle replays the same marks.
		//   3. Subtract Duration from the remaining window and repeat.
		//   4. Fire the final partial window inside the current cycle.
		//
		// The while loop handles the rare case where a huge DeltaTime (hitch,
		// editor scrub) covers multiple full cycles in one frame — every
		// intermediate cycle still fires its marks once.
		float CycleStart = PrevElapsed;
		float CycleEnd = CurrentElapsed;
		while (CycleEnd >= InstanceData.Duration)
		{
			FireRange(CycleStart, InstanceData.Duration);
			CycleEnd -= InstanceData.Duration;
			CycleStart = 0.f;
			InstanceData.NextMarkIdx = 0;
		}
		FireRange(CycleStart, CycleEnd);
		InstanceData.ElapsedTime = FMath::Fmod(CurrentElapsed, InstanceData.Duration);
	}
	else
	{
		// Non-loop path: one contiguous window from the previous tick to now,
		// clamped so a final tick that overshoots Duration still fires marks at
		// the boundary without going past it.
		if (InstanceData.Duration > 0.f && CurrentElapsed > InstanceData.Duration)
		{
			CurrentElapsed = InstanceData.Duration;
		}
		FireRange(PrevElapsed, CurrentElapsed);
		InstanceData.ElapsedTime = CurrentElapsed;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_PlayMontageWithMarks::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontageWithMarks::ResolveAnimInstance(EntityManager, Entity);
	if (AnimInst)
	{
		FOnMontageBlendingOutStarted EmptyBlendingOut;
		FOnMontageEnded EmptyEnded;
		AnimInst->Montage_SetBlendingOutDelegate(EmptyBlendingOut, InstanceData.Montage);
		AnimInst->Montage_SetEndDelegate(EmptyEnded, InstanceData.Montage);

		if (InstanceData.Montage && AnimInst->Montage_IsPlaying(InstanceData.Montage))
		{
			AnimInst->Montage_Stop(0.2f, InstanceData.Montage);
		}
	}

	InstanceData.BlendingOutDelegate.Unbind();
	InstanceData.EndedDelegate.Unbind();
}
