// Copyright Lukasz Baran. All Rights Reserved.

#include "Targeting/ArcTabTargetingState.h"

#include "ArcTargetingPipelineTypes.h"
#include "ArcTargetingSubsystem.h"
#include "AsyncMessageSystemBase.h"
#include "AsyncMessageWorldSubsystem.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "StructUtils/StructView.h"
#include "Targeting/ArcTabTargetingFragment.h"

DEFINE_LOG_CATEGORY(LogArcTabTargeting);

namespace ArcTabTargetingStatePrivate
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}
}

namespace ArcTabTargetingState
{
	bool IsSelectable(
		FMassEntityManager& EntityManager,
		const FMassEntityHandle SourceEntity,
		const FVector& SourceLocation,
		const FMassEntityHandle Target,
		const FArcTabTargetRetentionPolicy& Policy,
		FVector& OutTargetLocation,
		EArcTabTargetChangeReason& OutReason)
	{
		OutTargetLocation = FVector::ZeroVector;
		OutReason = EArcTabTargetChangeReason::InvalidTarget;

		if (!EntityManager.IsEntityValid(Target))
		{
			OutReason = EArcTabTargetChangeReason::TargetDestroyed;
			return false;
		}

		if (Target == SourceEntity)
		{
			return false;
		}

		FMassEntityView TargetView(EntityManager, Target);
		if (Policy.DeathTag.IsValid())
		{
			const FArcOwnedTagsFragment* TagsFragment = TargetView.GetSparseFragmentDataPtr<FArcOwnedTagsFragment>();
			if (TagsFragment != nullptr && TagsFragment->GetTags().HasTag(Policy.DeathTag))
			{
				OutReason = EArcTabTargetChangeReason::TargetDead;
				return false;
			}
		}

		const FTransformFragment* TargetTransform = TargetView.GetFragmentDataPtr<FTransformFragment>();
		if (TargetTransform == nullptr)
		{
			return false;
		}

		const FVector TargetLocation = TargetTransform->GetTransform().GetLocation();
		if (!ArcTabTargetingStatePrivate::IsFiniteVector(SourceLocation)
			|| !ArcTabTargetingStatePrivate::IsFiniteVector(TargetLocation)
			|| !ArcTabTargetingStatePrivate::IsFiniteVector(Policy.TargetLocationOffset)
			|| !FMath::IsFinite(Policy.LoseTargetRange)
			|| Policy.LoseTargetRange <= 0.0f)
		{
			return false;
		}

		if (FVector::DistSquared(SourceLocation, TargetLocation) > FMath::Square(static_cast<double>(Policy.LoseTargetRange)))
		{
			OutReason = EArcTabTargetChangeReason::OutOfRange;
			return false;
		}

		OutTargetLocation = TargetLocation + Policy.TargetLocationOffset;
		return true;
	}

	const FArcTargetCandidate* GetSelectedCandidate(
		const UArcTargetingSubsystem& Subsystem,
		const FMassEntityHandle SourceEntity,
		const FGameplayTag SelectedTargetTag)
	{
		if (!SelectedTargetTag.IsValid())
		{
			return nullptr;
		}

		const TArray<FArcTargetCandidate>* Published = Subsystem.GetLatestPersistentResult(SourceEntity, SelectedTargetTag);
		if (Published == nullptr || Published->IsEmpty())
		{
			return nullptr;
		}

		return &(*Published)[0];
	}

	void PublishSelectedTarget(
		UArcTargetingSubsystem& Subsystem,
		const FMassEntityHandle SourceEntity,
		const FGameplayTag SelectedTargetTag,
		const FArcTargetCandidate& Candidate,
		const FVector& TargetLocation)
	{
		TArray<FArcTargetCandidate> Published;
		FArcTargetCandidate& PublishedCandidate = Published.Add_GetRef(Candidate);
		PublishedCandidate.HitResult.Location = TargetLocation;
		PublishedCandidate.HitResult.ImpactPoint = TargetLocation;
		Subsystem.PublishResult(SourceEntity, SelectedTargetTag, MoveTemp(Published));
	}

	void ResetCycle(FArcTabTargetingFragment& State)
	{
		State.VisitedTargets.Reset();
		State.LastPressTime = 0.0;
	}

	void BroadcastTargetChanged(
		UWorld& World,
		const FMassEntityHandle SourceEntity,
		const FMassEntityHandle PreviousTarget,
		const FMassEntityHandle NewTarget,
		const EArcTabTargetChangeReason Reason)
	{
		FArcTabTargetChangedMessage Message;
		Message.SourceEntity = SourceEntity;
		Message.PreviousTarget = PreviousTarget;
		Message.NewTarget = NewTarget;
		Message.Reason = Reason;

		const TSharedPtr<FAsyncMessageSystemBase> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem(&World);
		if (!MessageSystem.IsValid())
		{
			UE_LOGF(LogArcTabTargeting, Warning, "Unable to publish target change because the world AsyncMessage system is unavailable.");
			return;
		}

		MessageSystem->QueueMessageForBroadcast(
			FAsyncMessageId(Arcx::TargetingMessages::TargetChanged),
			FConstStructView::Make(Message));
	}

	const TCHAR* LexReason(const EArcTabTargetChangeReason Reason)
	{
		switch (Reason)
		{
		case EArcTabTargetChangeReason::Selected:
			return TEXT("Selected");
		case EArcTabTargetChangeReason::Deselected:
			return TEXT("Deselected");
		case EArcTabTargetChangeReason::TargetDestroyed:
			return TEXT("TargetDestroyed");
		case EArcTabTargetChangeReason::TargetDead:
			return TEXT("TargetDead");
		case EArcTabTargetChangeReason::OutOfRange:
			return TEXT("OutOfRange");
		case EArcTabTargetChangeReason::InvalidTarget:
			return TEXT("InvalidTarget");
		}
		return TEXT("Unknown");
	}
}
