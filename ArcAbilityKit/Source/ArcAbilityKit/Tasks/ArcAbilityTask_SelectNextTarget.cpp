// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_SelectNextTarget.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingSubsystem.h"
#include "Engine/World.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Targeting/ArcTabTargetingFragment.h"
#include "Targeting/ArcTabTargetingState.h"

namespace ArcAbilityTaskSelectNextTargetPrivate
{
	struct FListEntry
	{
		const FArcTargetCandidate* Candidate = nullptr;
		FVector TargetLocation = FVector::ZeroVector;
	};

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	FString Describe(const FMassEntityHandle Entity)
	{
		return Entity.IsValid() ? Entity.DebugGetDescription() : FString(TEXT("None"));
	}

	void ClearOutput(FArcAbilityTask_SelectNextTargetInstanceData& InstanceData)
	{
		InstanceData.SelectedTarget = FMassEntityHandle();
		InstanceData.TargetLocation = FVector::ZeroVector;
		InstanceData.bHasTarget = false;
	}

	void FillOutput(
		FArcAbilityTask_SelectNextTargetInstanceData& InstanceData,
		const FMassEntityHandle Target,
		const FVector& TargetLocation)
	{
		InstanceData.SelectedTarget = Target;
		InstanceData.TargetLocation = TargetLocation;
		InstanceData.bHasTarget = true;
	}

	void FillOutputFromCurrent(
		FArcAbilityTask_SelectNextTargetInstanceData& InstanceData,
		FMassEntityManager& EntityManager,
		const FMassEntityHandle SourceEntity,
		const FVector& SourceLocation,
		const FMassEntityHandle CurrentTarget,
		const FArcTabTargetRetentionPolicy& Policy)
	{
		if (!CurrentTarget.IsValid())
		{
			return;
		}

		FVector TargetLocation = FVector::ZeroVector;
		EArcTabTargetChangeReason Reason = EArcTabTargetChangeReason::InvalidTarget;
		if (ArcTabTargetingState::IsSelectable(EntityManager, SourceEntity, SourceLocation, CurrentTarget, Policy, TargetLocation, Reason))
		{
			FillOutput(InstanceData, CurrentTarget, TargetLocation);
		}
	}

	EStateTreeRunStatus Fail(const FMassEntityHandle SourceEntity, const TCHAR* Reason)
	{
		UE_LOGF(LogArcTabTargeting, Log, "Select Next Target on %ls: Failed (%ls).", *Describe(SourceEntity), Reason);
		return EStateTreeRunStatus::Failed;
	}

	FArcTabTargetRetentionPolicy MakeRetentionPolicy(const FArcAbilityTask_SelectNextTargetInstanceData& InstanceData)
	{
		FArcTabTargetRetentionPolicy Policy;
		Policy.LoseTargetRange = InstanceData.LoseTargetRange;
		Policy.DeathTag = InstanceData.DeathTag;
		Policy.TargetLocationOffset = InstanceData.TargetLocationOffset;
		return Policy;
	}

	int32 FindInList(const TArray<FListEntry>& List, const FMassEntityHandle Entity)
	{
		return List.IndexOfByPredicate([Entity](const FListEntry& Entry) -> bool
		{
			return Entry.Candidate->Entity == Entity;
		});
	}
}

