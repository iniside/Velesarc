// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_PlayMontageManual.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Animation/ArcMontageMarkExtractor.h"
#include "Tasks/ArcMontageMarkEntry.h"
#include "MassActorSubsystem.h"
#include "MassSignalSubsystem.h"
#include "StateTreeAsyncExecutionContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Signals/ArcAbilitySignals.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogArcMontageManual, Log, All);

namespace ArcAbilityTaskPlayMontageManual
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
} // namespace ArcAbilityTaskPlayMontageManual

EStateTreeRunStatus FArcAbilityTask_PlayMontageManual::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontageManual::ResolveAnimInstance(EntityManager, Entity);
	if (!AnimInst || !InstanceData.Montage)
	{
		UE_LOGF(LogArcMontageManual, Warning,"[MontageManual] EnterState FAILED entity=[%d:%d] AnimInst=%ls Montage=%ls",
			Entity.Index, Entity.SerialNumber,
			AnimInst ? TEXT("valid") : TEXT("NULL"),
			InstanceData.Montage ? *InstanceData.Montage->GetName() : TEXT("NULL"));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime = 0.f;
	InstanceData.bLocalCompletionFired = false;

	const float MontageLength = InstanceData.Montage->GetPlayLength();
	const float ClampedDuration = FMath::Max(InstanceData.Duration, KINDA_SMALL_NUMBER);

	float EffectivePlayRate = 1.f;
	if (InstanceData.EndMode == EArcManualMontageEndMode::FixedDuration)
	{
		EffectivePlayRate = MontageLength / ClampedDuration;
	}
	else
	{
		EffectivePlayRate = FMath::Max(InstanceData.PlayRate, KINDA_SMALL_NUMBER);
	}

	{
		TArray<FArcMontageMarkEntry> RawMarks;
		ArcMontageMarks::ExtractMarks(InstanceData.Montage, RawMarks);

		TArray<TPair<float, FGameplayTag>> Scaled;
		Scaled.Reserve(RawMarks.Num());
		for (const FArcMontageMarkEntry& Entry : RawMarks)
		{
			Scaled.Emplace(Entry.Time / EffectivePlayRate, Entry.Tag);
		}
		Scaled.Sort([](const TPair<float, FGameplayTag>& A, const TPair<float, FGameplayTag>& B)
		{
			return A.Key < B.Key;
		});

		InstanceData.SortedNotifyTimes.Reset(Scaled.Num());
		InstanceData.SortedNotifyTags.Reset(Scaled.Num());
		for (const TPair<float, FGameplayTag>& P : Scaled)
		{
			InstanceData.SortedNotifyTimes.Add(P.Key);
			InstanceData.SortedNotifyTags.Add(P.Value);
		}
		InstanceData.NextNotifyIdx = 0;
		InstanceData.CycleLength = MontageLength / EffectivePlayRate;
	}

	AnimInst->Montage_Play(InstanceData.Montage, EffectivePlayRate);

	if (InstanceData.StartSection != NAME_None)
	{
		AnimInst->Montage_JumpToSection(InstanceData.StartSection, InstanceData.Montage);
	}

	FAnimMontageInstance* MontageInst = AnimInst->GetActiveInstanceForMontage(InstanceData.Montage);
	if (MontageInst)
	{
		MontageInst->bEnableAutoBlendOut = false;
	}

	UE_LOGF(LogArcMontageManual, Log,"[MontageManual] EnterState entity=[%d:%d] montage=%ls EndMode=%ls bLoop=%d MontageLength=%.3f Duration=%.3f PlayRate=%.3f EffectivePlayRate=%.3f StartSection=%ls MontageInst=%ls",
		Entity.Index, Entity.SerialNumber,
		*InstanceData.Montage->GetName(),
		(InstanceData.EndMode == EArcManualMontageEndMode::NaturalLength) ? TEXT("NaturalLength") : TEXT("FixedDuration"),
		InstanceData.bLoop ? 1 : 0,
		MontageLength, InstanceData.Duration, InstanceData.PlayRate, EffectivePlayRate,
		*InstanceData.StartSection.ToString(),
		MontageInst ? TEXT("found") : TEXT("NULL"));

	float CapturedEndTime = 0.f;
	if (InstanceData.bLoop)
	{
		CapturedEndTime = TNumericLimits<float>::Max();
	}
	else if (InstanceData.EndMode == EArcManualMontageEndMode::FixedDuration)
	{
		CapturedEndTime = ClampedDuration;
	}
	else
	{
		CapturedEndTime = MontageLength / EffectivePlayRate;
	}

	InstanceData.EndTime = CapturedEndTime;

	UE_LOGF(LogArcMontageManual, Log,"[MontageManual] EnterState entity=[%d:%d] EndTime=%.3f -> returning Running",
		Entity.Index, Entity.SerialNumber, InstanceData.EndTime);

	InstanceData.EndedDelegate.BindLambda(
		[WeakContext = Context.MakeWeakExecutionContext(), Entity](UAnimMontage* InMontage, bool bInterrupted)
		{
			UE_LOGF(LogArcMontageManual, Log,"[MontageManual] EndedDelegate FIRED entity=[%d:%d] montage=%ls bInterrupted=%d",
				Entity.Index, Entity.SerialNumber,
				InMontage ? *InMontage->GetName() : TEXT("NULL"), bInterrupted ? 1 : 0);

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* Data = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (Data && !Data->bLocalCompletionFired)
			{
				Data->bLocalCompletionFired = true;
				StrongContext.BroadcastDelegate(Data->OnInterrupted);
				StrongContext.FinishTask(EStateTreeFinishTaskType::Failed);
				UWorld* World = StrongContext.GetOwner()->GetWorld();
				UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
				if (SignalSub)
				{
					SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
				}
			}
		});
	AnimInst->Montage_SetEndDelegate(InstanceData.EndedDelegate, InstanceData.Montage);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PlayMontageManual::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	InstanceData.ElapsedTime += DeltaTime;

	{
		FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
		const FMassEntityHandle TickEntity = AbilityContext.GetEntity();
		UE_LOGF(LogArcMontageManual, Log,"[MontageManual] Tick entity=[%d:%d] DeltaTime=%.4f ElapsedTime=%.3f EndTime=%.3f bLoop=%d",
			TickEntity.Index, TickEntity.SerialNumber,
			DeltaTime, InstanceData.ElapsedTime, InstanceData.EndTime, InstanceData.bLoop ? 1 : 0);
	}

	// Notify crossing detection.
	//
	// Each notify has a sorted trigger time in task-local seconds (already scaled
	// by EffectivePlayRate at EnterState). On every Tick, we ask "which notifies
	// did the playhead cross during this frame?" and broadcast OnNotify for each.
	//
	// The window we inspect is (PrevElapsed, CurrentElapsed] — half-open on the
	// left so that a notify exactly on a frame boundary fires once, not twice
	// (the right edge of frame N == the left edge of frame N+1).
	//
	// NextNotifyIdx is a monotonic cursor into SortedNotifyTimes; it walks
	// forward and never revisits a notify in the same cycle. For looping
	// montages the cursor resets to 0 every time the playhead wraps past
	// CycleLength (see the bLoop branch below).
	const float PrevElapsed = InstanceData.ElapsedTime - DeltaTime;
	const float CurrentElapsed = InstanceData.ElapsedTime;
	const int32 NotifyCount = InstanceData.SortedNotifyTimes.Num();

	const TFunction<void(float, float)> FireRange = [&](float IntervalStart, float IntervalEnd)
	{
		// Broadcast every notify whose trigger time lies in (IntervalStart, IntervalEnd].
		// SortedNotifyTimes is ascending and NextNotifyIdx never moves backward, so a
		// single forward walk from NextNotifyIdx is sufficient.
		while (InstanceData.NextNotifyIdx < NotifyCount
			&& InstanceData.SortedNotifyTimes[InstanceData.NextNotifyIdx] >  IntervalStart
			&& InstanceData.SortedNotifyTimes[InstanceData.NextNotifyIdx] <= IntervalEnd)
		{
			InstanceData.LastFiredTag = InstanceData.SortedNotifyTags[InstanceData.NextNotifyIdx];
			Context.BroadcastDelegate(InstanceData.OnNotify);
			++InstanceData.NextNotifyIdx;
		}
	};

	if (InstanceData.bLoop && InstanceData.CycleLength > 0.f)
	{
		// Looping path. ElapsedTime keeps increasing past CycleLength (it stays
		// monotonic so non-loop completion math elsewhere still works), but the
		// playhead inside the montage wraps every CycleLength seconds. So we walk
		// the frame's elapsed window in cycle-sized chunks:
		//   1. Fire from CycleStart up to the cycle boundary (CycleLength).
		//   2. Reset the notify cursor — the next cycle replays the same notifies.
		//   3. Subtract CycleLength from the remaining window and repeat.
		//   4. Fire the final partial window inside the current cycle.
		//
		// The while loop handles the rare case where a huge DeltaTime (hitch,
		// editor scrub) covers multiple full cycles in one frame — every
		// intermediate cycle still fires its notifies once.
		float CycleStart = PrevElapsed;
		float CycleEnd = CurrentElapsed;
		while (CycleEnd >= InstanceData.CycleLength)
		{
			FireRange(CycleStart, InstanceData.CycleLength);
			CycleEnd -= InstanceData.CycleLength;
			CycleStart = 0.f;
			InstanceData.NextNotifyIdx = 0;
		}
		FireRange(CycleStart, CycleEnd);
		// Note: InstanceData.ElapsedTime is deliberately left monotonic — only
		// the local CycleStart/CycleEnd view wraps. The bLoop branch in the
		// completion check below already short-circuits on InstanceData.bLoop.
	}
	else
	{
		// Non-loop path: one contiguous window from the previous tick to now.
		FireRange(PrevElapsed, CurrentElapsed);
	}

	if (!InstanceData.bLoop && InstanceData.ElapsedTime >= InstanceData.EndTime)
	{
		if (!InstanceData.bLocalCompletionFired)
		{
			InstanceData.bLocalCompletionFired = true;
			Context.BroadcastDelegate(InstanceData.OnCompleted);
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_PlayMontageManual::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	UE_LOGF(LogArcMontageManual, Log,"[MontageManual] ExitState entity=[%d:%d] ElapsedTime=%.3f EndTime=%.3f bLocalCompletionFired=%d",
		Entity.Index, Entity.SerialNumber,
		InstanceData.ElapsedTime, InstanceData.EndTime, InstanceData.bLocalCompletionFired ? 1 : 0);

	UAnimInstance* AnimInst = ArcAbilityTaskPlayMontageManual::ResolveAnimInstance(EntityManager, Entity);
	if (AnimInst)
	{
		FOnMontageEnded EmptyEnded;
		AnimInst->Montage_SetEndDelegate(EmptyEnded, InstanceData.Montage);

		if (InstanceData.Montage)
		{
			AnimInst->Montage_Stop(InstanceData.BlendOutTime, InstanceData.Montage);
		}
	}

	InstanceData.EndedDelegate.Unbind();

	if (!InstanceData.bLocalCompletionFired)
	{
		InstanceData.bLocalCompletionFired = true;
		Context.BroadcastDelegate(InstanceData.OnInterrupted);
	}
}

#if WITH_EDITOR
EDataValidationResult FArcAbilityTask_PlayMontageManual::Compile(UE::StateTree::ICompileNodeContext& CompileContext)
{
	// Skip validation when Montage is property-bound — the value is supplied at runtime.
	const bool bMontageBound = CompileContext.HasBindingForProperty(GET_MEMBER_NAME_CHECKED(FInstanceDataType, Montage));
	if (bMontageBound)
	{
		return EDataValidationResult::NotValidated;
	}

	const FInstanceDataType& InstanceData = CompileContext.GetInstanceDataView().Get<FInstanceDataType>();
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!InstanceData.Montage)
	{
		CompileContext.AddValidationError(NSLOCTEXT("ArcAbilityTask", "PlayMontageManual_MissingMontage",
			"PlayMontageManual: Montage asset is not set."));
		Result = EDataValidationResult::Invalid;
	}
	else if (InstanceData.Montage->bEnableAutoBlendOut)
	{
		// Manual montage tasks drive blend-out via Montage_Stop in ExitState; the
		// montage's own auto-blend-out races that path and can fire OnMontageEnded
		// before ExitState's cleanup, double-firing OnInterrupted. Author the asset
		// with bEnableAutoBlendOut = false.
		CompileContext.AddValidationError(FText::Format(
			NSLOCTEXT("ArcAbilityTask", "PlayMontageManual_AutoBlendOutEnabled",
				"PlayMontageManual: Montage '{0}' has 'Enable Auto Blend Out' set; disable it on the montage asset."),
			FText::FromString(InstanceData.Montage->GetName())));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