EStateTreeRunStatus FArcAbilityTask_SelectNextTarget::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	using namespace ArcAbilityTaskSelectNextTargetPrivate;

	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	ClearOutput(InstanceData);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FMassEntityHandle SourceEntity = AbilityContext.GetEntity();

	if (!InstanceData.CandidatesTag.IsValid())
	{
		return Fail(SourceEntity, TEXT("CandidatesTag is empty"));
	}
	if (!InstanceData.PotentialTargetTag.IsValid())
	{
		return Fail(SourceEntity, TEXT("PotentialTargetTag is empty"));
	}
	if (!InstanceData.SelectedTargetTag.IsValid())
	{
		return Fail(SourceEntity, TEXT("SelectedTargetTag is empty"));
	}
	if (!FMath::IsFinite(InstanceData.LoseTargetRange) || InstanceData.LoseTargetRange <= 0.0f)
	{
		return Fail(SourceEntity, TEXT("LoseTargetRange must be positive"));
	}
	if (!FMath::IsFinite(InstanceData.CycleResetDelay) || InstanceData.CycleResetDelay < 0.0f)
	{
		return Fail(SourceEntity, TEXT("CycleResetDelay must not be negative"));
	}
	if (!IsFiniteVector(InstanceData.TargetLocationOffset))
	{
		return Fail(SourceEntity, TEXT("TargetLocationOffset is not finite"));
	}

	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	if (World == nullptr)
	{
		return Fail(SourceEntity, TEXT("no world"));
	}
	if (!EntityManager.IsEntityValid(SourceEntity))
	{
		return Fail(SourceEntity, TEXT("source entity is not valid"));
	}

	FMassEntityView SourceView(EntityManager, SourceEntity);
	FArcTabTargetingFragment* State = SourceView.GetSparseFragmentDataPtr<FArcTabTargetingFragment>();
	if (State == nullptr)
	{
		return Fail(SourceEntity, TEXT("no FArcTabTargetingFragment"));
	}

	const FTransformFragment* SourceTransform = SourceView.GetFragmentDataPtr<FTransformFragment>();
	if (SourceTransform == nullptr)
	{
		return Fail(SourceEntity, TEXT("no FTransformFragment"));
	}

	UArcTargetingSubsystem* Subsystem = UArcTargetingSubsystem::Get(World);
	if (Subsystem == nullptr)
	{
		return Fail(SourceEntity, TEXT("no UArcTargetingSubsystem"));
	}

	const FArcTabTargetRetentionPolicy Policy = MakeRetentionPolicy(InstanceData);
	const FVector SourceLocation = SourceTransform->GetTransform().GetLocation();
	const FArcTargetCandidate* CurrentCandidate = ArcTabTargetingState::GetSelectedCandidate(*Subsystem, SourceEntity, InstanceData.SelectedTargetTag);
	const FMassEntityHandle CurrentTarget = CurrentCandidate != nullptr ? CurrentCandidate->Entity : FMassEntityHandle();

	if (Transition.ChangeType == EStateTreeStateChangeType::Sustained)
	{
		FillOutputFromCurrent(InstanceData, EntityManager, SourceEntity, SourceLocation, CurrentTarget, Policy);
		return EStateTreeRunStatus::Succeeded;
	}

	const double CurrentTime = static_cast<double>(World->GetTimeSeconds());
	const double SincePress = CurrentTime - State->LastPressTime;
	const bool bCycleExpired = !State->VisitedTargets.IsEmpty()
		&& (SincePress < 0.0 || SincePress >= static_cast<double>(InstanceData.CycleResetDelay));
	if (bCycleExpired)
	{
		State->VisitedTargets.Reset();
	}
	State->LastPressTime = CurrentTime;

	const TArray<FArcTargetCandidate>* CandidateResult = Subsystem->GetLatestPersistentResult(SourceEntity, InstanceData.CandidatesTag);
	TArray<FListEntry> List;
	if (CandidateResult != nullptr)
	{
		List.Reserve(CandidateResult->Num());
		for (const FArcTargetCandidate& Candidate : *CandidateResult)
		{
			if (!Candidate.Entity.IsValid())
			{
				continue;
			}

			if (FindInList(List, Candidate.Entity) != INDEX_NONE)
			{
				UE_LOGF(LogArcTabTargeting, Verbose, "  candidate %ls skipped: duplicate.", *Describe(Candidate.Entity));
				continue;
			}

			FVector TargetLocation = FVector::ZeroVector;
			EArcTabTargetChangeReason Reason = EArcTabTargetChangeReason::InvalidTarget;
			if (!ArcTabTargetingState::IsSelectable(EntityManager, SourceEntity, SourceLocation, Candidate.Entity, Policy, TargetLocation, Reason))
			{
				UE_LOGF(LogArcTabTargeting, Verbose, "  candidate %ls skipped: not selectable (%ls).", *Describe(Candidate.Entity), ArcTabTargetingState::LexReason(Reason));
				continue;
			}

			UE_LOGF(LogArcTabTargeting, Verbose, "  candidate [%d] %ls score %.3f.", List.Num(), *Describe(Candidate.Entity), Candidate.Score);
			FListEntry& Entry = List.AddDefaulted_GetRef();
			Entry.Candidate = &Candidate;
			Entry.TargetLocation = TargetLocation;
		}
	}

	if (List.IsEmpty())
	{
		FillOutputFromCurrent(InstanceData, EntityManager, SourceEntity, SourceLocation, CurrentTarget, Policy);
		UE_LOGF(LogArcTabTargeting, Log, "Select Next Target on %ls: row 7 (%ls), target unchanged %ls, candidates 0, visited %d%ls.",
			*Describe(SourceEntity),
			CandidateResult == nullptr ? TEXT("list not computed yet") : TEXT("empty list"),
			*Describe(CurrentTarget),
			State->VisitedTargets.Num(),
			bCycleExpired ? TEXT(", cycle reset by delay") : TEXT(""));
		return EStateTreeRunStatus::Succeeded;
	}

	FMassEntityHandle PotentialTarget;
	const TArray<FArcTargetCandidate>* PotentialResult = Subsystem->GetLatestPersistentResult(SourceEntity, InstanceData.PotentialTargetTag);
	if (PotentialResult != nullptr)
	{
		for (const FArcTargetCandidate& Candidate : *PotentialResult)
		{
			if (Candidate.Entity.IsValid())
			{
				PotentialTarget = Candidate.Entity;
				break;
			}
		}
	}

	const TCHAR* PotentialSkipReason = nullptr;
	int32 PotentialIndex = INDEX_NONE;
	if (!PotentialTarget.IsValid())
	{
		PotentialSkipReason = TEXT("none");
	}
	else
	{
		FVector PotentialLocation = FVector::ZeroVector;
		EArcTabTargetChangeReason PotentialReason = EArcTabTargetChangeReason::InvalidTarget;
		PotentialIndex = FindInList(List, PotentialTarget);
		if (!ArcTabTargetingState::IsSelectable(EntityManager, SourceEntity, SourceLocation, PotentialTarget, Policy, PotentialLocation, PotentialReason))
		{
			PotentialSkipReason = TEXT("not selectable");
		}
		else if (PotentialIndex == INDEX_NONE)
		{
			PotentialSkipReason = TEXT("not on candidate list");
		}
		else if (PotentialTarget == CurrentTarget)
		{
			PotentialSkipReason = TEXT("is current target");
		}
		else if (State->VisitedTargets.Contains(PotentialTarget))
		{
			PotentialSkipReason = TEXT("already visited");
		}
	}

	if (PotentialSkipReason != nullptr)
	{
		UE_LOGF(LogArcTabTargeting, Verbose, "  potential %ls skipped: %ls.", *Describe(PotentialTarget), PotentialSkipReason);
	}

	int32 ChosenIndex = INDEX_NONE;
	const TCHAR* Row = nullptr;
	if (PotentialSkipReason == nullptr)
	{
		ChosenIndex = PotentialIndex;
		State->VisitedTargets.Reset();
		State->VisitedTargets.Add(PotentialTarget);
		Row = TEXT("row 1 (potential target)");
	}
	else
	{
		ChosenIndex = List.IndexOfByPredicate([State, CurrentTarget](const FListEntry& Entry) -> bool
		{
			return Entry.Candidate->Entity != CurrentTarget && !State->VisitedTargets.Contains(Entry.Candidate->Entity);
		});

		if (ChosenIndex != INDEX_NONE)
		{
			State->VisitedTargets.AddUnique(List[ChosenIndex].Candidate->Entity);
			Row = TEXT("row 4 (cycle)");
		}
		else if (CurrentTarget.IsValid())
		{
			Subsystem->ClearPublishedResult(SourceEntity, State->SelectedTargetTag);
			if (InstanceData.SelectedTargetTag.IsValid() && InstanceData.SelectedTargetTag != State->SelectedTargetTag)
			{
				Subsystem->ClearPublishedResult(SourceEntity, InstanceData.SelectedTargetTag);
			}

			ArcTabTargetingState::ResetCycle(*State);
			State->LastPressTime = CurrentTime;
			ClearOutput(InstanceData);

			UE_LOGF(LogArcTabTargeting, Log, "Select Next Target on %ls: row 5 (all candidates visited, deselect), previous %ls, candidates %d%ls.",
				*Describe(SourceEntity),
				*Describe(CurrentTarget),
				List.Num(),
				bCycleExpired ? TEXT(", cycle reset by delay") : TEXT(""));

			ArcTabTargetingState::BroadcastTargetChanged(*World, SourceEntity, CurrentTarget, FMassEntityHandle(), EArcTabTargetChangeReason::Deselected);
			return EStateTreeRunStatus::Succeeded;
		}
		else
		{
			ChosenIndex = 0;
			State->VisitedTargets.Reset();
			State->VisitedTargets.Add(List[ChosenIndex].Candidate->Entity);
			Row = TEXT("row 6 (restart cycle)");
		}
	}

	const FArcTargetCandidate Chosen = *List[ChosenIndex].Candidate;
	const FVector ChosenLocation = List[ChosenIndex].TargetLocation;

	if (State->SelectedTargetTag.IsValid() && State->SelectedTargetTag != InstanceData.SelectedTargetTag)
	{
		Subsystem->ClearPublishedResult(SourceEntity, State->SelectedTargetTag);
	}

	ArcTabTargetingState::PublishSelectedTarget(*Subsystem, SourceEntity, InstanceData.SelectedTargetTag, Chosen, ChosenLocation);
	State->SelectedTargetTag = InstanceData.SelectedTargetTag;
	State->RetentionPolicy = Policy;
	FillOutput(InstanceData, Chosen.Entity, ChosenLocation);

	UE_LOGF(LogArcTabTargeting, Log, "Select Next Target on %ls: %ls, selected %ls, previous %ls, candidates %d, visited %d%ls.",
		*Describe(SourceEntity),
		Row,
		*Describe(Chosen.Entity),
		*Describe(CurrentTarget),
		List.Num(),
		State->VisitedTargets.Num(),
		bCycleExpired ? TEXT(", cycle reset by delay") : TEXT(""));

	if (Chosen.Entity != CurrentTarget)
	{
		ArcTabTargetingState::BroadcastTargetChanged(*World, SourceEntity, CurrentTarget, Chosen.Entity, EArcTabTargetChangeReason::Selected);
	}

	return EStateTreeRunStatus::Succeeded;
}
